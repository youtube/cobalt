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

#include "cobalt/android/cobalt_process_state_summary_jni.h"

#include <algorithm>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "cobalt/android/jni_headers/CobaltProcessStateSummary_jni.h"

namespace cobalt {

static jint JNI_CobaltProcessStateSummary_ComputePersistentHash(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& j_data,
    jint length) {
  if (!j_data || length <= 0) {
    return 0;
  }
  std::vector<uint8_t> buffer;
  base::android::JavaByteArrayToByteVector(env, j_data, &buffer);
  size_t len = std::min(static_cast<size_t>(length), buffer.size());
  return static_cast<jint>(
      base::PersistentHash(base::span<const uint8_t>(buffer.data(), len)));
}

void CobaltProcessStateSummarySyncToSystemServer(
    const std::vector<uint8_t>& payload) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> java_array =
      base::android::ToJavaByteArray(env, payload.data(), payload.size());

  Java_CobaltProcessStateSummary_setProcessStateSummary(env, java_array);
}

std::optional<std::vector<uint8_t>>
CobaltProcessStateSummaryGetPriorSessionSummary() {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return std::nullopt;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> java_bytes =
      Java_CobaltProcessStateSummary_getPriorSessionProcessStateSummary(env);

  if (!java_bytes) {
    return std::nullopt;
  }

  std::vector<uint8_t> buffer;
  base::android::JavaByteArrayToByteVector(env, java_bytes, &buffer);
  return buffer;
}

int CobaltProcessStateSummaryRecordLatestExitReasonToUma(
    const std::string& uma_name) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return -1;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CobaltProcessStateSummary_recordLatestExitReasonToUma(
      env, base::android::ConvertUTF8ToJavaString(env, uma_name));
}

std::optional<std::vector<uint8_t>>
CobaltProcessStateSummaryRecordLatestExitReasonAndGetSummary(
    const std::string& uma_name,
    int* out_exit_reason) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    if (out_exit_reason) {
      *out_exit_reason = -1;
    }
    return std::nullopt;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<int> exit_reason_vec = {-1};
  base::android::ScopedJavaLocalRef<jintArray> out_array =
      base::android::ToJavaIntArray(env, exit_reason_vec);

  base::android::ScopedJavaLocalRef<jbyteArray> java_bytes =
      Java_CobaltProcessStateSummary_recordLatestExitReasonAndGetSummary(
          env, base::android::ConvertUTF8ToJavaString(env, uma_name),
          out_array);

  int exit_reason = -1;
  std::vector<int> c_array;
  base::android::JavaIntArrayToIntVector(env, out_array, &c_array);
  if (!c_array.empty()) {
    exit_reason = c_array[0];
  }
  if (out_exit_reason) {
    *out_exit_reason = exit_reason;
  }

  if (!java_bytes) {
    return std::nullopt;
  }

  std::vector<uint8_t> buffer;
  base::android::JavaByteArrayToByteVector(env, java_bytes, &buffer);
  return buffer;
}

}  // namespace cobalt
