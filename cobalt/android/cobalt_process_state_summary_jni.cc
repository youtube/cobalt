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

#include <optional>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "cobalt/android/app_event_bridge_jni/CobaltProcessStateSummary_jni.h"

namespace cobalt {
namespace android {

jint JNI_CobaltProcessStateSummary_ComputePersistentHash(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& j_data,
    jint length) {
  if (!j_data || length <= 0) {
    return 0;
  }
  std::vector<uint8_t> data;
  base::android::JavaByteArrayToByteVector(env, j_data, &data);
  if (data.empty() || static_cast<size_t>(length) > data.size()) {
    return 0;
  }
  return static_cast<jint>(
      base::PersistentHash(base::span(data).subspan(0, length)));
}

void SetProcessStateSummary(base::span<const uint8_t> summary_bytes) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> j_bytes =
      base::android::ToJavaByteArray(env, summary_bytes.data(),
                                     summary_bytes.size());
  Java_CobaltProcessStateSummary_setProcessStateSummary(env, j_bytes);
}

std::optional<std::vector<uint8_t>> GetPriorSessionProcessStateSummary() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> j_bytes =
      Java_CobaltProcessStateSummary_getPriorSessionProcessStateSummary(env);
  if (!j_bytes) {
    return std::nullopt;
  }
  std::vector<uint8_t> result;
  base::android::JavaByteArrayToByteVector(env, j_bytes, &result);
  return result;
}

std::optional<std::vector<uint8_t>> RecordLatestExitReasonAndGetSummary(
    const std::string& uma_name,
    int* out_exit_reason) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_uma_name =
      base::android::ConvertUTF8ToJavaString(env, uma_name);

  base::android::ScopedJavaLocalRef<jintArray> j_out_array;
  if (out_exit_reason) {
    int initial_val[1] = {-1};
    j_out_array = base::android::ToJavaIntArray(env, initial_val, 1);
  }

  base::android::ScopedJavaLocalRef<jbyteArray> j_bytes =
      Java_CobaltProcessStateSummary_recordLatestExitReasonAndGetSummary(
          env, j_uma_name, j_out_array);

  if (out_exit_reason && j_out_array) {
    std::vector<int> out_vec;
    base::android::JavaIntArrayToIntVector(env, j_out_array, &out_vec);
    if (!out_vec.empty() && out_vec[0] >= 0) {
      *out_exit_reason = out_vec[0];
    } else {
      *out_exit_reason = -1;
    }
  }

  if (!j_bytes) {
    return std::nullopt;
  }
  std::vector<uint8_t> result;
  base::android::JavaByteArrayToByteVector(env, j_bytes, &result);
  return result;
}

}  // namespace android
}  // namespace cobalt
