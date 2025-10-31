// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#import <Foundation/Foundation.h>

#include "starboard/common/memory.h"
#include "starboard/tvos/shared/accessibility_extension.h"
#import "starboard/tvos/shared/speech_synthesizer.h"
#import "starboard/tvos/shared/starboard_application.h"

namespace starboard {
namespace shared {
namespace uikit {

namespace accessibility {

bool GetTextToSpeechSettings(SbAccessibilityTextToSpeechSettings* out_setting) {
  if (!out_setting ||
      !starboard::common::MemoryIsZero(
          out_setting, sizeof(SbAccessibilityTextToSpeechSettings))) {
    return false;
  }

  @autoreleasepool {
    out_setting->has_text_to_speech_setting = true;
    out_setting->is_text_to_speech_enabled =
        SBDGetApplication().speechSynthesizer.screenReaderActive;
  }
  return true;
}

}  // namespace accessibility
}  // namespace uikit
}  // namespace shared
}  // namespace starboard
