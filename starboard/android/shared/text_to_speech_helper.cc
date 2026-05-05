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

#include "starboard/android/shared/text_to_speech_helper.h"

#include "starboard/android/shared/starboard_bridge.h"

// TODO(b/492704919): enable on AOSP when the layering violation is fixed.
#if !BUILDFLAG(IS_PARTNER_TOOLCHAIN)
#include "cobalt/browser/h5vcc_accessibility/h5vcc_accessibility_manager.h"
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/CobaltTextToSpeechHelper_jni.h"

namespace starboard {

TextToSpeechHelper* TextToSpeechHelper::GetInstance() {
  return base::Singleton<TextToSpeechHelper>::get();
}

void TextToSpeechHelper::Initialize(JNIEnv* env) {
  if (j_text_to_speech_helper_.obj()) {
    return;
  }
  j_text_to_speech_helper_ =
      StarboardBridge::GetInstance()->GetTextToSpeechHelper(env);
}

bool TextToSpeechHelper::IsTextToSpeechEnabled(JNIEnv* env) const {
  bool enabled = Java_CobaltTextToSpeechHelper_isScreenReaderEnabled(
      env, j_text_to_speech_helper_);
  return enabled;
}

void TextToSpeechHelper::SendTextToSpeechChangeEvent(bool enabled) const {
  // TODO(b/492704919): enable on AOSP when the layering violation is fixed.
#if !BUILDFLAG(IS_PARTNER_TOOLCHAIN)
  cobalt::browser::H5vccAccessibilityManager::GetInstance()
      ->OnTextToSpeechStateChanged(enabled);
#endif  // !BUILDFLAG(IS_PARTNER_TOOLCHAIN)
}

void JNI_CobaltTextToSpeechHelper_SendTTSChangedEvent(JNIEnv* env) {
  TextToSpeechHelper::GetInstance()->Initialize(env);
  bool enabled = TextToSpeechHelper::GetInstance()->IsTextToSpeechEnabled(env);
  TextToSpeechHelper::GetInstance()->SendTextToSpeechChangeEvent(enabled);
}

}  // namespace starboard
