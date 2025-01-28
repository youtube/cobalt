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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_system/h_5_vcc_system.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

H5vccSystem::H5vccSystem(LocalDOMWindow& window) {}

const String H5vccSystem::advertisingId() const {
  NOTIMPLEMENTED();

  // TODO(b/377049113) add a mojom service and populate the value for
  // advertising_id_.
  //   return advertising_id_;
  return String("fake advertisingId");
}

void H5vccSystem::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
