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

#include "cobalt/browser/h5vcc_accessibility/platform_text_to_speech_helper_starboard.h"

#include <string.h>

#include "starboard/extension/accessibility.h"

namespace h5vcc_accessibility {

PlatformTextToSpeechHelperStarboard::PlatformTextToSpeechHelperStarboard(
    base::WeakPtr<PlatformTextToSpeechHelper::Client> client)
    : PlatformTextToSpeechHelper(client) {}

PlatformTextToSpeechHelperStarboard::~PlatformTextToSpeechHelperStarboard() =
    default;

bool PlatformTextToSpeechHelperStarboard::IsTextToSpeechEnabled() {
  auto accessibility_api =
      static_cast<const StarboardExtensionAccessibilityApi*>(
          SbSystemGetExtension(kStarboardExtensionAccessibilityName));
  if (!accessibility_api ||
      strcmp(accessibility_api->name, kStarboardExtensionAccessibilityName) !=
          0 ||
      accessibility_api->version < 1) {
    return false;
  }

  SbAccessibilityTextToSpeechSettings settings{};
  if (!accessibility_api->GetTextToSpeechSettings(&settings)) {
    return false;
  }

  return settings.has_text_to_speech_setting &&
         settings.is_text_to_speech_enabled;
}

}  // namespace h5vcc_accessibility
