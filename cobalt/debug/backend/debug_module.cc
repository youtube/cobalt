// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/debug/backend/debug_module.h"

#include "cobalt/debug/backend/render_layer.h"

namespace cobalt {
namespace debug {
namespace backend {

DebugModule::DebugModule(dom::Console* console,
                         script::GlobalEnvironment* global_environment,
                         RenderOverlay* render_overlay,
                         render_tree::ResourceProvider* resource_provider,
                         dom::Window* window) {
  ConstructionData data(console, global_environment, MessageLoop::current(),
                        render_overlay, resource_provider, window);
  Build(data);
}

DebugModule::DebugModule(dom::Console* console,
                         script::GlobalEnvironment* global_environment,
                         RenderOverlay* render_overlay,
                         render_tree::ResourceProvider* resource_provider,
                         dom::Window* window, MessageLoop* message_loop) {
  ConstructionData data(console, global_environment, message_loop,
                        render_overlay, resource_provider, window);
  Build(data);
}

DebugModule::~DebugModule() {
  if (script_debugger_) {
    script_debugger_->Detach();
  }
  if (debug_backend_) {
    debug_backend_->UnbindAgents();
  }
}

void DebugModule::Build(const ConstructionData& data) {
  DCHECK(data.message_loop);

  if (MessageLoop::current() == data.message_loop) {
    BuildInternal(data, NULL);
  } else {
    base::WaitableEvent created(true, false);
    data.message_loop->PostTask(
        FROM_HERE,
        base::Bind(&DebugModule::BuildInternal, base::Unretained(this), data,
                   base::Unretained(&created)));
    created.Wait();
  }

  DCHECK(debug_dispatcher_);
}

void DebugModule::BuildInternal(const ConstructionData& data,
                                base::WaitableEvent* created) {
  DCHECK(MessageLoop::current() == data.message_loop);
  DCHECK(data.console);
  DCHECK(data.global_environment);
  DCHECK(data.render_overlay);
  DCHECK(data.resource_provider);
  DCHECK(data.window);

  // Create the backend objects supporting the debugger agents.
  script_debugger_ =
      script::ScriptDebugger::CreateDebugger(data.global_environment, this);
  script_runner_.reset(
      new DebugScriptRunner(data.global_environment, script_debugger_.get(),
                            data.window->document()->csp_delegate()));
  debug_dispatcher_.reset(
      new DebugDispatcher(script_debugger_.get(), script_runner_.get()));
  debug_backend_ = make_scoped_refptr(new DebugBackend(
      data.global_environment, script_debugger_.get(),
      base::Bind(&DebugModule::SendEvent, base::Unretained(this))));

  // Create render layers for the agents that need them and chain them
  // together. Ownership will be passed to the agent that uses each layer.
  // The layers will be painted in the reverse order they are listed here.
  scoped_ptr<RenderLayer> page_render_layer(new RenderLayer(base::Bind(
      &RenderOverlay::SetOverlay, base::Unretained(data.render_overlay))));

  scoped_ptr<RenderLayer> dom_render_layer(new RenderLayer(
      base::Bind(&RenderLayer::SetBackLayer, page_render_layer->AsWeakPtr())));

  // Create the agents that implement the various devtools protocol domains by
  // handling commands and sending event notifications. The script debugger
  // agent is an adapter to the engine-specific script debugger, which may
  // directly handle one or more protocol domains.
  script_debugger_agent_.reset(
      new ScriptDebuggerAgent(debug_dispatcher_.get(), script_debugger_.get()));
  console_agent_.reset(new ConsoleAgent(debug_dispatcher_.get(), data.console));
  log_agent_.reset(new LogAgent(debug_dispatcher_.get()));
  dom_agent_.reset(
      new DOMAgent(debug_dispatcher_.get(), dom_render_layer.Pass()));
  css_agent_ = make_scoped_refptr(new CSSAgent(debug_dispatcher_.get()));
  page_agent_.reset(new PageAgent(debug_dispatcher_.get(), data.window,
                                  page_render_layer.Pass(),
                                  data.resource_provider));
  tracing_agent_.reset(
      new TracingAgent(debug_dispatcher_.get(), script_debugger_.get()));

  // Hook up the backend objects after the agents have been created.
  debug_backend_->BindAgents(css_agent_);
  script_debugger_->Attach();

  if (created) {
    created->Signal();
  }
}

void DebugModule::SendEvent(const std::string& method,
                            const base::optional<std::string>& params) {
  DCHECK(debug_dispatcher_);
  debug_dispatcher_->SendEvent(method, params);
}

void DebugModule::OnScriptDebuggerPause() {
  DCHECK(debug_dispatcher_);
  debug_dispatcher_->SetPaused(true);
}

void DebugModule::OnScriptDebuggerResume() {
  DCHECK(debug_dispatcher_);
  debug_dispatcher_->SetPaused(false);
}

void DebugModule::OnScriptDebuggerResponse(const std::string& response) {
  DCHECK(script_debugger_agent_);
  script_debugger_agent_->SendCommandResponse(response);
}

void DebugModule::OnScriptDebuggerEvent(const std::string& event) {
  DCHECK(script_debugger_agent_);
  script_debugger_agent_->SendEvent(event);
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
