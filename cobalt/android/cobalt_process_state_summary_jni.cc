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

#include <algorithm>
#include <vector>

#include "base/android/jni_array.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "cobalt/android/jni_headers/CobaltProcessStateSummary_jni.h"
#pragma clang diagnostic pop

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

}  // namespace cobalt
