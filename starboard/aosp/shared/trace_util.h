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

#ifndef STARBOARD_ANDROID_SHARED_TRACE_UTIL_H_
#define STARBOARD_ANDROID_SHARED_TRACE_UTIL_H_

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"

namespace starboard {
namespace android {
namespace shared {

// A simple scoped wrapper of |android.os.Trace|.
struct ScopedTrace {
  explicit ScopedTrace(const char* section_name) {
    JniEnvExt* env = JniEnvExt::Get();
    ScopedLocalJavaRef<jstring> j_section_name(
        env->NewStringStandardUTFOrAbort(section_name));
    env->CallStaticVoidMethodOrAbort("android/os/Trace", "beginSection",
                                     "(Ljava/lang/String;)V",
                                     j_section_name.Get());
  }

  ~ScopedTrace() {
    JniEnvExt* env = JniEnvExt::Get();
    env->CallStaticVoidMethodOrAbort("android/os/Trace", "endSection", "()V");
  }
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_TRACE_UTIL_H_
