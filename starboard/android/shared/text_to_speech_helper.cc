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

#include "cobalt/browser/h5vcc_accessibility/h5vcc_accessibility_manager.h"
#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/memory.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/CobaltTextToSpeechHelper_jni.h"

namespace starboard {

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

CobaltTextToSpeechHelper* CobaltTextToSpeechHelper::GetInstance() {
  return base::Singleton<CobaltTextToSpeechHelper>::get();
}

void CobaltTextToSpeechHelper::Initialize(JNIEnv* env) {
  if (j_text_to_speech_helper_.obj()) {
    return;
  }
  j_text_to_speech_helper_ =
      StarboardBridge::GetInstance()->GetTextToSpeechHelper(env);
}

bool CobaltTextToSpeechHelper::IsTextToSpeechEnabled(JNIEnv* env) const {
  bool enabled = Java_CobaltTextToSpeechHelper_isScreenReaderEnabled(
      env, j_text_to_speech_helper_);
  return enabled;
}

void CobaltTextToSpeechHelper::SendTextToSpeechChangeEvent(bool enabled) const {
  cobalt::browser::H5vccAccessibilityManager::GetInstance()
      ->OnTextToSpeechStateChanged(enabled);
}

void JNI_CobaltTextToSpeechHelper_SendTTSChangedEvent(JNIEnv* env) {
  CobaltTextToSpeechHelper::GetInstance()->Initialize(env);
  bool enabled =
      CobaltTextToSpeechHelper::GetInstance()->IsTextToSpeechEnabled(env);
  CobaltTextToSpeechHelper::GetInstance()->SendTextToSpeechChangeEvent(enabled);
}

}  // namespace starboard
