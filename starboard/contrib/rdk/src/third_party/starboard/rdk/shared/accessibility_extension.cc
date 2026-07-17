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
#include "third_party/starboard/rdk/shared/rdkservices.h"

namespace starboard {

bool GetTextToSpeechSettings(SbAccessibilityTextToSpeechSettings* out_setting) {
  if (!out_setting ||
      !MemoryIsZero(out_setting, sizeof(SbAccessibilityTextToSpeechSettings))) {
    return false;
  }
  out_setting->has_text_to_speech_setting = true;
  out_setting->is_text_to_speech_enabled = TextToSpeech::IsEnabled();
  return true;
}

bool GetDisplaySettings(SbAccessibilityDisplaySettings* out_setting) {
  if (!out_setting ||
      !MemoryIsZero(out_setting, sizeof(SbAccessibilityDisplaySettings))) {
    return false;
  }

  return Accessibility::GetDisplaySettings(out_setting);
}

bool GetCaptionSettings(SbAccessibilityCaptionSettings* caption_settings) {
  if (!caption_settings ||
      !MemoryIsZero(caption_settings, sizeof(SbAccessibilityCaptionSettings))) {
    return false;
  }

  return Accessibility::GetCaptionSettings(caption_settings);
}

bool SetCaptionsEnabled(bool enabled) {
  return false;
}


const StarboardExtensionAccessibilityApi kAccessibilityAPI = {
  kStarboardExtensionAccessibilityName,
  1,
  &GetTextToSpeechSettings,
  &GetDisplaySettings,
  &GetCaptionSettings,
  &SetCaptionsEnabled
};

const void* GetAccessibilityApi() {
  return &kAccessibilityAPI;
}

}  // namespace starboard
