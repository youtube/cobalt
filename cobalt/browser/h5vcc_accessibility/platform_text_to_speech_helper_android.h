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

#ifndef COBALT_BROWSER_H5VCC_ACCESSIBILITY_PLATFORM_TEXT_TO_SPEECH_HELPER_ANDROID_H_
#define COBALT_BROWSER_H5VCC_ACCESSIBILITY_PLATFORM_TEXT_TO_SPEECH_HELPER_ANDROID_H_

#include "base/scoped_observation.h"
#include "cobalt/browser/h5vcc_accessibility/platform_text_to_speech_helper.h"
#include "starboard/android/shared/text_to_speech_observer.h"

namespace starboard {

class CobaltTextToSpeechHelper;

}  // namespace starboard

namespace h5vcc_accessibility {

// TODO(b/407584170): Understand how many H5vccAccessibilityImpl instances are
// generated in the browser.
// TODO(b/407584348): Compare with a different design approach that implements
// a separate EventListener to replace TextToSpeechObserver. With this
// alternative, H5vccAccessibilityImpl can have a single responsibility and
// delegate client/listener management to another class.
class PlatformTextToSpeechHelperAndroid
    : public PlatformTextToSpeechHelper,
      public starboard::TextToSpeechObserver {
 public:
  explicit PlatformTextToSpeechHelperAndroid(
      base::WeakPtr<PlatformTextToSpeechHelper::Client> client);
  ~PlatformTextToSpeechHelperAndroid() override;

  // PlatformTextToSpeechHelper overrides.
  bool IsTextToSpeechEnabled() override;

  // starboard::TextToSpeechObserver overrides.
  void ObserveTextToSpeechChange() override;

 private:
  base::ScopedObservation<starboard::CobaltTextToSpeechHelper,
                          starboard::TextToSpeechObserver>
      cobalt_text_to_speech_observer_{this};
};

}  // namespace h5vcc_accessibility

#endif  // COBALT_BROWSER_H5VCC_ACCESSIBILITY_PLATFORM_TEXT_TO_SPEECH_HELPER_ANDROID_H_
