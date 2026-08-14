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

#include "starboard/android/shared/audio_permission_requester.h"

#include "starboard/android/shared/starboard_bridge.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/AudioPermissionRequester_jni.h"

namespace starboard {

bool RequestRecordAudioPermission(JNIEnv* env) {
  jni_zero::ScopedJavaLocalRef<jobject> j_audio_permission_requester =
      StarboardBridge::GetInstance()->GetAudioPermissionRequester(env);

  jboolean j_permission =
      Java_AudioPermissionRequester_requestRecordAudioPermission(
          env, j_audio_permission_requester);
  return j_permission == JNI_TRUE;
}
}  // namespace starboard
