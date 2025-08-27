// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/extension/crash_handler.h"
#include "base/android/jni_android.h"
#include "starboard/android/shared/crash_handler.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_state.h"
#include "starboard/android/shared/jni_utils.h"

namespace starboard::android::shared {

bool OverrideCrashpadAnnotations(CrashpadAnnotations* crashpad_annotations) {
  return false;  // Deprecated
}

bool SetString(const char* key, const char* value) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedLocalJavaRef<jstring> j_key(JniNewStringStandardUTFOrAbort(env, key));
  ScopedLocalJavaRef<jstring> j_value(
      JniNewStringStandardUTFOrAbort(env, value));
  JniCallVoidMethodOrAbort(
      env, JNIState::GetStarboardBridge(), "setCrashContext",
      "(Ljava/lang/String;Ljava/lang/String;)V", j_key.Get(), j_value.Get());
  return true;
}

const CobaltExtensionCrashHandlerApi kCrashHandlerApi = {
    kCobaltExtensionCrashHandlerName,
    2,
    &OverrideCrashpadAnnotations,
    &SetString,
};

const void* GetCrashHandlerApi() {
  return &kCrashHandlerApi;
}

}  // namespace starboard::android::shared
