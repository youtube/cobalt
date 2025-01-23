// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_system/h_5_vcc_system.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// static
const char H5vccSystem::kSupplementName[] = "H5vccSystem";

//static
H5vccSystem* H5vccSystem::h5vccSystem(LocalDOMWindow& window) {
  H5vccSystem* h5vccSystem = Supplement<LocalDOMWindow>::From<H5vccSystem>(window);
  if (!h5vccSystem && window.GetExecutionContext()) {
    h5vccSystem = MakeGarbageCollected<H5vccSystem>(window);
  }
  return h5vccSystem;
}

H5vccSystem::H5vccSystem(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      ExecutionContextLifecycleObserver(window.GetExecutionContext()) {}

void H5vccSystem::ContextDestroyed() {}

const String H5vccSystem::advertisingId() const {
  // TODO populate the value here.
//   return advertising_id_;
  return String("Colin test advertisingId");
}

void H5vccSystem::Trace(Visitor* visitor) const {
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
