// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_BYTE_BUFFER_HELPER_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_BYTE_BUFFER_HELPER_H_

#include <jni.h>
#include <stdint.h>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"

namespace payments {
namespace android {

// Converts a java.nio.ByteBuffer into a vector of bytes. Sample usage:
//
//  mojom::PaymentDetailsPtr details;
//  bool success = mojom::PaymentDetails::Deserialize(
//      JavaByteBufferToNativeByteVector(env, byte_buffer), &details);
std::vector<uint8_t> JavaByteBufferToNativeByteVector(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& buffer);

// Deserializes a java.nio.ByteBuffer into a native Mojo object. Returns true if
// deserialization is successful.
template <typename T>
bool DeserializeFromJavaByteBuffer(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jbuffer,
    mojo::StructPtr<T>* out) {
  DCHECK(out);
  return T::Deserialize(JavaByteBufferToNativeByteVector(env, jbuffer), out);
}

// Deserializes a java.nio.ByteBuffer[] into a vector of native Mojo objects.
// The content of |out| is replaced. Returns true if all entries are
// deserialized successfully.
template <typename T>
bool DeserializeFromJavaByteBufferArray(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& jbuffers,
    std::vector<mojo::StructPtr<T>>* out) {
  DCHECK(out);
  out->clear();
  for (const auto& jbuffer : jbuffers.ReadElements<jobject>()) {
    mojo::StructPtr<T> data;
    if (!DeserializeFromJavaByteBuffer(env, jbuffer, &data)) {
      out->clear();
      return false;
    }
    out->push_back(std::move(data));
  }
  return true;
}

// Serializes a vector of native Mojo objects into a Java byte[][].
template <typename T>
base::android::ScopedJavaLocalRef<jobjectArray>
SerializeToJavaArrayOfByteArrays(JNIEnv* env,
                                 const std::vector<mojo::StructPtr<T>>& input) {
  std::vector<std::vector<uint8_t>> serialized_elements(input.size());
  for (size_t i = 0; i < input.size(); i++) {
    serialized_elements[i] = T::Serialize(&input[i]);
  }
  return base::android::ToJavaArrayOfByteArray(env, serialized_elements);
}

}  // namespace android
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_BYTE_BUFFER_HELPER_H_
