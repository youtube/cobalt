// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_key_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/payments/content/android/browser_binding_jni/BrowserBoundKey_jni.h"
#include "third_party/jni_zero/jni_zero.h"

namespace payments {

BrowserBoundKeyAndroid::BrowserBoundKeyAndroid(
    jni_zero::ScopedJavaLocalRef<jobject> impl)
    : impl_(impl) {}

BrowserBoundKeyAndroid::~BrowserBoundKeyAndroid() = default;

std::vector<uint8_t> BrowserBoundKeyAndroid::Sign(
    const std::vector<uint8_t>& client_data) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jbyteArray> client_data_json_jarray =
      base::android::ToJavaByteArray(env, client_data);
  jni_zero::ScopedJavaLocalRef<jbyteArray> signature_jarray =
      Java_BrowserBoundKey_sign(env, impl_, client_data_json_jarray);
  std::vector<uint8_t> signature_output;
  base::android::AppendJavaByteArrayToByteVector(env, signature_jarray,
                                                 &signature_output);
  return signature_output;
}

std::vector<uint8_t> BrowserBoundKeyAndroid::GetPublicKeyAsCoseKey() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_BrowserBoundKey_getPublicKeyAsCoseKey(env, impl_);
}

}  // namespace payments
