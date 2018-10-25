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

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/debug/backend/console_component.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/backend/dom_component.h"
#include "cobalt/debug/backend/page_component.h"
#include "cobalt/debug/backend/render_overlay.h"
#include "cobalt/debug/backend/runtime_component.h"
#include "cobalt/debug/backend/script_debugger_component.h"
#include "cobalt/dom/console.h"
#include "cobalt/dom/window.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/script_debugger.h"

namespace cobalt {
namespace debug {
namespace backend {

// A container for a DebugDispatcher and all the components that provide its
// functionality. This class is not intended to provide any functionality of
// its own beyond access to the DebugDispatcher member, which provides the
// interface specified by the debugging protocol:
// https://chromedevtools.github.io/devtools-protocol/1-3
class DebugModule : public script::ScriptDebugger::Delegate {
 public:
  // Construct the debug dispatcher on the current message loop.
  DebugModule(dom::Console* console,
              script::GlobalEnvironment* global_environment,
              RenderOverlay* render_overlay,
              render_tree::ResourceProvider* resource_provider,
              dom::Window* window);

  // Construct the debug dispatcher on the specified message loop.
  DebugModule(dom::Console* console,
              script::GlobalEnvironment* global_environment,
              RenderOverlay* render_overlay,
              render_tree::ResourceProvider* resource_provider,
              dom::Window* window, MessageLoop* message_loop);

  virtual ~DebugModule();

  DebugDispatcher* debug_dispatcher() const { return debug_dispatcher_.get(); }

 private:
  // Data used to construct an instance of this class that does not need to be
  // persisted.
  struct ConstructionData {
    ConstructionData(dom::Console* console,
                     script::GlobalEnvironment* global_environment,
                     MessageLoop* message_loop, RenderOverlay* render_overlay,
                     render_tree::ResourceProvider* resource_provider,
                     dom::Window* window)
        : console(console),
          global_environment(global_environment),
          message_loop(message_loop),
          render_overlay(render_overlay),
          resource_provider(resource_provider),
          window(window) {}

    dom::Console* console;
    script::GlobalEnvironment* global_environment;
    MessageLoop* message_loop;
    RenderOverlay* render_overlay;
    render_tree::ResourceProvider* resource_provider;
    dom::Window* window;
  };

  // Builds |debug_dispatcher_| on |message_loop_| by calling |BuildInternal|,
  // posting the task and waiting for it if necessary.
  void Build(const ConstructionData& data);

  // Signals |created| when done, if not NULL.
  void BuildInternal(const ConstructionData& data,
                     base::WaitableEvent* created);

  // script::ScriptDebugger::Delegate implementation.
  void OnScriptDebuggerPause() override;
  void OnScriptDebuggerResume() override;
  void OnScriptDebuggerResponse(const std::string& response) override;
  void OnScriptDebuggerEvent(const std::string& event) override;

  // Handles routing of commands, responses and event notifications.
  scoped_ptr<DebugDispatcher> debug_dispatcher_;

  // Handles all debugging interaction with the JavaScript engine.
  scoped_ptr<script::ScriptDebugger> script_debugger_;

  // Debug components implement the debugging protocol.
  scoped_ptr<ConsoleComponent> console_component_;
  scoped_ptr<DOMComponent> dom_component_;
  scoped_ptr<PageComponent> page_component_;
  scoped_ptr<RuntimeComponent> runtime_component_;
  scoped_ptr<ScriptDebuggerComponent> script_debugger_component_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_DEBUG_MODULE_H_
