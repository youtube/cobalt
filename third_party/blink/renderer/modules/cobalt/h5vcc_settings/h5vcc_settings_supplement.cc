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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_settings/h5vcc_settings_supplement.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

// static
const char H5vccSettingsSupplement::kSupplementName[] = "H5vccSettingsSupplement";

// static
H5vccSettingsSupplement& H5vccSettingsSupplement::From(LocalDOMWindow& window) {
  H5vccSettingsSupplement* supplement =
      Supplement<LocalDOMWindow>::From<H5vccSettingsSupplement>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<H5vccSettingsSupplement>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

H5vccSettingsSupplement::H5vccSettingsSupplement(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void H5vccSettingsSupplement::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
