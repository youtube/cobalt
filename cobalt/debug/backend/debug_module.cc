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

#include <memory>

#include "cobalt/debug/backend/render_layer.h"

namespace cobalt {
namespace debug {
namespace backend {

namespace {
constexpr char kCobaltAgent[] = "CobaltAgent";
constexpr char kScriptDebuggerAgent[] = "ScriptDebuggerAgent";
constexpr char kLogAgent[] = "LogAgent";
constexpr char kDomAgent[] = "DomAgent";
constexpr char kCssAgent[] = "CssAgent";
constexpr char kOverlayAgent[] = "OverlayAgent";
constexpr char kPageAgent[] = "PageAgent";
constexpr char kTracingAgent[] = "TracingAgent";

// Move the state for a particular agent out of the dictionary holding the
// state for all agents. Returns a NULL JSONObject if either |agents_state| is
// NULL or it doesn't hold a state for the agent.
JSONObject RemoveAgentState(const std::string& agent_name,
                            base::Value::Dict* state_dict) {
#ifndef USE_HACKY_COBALT_CHANGES
  if (state_dict == nullptr) {
    return JSONObject();
  }

  std::unique_ptr<base::Value> value;
  if (!state_dict->Remove(agent_name, &value)) {
    return JSONObject();
  }

  std::unique_ptr<base::DictionaryValue> dictionary_value =
      base::DictionaryValue::From(std::move(value));
  if (!dictionary_value) {
    DLOG(ERROR) << "Unexpected state type for " << agent_name;
    return JSONObject();
  }

  return dictionary_value;
#else
  return nullptr;
#endif
}

void StoreAgentState(base::Value::Dict* state_dict,
                     const std::string& agent_name, JSONObject agent_state) {
#ifndef USE_HACKY_COBALT_CHANGES
  if (agent_state) {
    state_dict->Set(agent_name,
                    std::unique_ptr<base::Value>(agent_state.release()));
  }
#endif
}

}  // namespace

DebugModule::DebugModule(DebuggerHooksImpl* debugger_hooks,
                         script::GlobalEnvironment* global_environment,
                         RenderOverlay* render_overlay,
                         render_tree::ResourceProvider* resource_provider,
                         dom::Window* window, DebuggerState* debugger_state) {
  ConstructionData data(debugger_hooks, global_environment,
                        base::MessageLoop::current(), render_overlay,
                        resource_provider, window, debugger_state);
  Build(data);
}

DebugModule::DebugModule(DebuggerHooksImpl* debugger_hooks,
                         script::GlobalEnvironment* global_environment,
                         RenderOverlay* render_overlay,
                         render_tree::ResourceProvider* resource_provider,
                         dom::Window* window, DebuggerState* debugger_state,
                         base::MessageLoop* message_loop) {
  ConstructionData data(debugger_hooks, global_environment, message_loop,
                        render_overlay, resource_provider, window,
                        debugger_state);
  Build(data);
}

DebugModule::~DebugModule() {
  if (debugger_hooks_) debugger_hooks_->DetachDebugger();
  if (!is_frozen_) {
    // Shutting down without navigating. Give everything a chance to cleanup by
    // freezing, but throw away the state.
    Freeze();
  }
  if (debug_backend_) {
    debug_backend_->UnbindAgents();
  }
}

void DebugModule::Build(const ConstructionData& data) {
  DCHECK(data.message_loop);

  if (base::MessageLoop::current() == data.message_loop) {
    BuildInternal(data);
  } else {
    data.message_loop->task_runner()->PostBlockingTask(
        FROM_HERE,
        base::Bind(&DebugModule::BuildInternal, base::Unretained(this), data));
  }

  DCHECK(debug_dispatcher_);
}

void DebugModule::BuildInternal(const ConstructionData& data) {
  DCHECK(base::MessageLoop::current() == data.message_loop);
  DCHECK(data.global_environment);

  // Create the backend objects supporting the debugger agents.
  script_debugger_ =
      script::ScriptDebugger::CreateDebugger(data.global_environment, this);
  script_runner_.reset(new DebugScriptRunner(
      data.global_environment, script_debugger_.get(),
      data.window ? data.window->csp_delegate() : nullptr));
  debug_dispatcher_.reset(
      new DebugDispatcher(script_debugger_.get(), script_runner_.get()));
  debug_backend_ = WrapRefCounted(new DebugBackend(
      data.global_environment, script_debugger_.get(),
      base::Bind(&DebugModule::SendEvent, base::Unretained(this))));

  debugger_hooks_ = data.debugger_hooks;
  if (debugger_hooks_) debugger_hooks_->AttachDebugger(script_debugger_.get());


  // Create the agents that implement the various devtools protocol domains by
  // handling commands and sending event notifications. The script debugger
  // agent is an adapter to the engine-specific script debugger, which may
  // directly handle one or more protocol domains.
  script_debugger_agent_.reset(
      new ScriptDebuggerAgent(debug_dispatcher_.get(), script_debugger_.get()));
  cobalt_agent_.reset(new CobaltAgent(debug_dispatcher_.get()));
  log_agent_.reset(new LogAgent(debug_dispatcher_.get()));
  dom_agent_.reset(new DOMAgent(debug_dispatcher_.get()));
  css_agent_ = WrapRefCounted(new CSSAgent(debug_dispatcher_.get()));
  if (data.render_overlay && data.resource_provider && data.window) {
    // Create render layers for the agents that need them and chain them
    // together. Ownership will be passed to the agent that uses each layer.
    // The layers will be painted in the reverse order they are listed here.
    std::unique_ptr<RenderLayer> page_render_layer(new RenderLayer(base::Bind(
        &RenderOverlay::SetOverlay, base::Unretained(data.render_overlay))));

    std::unique_ptr<RenderLayer> overlay_render_layer(
        new RenderLayer(base::Bind(&RenderLayer::SetBackLayer,
                                   page_render_layer->AsWeakPtr())));
    overlay_agent_.reset(new OverlayAgent(debug_dispatcher_.get(),
                                          std::move(overlay_render_layer)));
    page_agent_.reset(new PageAgent(debug_dispatcher_.get(), data.window,
                                    std::move(page_render_layer),
                                    data.resource_provider));
  }
  tracing_agent_.reset(
      new TracingAgent(debug_dispatcher_.get(), script_debugger_.get()));

  // Hook up hybrid agent JavaScript to the DebugBackend.
  debug_backend_->BindAgents(css_agent_);

  // Restore the clients in the dispatcher first so they get events that the
  // agents might send as part of restoring state.
  if (data.debugger_state) {
    debug_dispatcher_->RestoreClients(
        std::move(data.debugger_state->attached_clients));
  }

  // Restore the agents with their state from before navigation. Do this
  // unconditionally to give the agents a place to initialize themselves whether
  // or not state is being restored.
#ifndef USE_HACKY_COBALT_CHANGES
  base::DictionaryValue* agents_state =
      data.debugger_state == nullptr ? nullptr
                                     : data.debugger_state->agents_state.get();
  cobalt_agent_->Thaw(RemoveAgentState(kCobaltAgent, agents_state));
  script_debugger_agent_->Thaw(
      RemoveAgentState(kScriptDebuggerAgent, agents_state));
  log_agent_->Thaw(RemoveAgentState(kLogAgent, agents_state));
  dom_agent_->Thaw(RemoveAgentState(kDomAgent, agents_state));
  css_agent_->Thaw(RemoveAgentState(kCssAgent, agents_state));
  if (overlay_agent_)
    overlay_agent_->Thaw(RemoveAgentState(kOverlayAgent, agents_state));
  if (page_agent_)
    page_agent_->Thaw(RemoveAgentState(kPageAgent, agents_state));
  tracing_agent_->Thaw(RemoveAgentState(kTracingAgent, agents_state));
#endif

  is_frozen_ = false;
}

std::unique_ptr<DebuggerState> DebugModule::Freeze() {
#ifndef USE_HACKY_COBALT_CHANGES
  DCHECK(!is_frozen_);
  is_frozen_ = true;

  std::unique_ptr<DebuggerState> debugger_state(new DebuggerState());

  debugger_state->agents_state.reset(new base::DictionaryValue());
  base::DictionaryValue* agents_state = debugger_state->agents_state.get();
  StoreAgentState(agents_state, kCobaltAgent, cobalt_agent_->Freeze());
  StoreAgentState(agents_state, kScriptDebuggerAgent,
                  script_debugger_agent_->Freeze());
  StoreAgentState(agents_state, kLogAgent, log_agent_->Freeze());
  StoreAgentState(agents_state, kDomAgent, dom_agent_->Freeze());
  StoreAgentState(agents_state, kCssAgent, css_agent_->Freeze());
  if (overlay_agent_)
    StoreAgentState(agents_state, kOverlayAgent, overlay_agent_->Freeze());
  if (page_agent_)
    StoreAgentState(agents_state, kPageAgent, page_agent_->Freeze());
  StoreAgentState(agents_state, kTracingAgent, tracing_agent_->Freeze());

  // Take the clients from the dispatcher last so they still get events that
  the
      // agents might send as part of being frozen.
      debugger_state->attached_clients = debug_dispatcher_->ReleaseClients();

  return debugger_state;
#else
  return nullptr;
#endif
}

void DebugModule::SendEvent(const std::string& method,
                            const std::string& params) {
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
