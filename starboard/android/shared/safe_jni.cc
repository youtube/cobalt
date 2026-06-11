// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/safe_jni.h"

#include "base/android/jni_array.h"
#include "base/android/jni_bytebuffer.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"

namespace starboard {

using jni_zero::ScopedJavaLocalRef;

std::vector<ScopedJavaLocalRef<jobject>> JavaObjectArrayToVector(
    JNIEnv* env,
    const jni_zero::JavaRef<jobjectArray>& array) {
  SB_CHECK(env);
  if (!array) {
    return {};
  }
  size_t length = base::android::SafeGetArrayLength(env, array);
  std::vector<ScopedJavaLocalRef<jobject>> out(length);
  for (size_t i = 0; i < length; ++i) {
    out[i] = ScopedJavaLocalRef<jobject>(
        env, env->GetObjectArrayElement(array.obj(), static_cast<jsize>(i)));
  }
  return out;
}

Span<uint8_t> JavaByteBufferToSpan(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& byte_buffer) {
  SB_CHECK(env);
  if (!byte_buffer) {
    return {};
  }
  auto span =
      base::android::MaybeJavaByteBufferToMutableSpan(env, byte_buffer.obj());
  if (!span.has_value()) {
    return {};
  }
  return {span->data(), span->size()};
}

}  // namespace starboard
