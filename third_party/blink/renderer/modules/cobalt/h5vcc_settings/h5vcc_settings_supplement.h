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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_SETTINGS_H5VCC_SETTINGS_SUPPLEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_SETTINGS_H5VCC_SETTINGS_SUPPLEMENT_H_

#include <cstdint>
#include <optional>

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class MODULES_EXPORT H5vccSettingsSupplement final
    : public GarbageCollected<H5vccSettingsSupplement>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  static H5vccSettingsSupplement& From(LocalDOMWindow& window);

  explicit H5vccSettingsSupplement(LocalDOMWindow& window);

  void SetAppendFirstSegmentSynchronously(bool value) {
    append_first_segment_synchronously_ = value;
  }
  std::optional<bool> GetAppendFirstSegmentSynchronously() const {
    return append_first_segment_synchronously_;
  }

  void Trace(Visitor* visitor) const override;

 private:
  std::optional<bool> append_first_segment_synchronously_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_SETTINGS_H5VCC_SETTINGS_SUPPLEMENT_H_
