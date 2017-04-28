// Copyright 2017 Google Inc. All Rights Reserved.
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

// Module Overview: Starboard Accessibility module
//
// Provides access to the system options and settings related to accessibility.

#ifndef STARBOARD_ACCESSIBILITY_H_
#define STARBOARD_ACCESSIBILITY_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SB_API_VERSION >= 4

// A group of settings related to text-to-speech functionality, for platforms
// that expose system settings for text-to-speech.
typedef struct SbAccessibilityTextToSpeechSettings {
  // Whether this platform has a system setting for text-to-speech or not.
  bool has_text_to_speech_setting;

  // Whether the text-to-speech setting is enabled or not. This setting is only
  // valid if |has_text_to_speech_setting| is set to true.
  bool is_text_to_speech_enabled;
} SbAccessibilityTextToSpeechSettings;

// Get the platform settings related to the text-to-speech accessibility
// feature. This function returns false if |out_settings| is NULL or if it is
// not zero-initialized.
//
// |out_settings|: A pointer to a zero-initialized
//    SbAccessibilityTextToSpeechSettings struct.
SB_EXPORT bool SbAccessibilityGetTextToSpeechSettings(
    SbAccessibilityTextToSpeechSettings* out_settings);

typedef struct SbAccessibilityDisplaySettings {
  // Whether this platform has a system setting for high contrast text or not.
  bool has_high_contrast_text_setting;

  // Whether the high contrast text setting is enabled or not.
  bool is_high_contrast_text_enabled;
} SbAccessibilityDisplaySettings;

// Get the platform settings related to high contrast text.
// This function returns false if |out_settings| is NULL or if it is
// not zero-initialized.
//
// |out_settings|: A pointer to a zero-initialized
//    SbAccessibilityDisplaySettings* struct.
SB_EXPORT bool SbAccessibilityGetDisplaySettings(
    SbAccessibilityDisplaySettings* out_settings);

#endif  // SB_API_VERSION >= 4

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_ACCESSIBILITY_H_
