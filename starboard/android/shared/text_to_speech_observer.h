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

#ifndef STARBOARD_ANDROID_SHARED_TEXT_TO_SPEECH_OBSERVER_H_
#define STARBOARD_ANDROID_SHARED_TEXT_TO_SPEECH_OBSERVER_H_

#include "base/observer_list_types.h"

namespace starboard {
namespace android {
namespace shared {

// Breaks circular dependency between H5vccAccessibilityImpl and
// CobaltTextToSpeechHelper through an observer interface pattern:
//
// H5vccAccessibilityImpl implements TextToSpeechObserver to receive state
// changes, CobaltTextToSpeechHelper maintains observer list without concrete
// H5vccAccessibilityImpl dependencies. State queries flow
// H5vccAccessibilityImpl → CobaltTextToSpeechHelper. Notifications flow
// CobaltTextToSpeechHelper → TextToSpeechObserver (H5vccAccessibilityImpl).
class TextToSpeechObserver : public base::CheckedObserver {
 public:
  virtual void ObserveTextToSpeechChange() = 0;
  ~TextToSpeechObserver() override = default;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_TEXT_TO_SPEECH_OBSERVER_H_
