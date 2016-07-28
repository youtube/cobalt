/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_runtime_event_target.h"

#include "base/location.h"

namespace cobalt {
namespace h5vcc {

H5vccRuntimeEventTarget::H5vccRuntimeEventTarget() {}

H5vccRuntimeEventTarget::~H5vccRuntimeEventTarget() {
  // Delete all registered listeners.
  for (ListenerVector::const_iterator it = listeners_.begin();
       it != listeners_.end(); ++it) {
    delete *it;
  }
}

void H5vccRuntimeEventTarget::AddListener(
    const H5vccRuntimeEventTarget::H5vccRuntimeEventCallbackHolder& callback) {
  base::AutoLock auto_lock(lock_);
  listeners_.push_back(new ListenerInfo(this, callback));
}

void H5vccRuntimeEventTarget::DispatchEvent() {
  base::AutoLock auto_lock(lock_);

  for (ListenerVector::const_iterator it = listeners_.begin();
       it != listeners_.end(); ++it) {
    const ListenerInfo* listener = *it;
    listener->message_loop->PostTask(
        FROM_HERE,
        base::Bind(&H5vccRuntimeEventTarget::NotifyListener, this, listener));
  }
}

void H5vccRuntimeEventTarget::NotifyListener(
    const H5vccRuntimeEventTarget::ListenerInfo* listener) {
  DCHECK_EQ(base::MessageLoopProxy::current(), listener->message_loop);
  listener->callback.value().Run();
}

}  // namespace h5vcc
}  // namespace cobalt
