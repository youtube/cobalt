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

#ifndef STARBOARD_ANDROID_SHARED_JNI_UTILS_H_
#define STARBOARD_ANDROID_SHARED_JNI_UTILS_H_

#include <jni.h>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"

namespace starboard {
namespace android {
namespace shared {

// Wrapper class to manage the lifetime of a local reference to Java type
// |JT|. This is necessary for local references to |JT|s that are obtained in
// native code that was not called into from Java, since they will otherwise
// not be cleaned up.
template <typename JT>
class ScopedLocalJavaRef {
 public:
  explicit ScopedLocalJavaRef(jobject j_object = NULL)
      : jt_(static_cast<JT>(j_object)) {}
  ~ScopedLocalJavaRef() {
    if (jt_) {
      JniEnvExt::Get()->DeleteLocalRef(jt_);
      jt_ = NULL;
    }
  }
  JT Get() const { return jt_; }
  void Reset(jobject j_object) {
    if (jt_) {
      JniEnvExt::Get()->DeleteLocalRef(jt_);
    }
    jt_ = static_cast<JT>(j_object);
  }
  operator bool() const { return jt_; }

 private:
  JT jt_;

  ScopedLocalJavaRef(const ScopedLocalJavaRef&) = delete;
  void operator=(const ScopedLocalJavaRef&) = delete;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_JNI_UTILS_H_
