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
#ifndef COBALT_DEBUG_BACKEND_DEBUG_MODULE_H_
#define COBALT_DEBUG_BACKEND_DEBUG_MODULE_H_

#include <memory>
#include <string>

#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/debug/backend/css_agent.h"
#include "cobalt/debug/backend/debug_backend.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/backend/debug_script_runner.h"
#include "cobalt/debug/backend/debugger_hooks_impl.h"
#include "cobalt/debug/backend/debugger_state.h"
#include "cobalt/debug/backend/dom_agent.h"
#include "cobalt/debug/backend/log_agent.h"
#include "cobalt/debug/backend/overlay_agent.h"
#include "cobalt/debug/backend/page_agent.h"
#include "cobalt/debug/backend/render_overlay.h"
#include "cobalt/debug/backend/script_debugger_agent.h"
#include "cobalt/debug/backend/tracing_agent.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/dom/window.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/script_debugger.h"

namespace cobalt {
namespace debug {
namespace backend {

// A container for a DebugDispatcher and all the agents that implement debugging
// protocol domains. This class is not intended to provide any functionality of
// its own beyond access to the DebugDispatcher member, which provides the
// interface to send commands specified by the debugging protocol:
// https://chromedevtools.github.io/devtools-protocol/1-3
class DebugModule : public script::ScriptDebugger::Delegate {
 public:
  // Construct the debug dispatcher on the current message loop.
  DebugModule(DebuggerHooksImpl* debugger_hooks,
              script::GlobalEnvironment* global_environment,
              RenderOverlay* render_overlay,
              render_tree::ResourceProvider* resource_provider,
              dom::Window* window, DebuggerState* debugger_state);

  // Construct the debug dispatcher on the specified message loop.
  DebugModule(DebuggerHooksImpl* debugger_hooks,
              script::GlobalEnvironment* global_environment,
              RenderOverlay* render_overlay,
              render_tree::ResourceProvider* resource_provider,
              dom::Window* window, DebuggerState* debugger_state,
              base::MessageLoop* message_loop);

  virtual ~DebugModule();

  DebugDispatcher* debug_dispatcher() const { return debug_dispatcher_.get(); }

  // Shutdown this instance of the debugger before navigation and return its
  // state so that it can be restored in the new |DebugModule| after navigation.
  // This instance of the debugger is no longer usable after calling this
  // method, with the expectation is that it will soon be deleted with its
  // owning WebModule.
  std::unique_ptr<DebuggerState> Freeze();

 private:
  // Data used to construct an instance of this class that does not need to be
  // persisted.
  struct ConstructionData {
    ConstructionData(DebuggerHooksImpl* debugger_hooks,
                     script::GlobalEnvironment* global_environment,
                     base::MessageLoop* message_loop,
                     RenderOverlay* render_overlay,
                     render_tree::ResourceProvider* resource_provider,
                     dom::Window* window, DebuggerState* debugger_state)
        : debugger_hooks(debugger_hooks),
          global_environment(global_environment),
          message_loop(message_loop),
          render_overlay(render_overlay),
          resource_provider(resource_provider),
          window(window),
          debugger_state(debugger_state) {}

    DebuggerHooksImpl* debugger_hooks;
    script::GlobalEnvironment* global_environment;
    base::MessageLoop* message_loop;
    RenderOverlay* render_overlay;
    render_tree::ResourceProvider* resource_provider;
    dom::Window* window;
    DebuggerState* debugger_state;
  };

  // Builds |debug_dispatcher_| on |message_loop_| by calling |BuildInternal|,
  // posting the task and waiting for it if necessary.
  void Build(const ConstructionData& data);

  // Signals |created| when done, if not NULL.
  void BuildInternal(const ConstructionData& data,
                     base::WaitableEvent* created);

  // Sends a protocol event to the frontend through |DebugDispatcher|.
  void SendEvent(const std::string& method, const std::string& params);

  // script::ScriptDebugger::Delegate implementation.
  void OnScriptDebuggerPause() override;
  void OnScriptDebuggerResume() override;
  void OnScriptDebuggerResponse(const std::string& response) override;
  void OnScriptDebuggerEvent(const std::string& event) override;

  bool is_frozen_ = true;

  DebuggerHooksImpl* debugger_hooks_ = nullptr;

  // Handles all debugging interaction with the JavaScript engine.
  std::unique_ptr<script::ScriptDebugger> script_debugger_;

  // Runs backend JavaScript.
  std::unique_ptr<DebugScriptRunner> script_runner_;

  // Handles routing of commands, responses and event notifications.
  std::unique_ptr<DebugDispatcher> debug_dispatcher_;

  // Wrappable object providing native helpers for backend JavaScript.
  scoped_refptr<DebugBackend> debug_backend_;
  std::unique_ptr<LogAgent> log_agent_;
  std::unique_ptr<DOMAgent> dom_agent_;
  scoped_refptr<CSSAgent> css_agent_;
  std::unique_ptr<OverlayAgent> overlay_agent_;
  std::unique_ptr<PageAgent> page_agent_;
  std::unique_ptr<ScriptDebuggerAgent> script_debugger_agent_;
  std::unique_ptr<TracingAgent> tracing_agent_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_DEBUG_MODULE_H_
