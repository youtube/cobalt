/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_DEBUG_DEBUGGER_EVENT_TARGET_H_
#define COBALT_DEBUG_DEBUGGER_EVENT_TARGET_H_

#if defined(ENABLE_DEBUG_CONSOLE)

#include <set>
#include <string>

#include "base/callback.h"
#include "base/message_loop.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_object.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace debug {

// This class implements the onEvent attribute of the Chrome Debugger protocol:
// https://developer.chrome.com/extensions/debugger#event-onEvent
//
// It receives events from the debug server and passes along to registered
// listeners.

class DebuggerEventTarget : public script::Wrappable {
 public:
  // Type for JavaScript debugger event callback.
  typedef script::CallbackFunction<void(
      const std::string& method,
      const base::optional<std::string>& json_params)> DebuggerEventCallback;
  typedef script::ScriptObject<DebuggerEventCallback> DebuggerEventCallbackArg;

  // Type for listener info.
  // We store the message loop from which the listener was registered,
  // so we can run the callback on the same loop.
  struct ListenerInfo {
    ListenerInfo(DebuggerEventTarget* const debugger_event_target,
                 const DebuggerEventCallbackArg& cb,
                 const scoped_refptr<base::MessageLoopProxy>& proxy)
        : callback(debugger_event_target, cb), message_loop_proxy(proxy) {}
    DebuggerEventCallbackArg::Reference callback;
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy;
  };

  DebuggerEventTarget();
  ~DebuggerEventTarget();

  // Dispatches a debugger event to the registered listeners.
  // May be called from any thread.
  void DispatchEvent(const std::string& method,
                     const base::optional<std::string>& json_params);

  // Called from JavaScript to register an event listener callback.
  // May be called from any thread.
  void AddListener(const DebuggerEventCallbackArg& callback);

  DEFINE_WRAPPABLE_TYPE(DebuggerEventTarget);

 protected:
  // Notifies a particular listener. Called on the same message loop the
  // listener registered its callback from.
  void NotifyListener(const ListenerInfo* listener, const std::string& method,
                      const base::optional<std::string>& json_params);

 private:
  typedef std::set<ListenerInfo*> ListenerSet;

  ListenerSet listeners_;
  base::Lock lock_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // ENABLE_DEBUG_CONSOLE
#endif  // COBALT_DEBUG_DEBUGGER_EVENT_TARGET_H_
