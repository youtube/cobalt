// Copyright 2017 Google Inc. All Rights Reserved.
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
#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace android {
namespace shared {

// Wrapper class to manage the lifetime of a local reference to a |jobject|.
// This is necessary for local references to |jobject|s that are obtained in
// native code that was not called into from Java, since they will otherwise
// not be cleaned up.
class ScopedLocalJavaRef {
 public:
  explicit ScopedLocalJavaRef(jobject j_object = NULL) : j_object_(j_object) {}
  ~ScopedLocalJavaRef() {
    if (j_object_) {
      JniEnvExt::Get()->DeleteLocalRef(j_object_);
      j_object_ = NULL;
    }
  }
  jobject Get() const { return j_object_; }
  void Reset(jobject new_object) {
    if (j_object_) {
      JniEnvExt::Get()->DeleteLocalRef(j_object_);
    }
    j_object_ = new_object;
  }
  operator bool() const { return j_object_; }

 private:
  jobject j_object_;

  SB_DISALLOW_COPY_AND_ASSIGN(ScopedLocalJavaRef);
};

// Convenience class to manage the lifetime of a local Java ByteBuffer
// reference, and provide accessors to its properties.
class ScopedJavaByteBuffer {
 public:
  explicit ScopedJavaByteBuffer(jobject j_byte_buffer)
      : j_byte_buffer_(j_byte_buffer) {}
  void* address() const {
    return JniEnvExt::Get()->GetDirectBufferAddress(j_byte_buffer_.Get());
  }
  jint capacity() const {
    return JniEnvExt::Get()->GetDirectBufferCapacity(j_byte_buffer_.Get());
  }
  bool IsNull() const { return !j_byte_buffer_ || !address(); }
  void CopyInto(const void* source, jint count) {
    SB_DCHECK(!IsNull());
    SB_DCHECK(count >= 0 && count <= capacity());
    SbMemoryCopy(address(), source, count);
  }

 private:
  ScopedLocalJavaRef j_byte_buffer_;

  SB_DISALLOW_COPY_AND_ASSIGN(ScopedJavaByteBuffer);
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_JNI_UTILS_H_
