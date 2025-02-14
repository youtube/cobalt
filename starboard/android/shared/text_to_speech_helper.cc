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

#include <android/native_activity.h>

#include "starboard/android/shared/accessibility_extension.h"
#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/memory.h"
#include "starboard/speech_synthesis.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/CobaltTextToSpeechHelper_jni.h"

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace starboard {
namespace android {
namespace shared {

namespace accessibility {

bool GetTextToSpeechSettings(SbAccessibilityTextToSpeechSettings* out_setting) {
  if (!out_setting ||
      !starboard::common::MemoryIsZero(
          out_setting, sizeof(SbAccessibilityTextToSpeechSettings))) {
    return false;
  }

  JNIEnv* env = AttachCurrentThread();

  out_setting->has_text_to_speech_setting = true;
  ScopedJavaLocalRef<jobject> j_tts_helper =
      starboard::android::shared::StarboardBridge::GetInstance()
          ->GetTextToSpeechHelper(env);

  out_setting->is_text_to_speech_enabled =
      Java_CobaltTextToSpeechHelper_isScreenReaderEnabled(env, j_tts_helper);

  SB_LOG(INFO) << "TTS: out_setting->is_text_to_speech_enabled "
               << out_setting->is_text_to_speech_enabled;
  return true;
}

}  // namespace accessibility

extern "C" SB_EXPORT_PLATFORM void
JNI_CobaltTextToSpeechHelper_SendTTSChangedEvent(JNIEnv* env) {
  ApplicationAndroid::Get()->SendTTSChangedEvent();
}

void SbSpeechSynthesisSpeak(const char* text) {
  if (!text) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_tts_helper =
      starboard::android::shared::StarboardBridge::GetInstance()
          ->GetTextToSpeechHelper(env);

  jstring text_string = env->NewStringUTF(text);
  ScopedJavaLocalRef<jstring> j_text_string(env, text_string);

  Java_CobaltTextToSpeechHelper_speak(env, j_tts_helper, j_text_string);
}

void SbSpeechSynthesisCancel() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_tts_helper =
      starboard::android::shared::StarboardBridge::GetInstance()
          ->GetTextToSpeechHelper(env);

  Java_CobaltTextToSpeechHelper_cancel(env, j_tts_helper);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
