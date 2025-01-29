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

#include "starboard/speech_synthesis.h"

#include "starboard/android/shared/starboard_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/CobaltTextToSpeechHelper_jni.h"

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

void SbSpeechSynthesisCancel() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_tts_helper =
      starboard::android::shared::StarboardBridge::GetInstance()
          ->GetTextToSpeechHelper(env);

  Java_CobaltTextToSpeechHelper_cancel(env, j_tts_helper);
}
