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

#include "starboard/android/shared/accessibility_extension.h"
#include "starboard/android/shared/text_to_speech_helper.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

bool GetTextToSpeechSettings(SbAccessibilityTextToSpeechSettings* out_setting) {
  if (!out_setting) {
    return false;
  }
  JNIEnv* env = jni_zero::AttachCurrentThread();
  TextToSpeechHelper* helper = TextToSpeechHelper::GetInstance();
  helper->Initialize(env);
  out_setting->has_text_to_speech_setting = true;
  out_setting->is_text_to_speech_enabled = helper->IsTextToSpeechEnabled(env);
  return true;
}

}  // namespace starboard
