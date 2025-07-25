// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/byte_buffer_helper.h"

#include <cstring>
#include <utility>
#include <vector>

namespace payments {
namespace android {

std::vector<uint8_t> JavaByteBufferToNativeByteVector(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& buffer) {
  jbyte* buf_in =
      static_cast<jbyte*>(env->GetDirectBufferAddress(buffer.obj()));
  jlong buf_size = env->GetDirectBufferCapacity(buffer.obj());
  std::vector<uint8_t> result(buf_size);
  memcpy(&result[0], buf_in, buf_size);
  return result;
}

}  // namespace android
}  // namespace payments
