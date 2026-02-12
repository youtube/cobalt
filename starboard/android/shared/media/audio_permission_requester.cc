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

#include "starboard/android/shared/media/audio_permission_requester.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "starboard/android/shared/starboard_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/AudioPermissionRequester_jni.h"

namespace starboard {
// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

bool RequestRecordAudioPermission(JNIEnv* env) {
  ScopedJavaLocalRef<jobject> j_audio_permission_requester =
      StarboardBridge::GetInstance()->GetAudioPermissionRequester(env);

  jboolean j_permission =
      Java_AudioPermissionRequester_requestRecordAudioPermission(
          env, j_audio_permission_requester);
  return j_permission == JNI_TRUE;
}
}  // namespace starboard
