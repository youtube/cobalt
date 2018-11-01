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

  // Create the script debugger. This is owned by this object, and is
  // accessible to all the debugger components.
  script_debugger_ =
      script::ScriptDebugger::CreateDebugger(data.global_environment, this);

  // Create the debug dispatcher itself.
  debug_dispatcher_.reset(new DebugDispatcher(
      data.global_environment, data.window->document()->csp_delegate(),
      script_debugger_.get()));

  // Create render layers for the components that need them and chain them
  // together. Ownership will be passed to the component that uses each layer.
  // The layers will be painted in the reverse order they are listed here.
  scoped_ptr<RenderLayer> page_render_layer(new RenderLayer(base::Bind(
      &RenderOverlay::SetOverlay, base::Unretained(data.render_overlay))));

  scoped_ptr<RenderLayer> dom_render_layer(new RenderLayer(
      base::Bind(&RenderLayer::SetBackLayer, page_render_layer->AsWeakPtr())));

  // Create the various components that implement the functionality of the
  // debugger by handling commands and sending event notifications. The script
  // debugger component is an adapter to the engine-specific script debugger,
  // which may directly handle one or more protocol domains, so we won't
  // instantiate our own component(s) to handle the same domain(s).
  script_debugger_component_.reset(new ScriptDebuggerComponent(
      debug_dispatcher_.get(), script_debugger_.get()));

  if (!script_debugger_component_->IsSupportedDomain("Runtime")) {
    runtime_component_.reset(new RuntimeComponent(debug_dispatcher_.get()));
  }

  console_component_.reset(
      new ConsoleComponent(debug_dispatcher_.get(), data.console));

  log_component_.reset(new LogComponent(debug_dispatcher_.get()));

  dom_component_.reset(
      new DOMComponent(debug_dispatcher_.get(), dom_render_layer.Pass()));

  page_component_.reset(new PageComponent(debug_dispatcher_.get(), data.window,
                                          page_render_layer.Pass(),
                                          data.resource_provider));

  script_debugger_->Attach();

  if (created) {
    created->Signal();
  }
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
  DCHECK(script_debugger_component_);
  script_debugger_component_->SendCommandResponse(response);
}

void DebugModule::OnScriptDebuggerEvent(const std::string& event) {
  DCHECK(script_debugger_component_);
  script_debugger_component_->SendEvent(event);
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
