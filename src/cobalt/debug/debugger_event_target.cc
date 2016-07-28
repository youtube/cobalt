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

#if defined(ENABLE_DEBUG_CONSOLE)

#include "cobalt/debug/debugger_event_target.h"

#include "cobalt/base/source_location.h"

namespace cobalt {
namespace debug {

DebuggerEventTarget::DebuggerEventTarget() {}

DebuggerEventTarget::~DebuggerEventTarget() {
  // Delete all registered listeners.
  for (ListenerSet::const_iterator it = listeners_.begin();
       it != listeners_.end(); ++it) {
    delete *it;
  }
}

void DebuggerEventTarget::DispatchEvent(
    const std::string& method, const base::optional<std::string>& json_params) {
  base::AutoLock auto_lock(lock_);

  for (ListenerSet::const_iterator it = listeners_.begin();
       it != listeners_.end(); ++it) {
    const ListenerInfo* listener = *it;
    listener->message_loop_proxy->PostTask(
        FROM_HERE, base::Bind(&DebuggerEventTarget::NotifyListener, this,
                              listener, method, json_params));
  }
}

void DebuggerEventTarget::AddListener(
    const DebuggerEventTarget::DebuggerEventCallbackArg& callback) {
  base::AutoLock auto_lock(lock_);
  listeners_.insert(
      new ListenerInfo(this, callback, base::MessageLoopProxy::current()));
}

void DebuggerEventTarget::NotifyListener(
    const DebuggerEventTarget::ListenerInfo* listener,
    const std::string& method, const base::optional<std::string>& json_params) {
  DCHECK_EQ(base::MessageLoopProxy::current(), listener->message_loop_proxy);
  listener->callback.value().Run(method, json_params);
}

}  // namespace debug
}  // namespace cobalt

#endif  // ENABLE_DEBUG_CONSOLE
