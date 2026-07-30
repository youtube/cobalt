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

#ifndef STARBOARD_ANDROID_SHARED_SAFE_JNI_H_
#define STARBOARD_ANDROID_SHARED_SAFE_JNI_H_

#include <array>

#include "starboard/common/log.h"
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/jni_zero/jni_zero.h"

// ============================================================================
// JNI Safety Gatekeeper Philosophy
// ============================================================================
// This library serves as a safe boundary between Starboard C++ and raw Java
// JNI.
//
// 1. Minimize Unsafe JNI: We want to minimize the use of raw, legacy JNI Env
//    calls (like GetObjectArrayElement, GetMethodID) in our main media and
//    platform logic. Raw JNI is highly error-prone, prone to local reference
//    leaks, and difficult to review.
// 2. Strict Isolation: If we absolutely must write raw JNI (because a safe
//    helper does not exist in base::android or jni_zero), we isolate it here.
// 3. Rigorous Audit: Every function in this library must undergo rigorous
//    code review to ensure it is 100% leak-free, uses RAII (ScopedJavaLocalRef)
//    correctly, and handles type/narrowing conversions safely.
// 4. Mandated Reuse: All engineers should ALWAYS use the safe, high-level C++
//    APIs exposed in this file instead of writing raw JNI in their own files.
// ============================================================================

namespace jni_zero {
template <>
inline std::array<float, 16> FromJniArray<std::array<float, 16>>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  jfloatArray j_array = static_cast<jfloatArray>(j_object.obj());
  std::array<float, 16> ret;
  env->GetFloatArrayRegion(j_array, /*start=*/0, /*len=*/16,
                           reinterpret_cast<jfloat*>(ret.data()));
  return ret;
}
}  // namespace jni_zero

namespace starboard {

inline void SetFloatArrayRegion(JNIEnv* env,
                                const jni_zero::JavaRef<jfloatArray>& array,
                                jsize start,
                                jsize len,
                                const jfloat* buf) {
  SB_CHECK(env);
  SB_CHECK(!array.is_null());
  env->SetFloatArrayRegion(array.obj(), start, len, buf);
}

inline void SetByteArrayRegion(JNIEnv* env,
                               const jni_zero::JavaRef<jbyteArray>& array,
                               jsize start,
                               jsize len,
                               const jbyte* buf) {
  SB_CHECK(env);
  SB_CHECK(!array.is_null());
  env->SetByteArrayRegion(array.obj(), start, len, buf);
}

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_SAFE_JNI_H_
