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

#ifndef COBALT_H5VCC_H5VCC_RUNTIME_EVENT_TARGET_H_
#define COBALT_H5VCC_H5VCC_RUNTIME_EVENT_TARGET_H_

#include <vector>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "cobalt/h5vcc/h5vcc_event_listener_container.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

// This class implements the onPause attribute of the h5vcc.runtime protocol,
// modeled after the events in Chrome's runtime extension, e.g.
// https://developer.chrome.com/apps/runtime#event-onSuspend

class H5vccRuntimeEventTarget : public script::Wrappable {
 public:
  // Type for JavaScript event callback.
  typedef script::CallbackFunction<void()> H5vccRuntimeEventCallback;
  typedef script::ScriptValue<H5vccRuntimeEventCallback>
      H5vccRuntimeEventCallbackHolder;

  H5vccRuntimeEventTarget();

  // Called from JavaScript to register an event listener callback.
  // May be called from any thread.
  void AddListener(const H5vccRuntimeEventCallbackHolder& callback_holder);

  // Dispatches an event to the registered listeners.
  // May be called from any thread.
  void DispatchEvent();

  DEFINE_WRAPPABLE_TYPE(H5vccRuntimeEventTarget);

 private:
  H5vccEventListenerContainer<void, H5vccRuntimeEventCallback> listeners_;

  DISALLOW_COPY_AND_ASSIGN(H5vccRuntimeEventTarget);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_RUNTIME_EVENT_TARGET_H_
