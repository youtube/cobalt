// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/cobalt/h_5_vcc.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/cobalt/crash_log/crash_log.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_runtime/h_5_vcc_runtime.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_system/h_5_vcc_system.h"

namespace blink {

// static
const char H5vcc::kSupplementName[] = "H5vcc";

// static
H5vcc* H5vcc::h5vcc(LocalDOMWindow& window) {
  H5vcc* h5vcc = Supplement<LocalDOMWindow>::From<H5vcc>(window);
  if (!h5vcc && window.GetExecutionContext()) {
    h5vcc = MakeGarbageCollected<H5vcc>(window);
    ProvideTo(window, h5vcc);
  }
  return h5vcc;
}

H5vcc::H5vcc(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      crash_log_(MakeGarbageCollected<CrashLog>(window)),
      system_(MakeGarbageCollected<H5vccSystem>(window)),
      runtime_(MakeGarbageCollected<H5vccRuntime>(window)) {}

void H5vcc::Trace(Visitor* visitor) const {
  visitor->Trace(crash_log_);
  visitor->Trace(system_);
  visitor->Trace(runtime_);
  Supplement<LocalDOMWindow>::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
