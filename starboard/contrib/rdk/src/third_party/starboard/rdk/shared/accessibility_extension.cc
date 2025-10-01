//
// Copyright 2024 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "starboard/common/memory.h"

#include "third_party/starboard/rdk/shared/accessibility_extension.h"
#include "third_party/starboard/rdk/shared/platform/platform_interface.h"

namespace starboard {

namespace accessibility {

bool GetTextToSpeechSettings(SbAccessibilityTextToSpeechSettings* out_setting) {
  if (!out_setting ||
      !::starboard::common::MemoryIsZero(
        out_setting, sizeof(SbAccessibilityTextToSpeechSettings))) {
    return false;
  }
  out_setting->has_text_to_speech_setting = true;
  out_setting->is_text_to_speech_enabled =
    platform::text_to_speech().is_enabled().value_or(false);
  return true;
}

bool GetDisplaySettings(SbAccessibilityDisplaySettings* out_setting) {
  if (!out_setting ||
      !::starboard::common::MemoryIsZero(
        out_setting, sizeof(SbAccessibilityDisplaySettings))) {
    return false;
  }

  return platform::accessibility().display_settings(*out_setting).value_or(false);
}

bool GetCaptionSettings(SbAccessibilityCaptionSettings* out_setting) {
  if (!out_setting ||
      !::starboard::common::MemoryIsZero(
          out_setting, sizeof(SbAccessibilityCaptionSettings))) {
    return false;
  }

  return platform::accessibility().caption_settings(*out_setting).value_or(false);
}

bool SetCaptionsEnabled(bool enabled) {
  return false;
}

}  // namespace accessibility

const StarboardExtensionAccessibilityApi kAccessibilityAPI = {
  kStarboardExtensionAccessibilityName,
  1,
  &accessibility::GetTextToSpeechSettings,
  &accessibility::GetDisplaySettings,
  &accessibility::GetCaptionSettings,
  &accessibility::SetCaptionsEnabled
};

const void* GetAccessibilityApi() {
  return &kAccessibilityAPI;
}

}  // namespace starboard
