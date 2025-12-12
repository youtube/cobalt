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

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/CobaltTextToSpeechHelper_jni.h"

namespace starboard {

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

void CobaltTextToSpeechHelper::AddObserver(TextToSpeechObserver* observer) {
  base::AutoLock lock(observers_lock_);
  observers_.AddObserver(observer);
}

void CobaltTextToSpeechHelper::RemoveObserver(TextToSpeechObserver* observer) {
  base::AutoLock lock(observers_lock_);
  observers_.RemoveObserver(observer);
}

bool CobaltTextToSpeechHelper::IsTextToSpeechEnabled(JNIEnv* env) const {
  bool enabled = Java_CobaltTextToSpeechHelper_isScreenReaderEnabled(
      env, j_text_to_speech_helper_);
  return enabled;
}

void CobaltTextToSpeechHelper::SendTextToSpeechChangeEvent() const {
  base::AutoLock lock(observers_lock_);
  for (TextToSpeechObserver& observer : observers_) {
    observer.ObserveTextToSpeechChange();
  }
}

void JNI_CobaltTextToSpeechHelper_SendTTSChangedEvent(JNIEnv* env) {
  CobaltTextToSpeechHelper::GetInstance()->Initialize(env);
  CobaltTextToSpeechHelper::GetInstance()->SendTextToSpeechChangeEvent();
}

}  // namespace starboard
