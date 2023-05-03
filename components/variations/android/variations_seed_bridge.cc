// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/android/variations_seed_bridge.h"

#include <jni.h>
#include <stdint.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "jni/VariationsSeedBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {

std::string JNI_VariationsSeedBridge_JavaByteArrayToString(
    JNIEnv* env,
    jbyteArray byte_array) {
  if (!byte_array)
    return std::string();
  std::vector<uint8_t> array_data;
  base::android::JavaByteArrayToByteVector(env, byte_array, &array_data);
  return std::string(array_data.begin(), array_data.end());
}

ScopedJavaLocalRef<jbyteArray> JNI_VariationsSeedBridge_StringToJavaByteArray(
    JNIEnv* env,
    const std::string& str_data) {
  std::vector<uint8_t> array_data(str_data.begin(), str_data.end());
  return base::android::ToJavaByteArray(env, array_data);
}

}  // namespace

namespace variations {
namespace android {

std::unique_ptr<variations::SeedResponse> GetVariationsFirstRunSeed() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_seed_data =
      Java_VariationsSeedBridge_getVariationsFirstRunSeedData(env);
  ScopedJavaLocalRef<jstring> j_seed_signature =
      Java_VariationsSeedBridge_getVariationsFirstRunSeedSignature(env);
  ScopedJavaLocalRef<jstring> j_seed_country =
      Java_VariationsSeedBridge_getVariationsFirstRunSeedCountry(env);
  ScopedJavaLocalRef<jstring> j_response_date =
      Java_VariationsSeedBridge_getVariationsFirstRunSeedDate(env);
  jboolean j_is_gzip_compressed =
      Java_VariationsSeedBridge_getVariationsFirstRunSeedIsGzipCompressed(env);

  auto seed = std::make_unique<variations::SeedResponse>();
  seed->data =
      JNI_VariationsSeedBridge_JavaByteArrayToString(env, j_seed_data.obj());
  seed->signature = ConvertJavaStringToUTF8(j_seed_signature);
  seed->country = ConvertJavaStringToUTF8(j_seed_country);
  seed->date = ConvertJavaStringToUTF8(j_response_date);
  seed->is_gzip_compressed = static_cast<bool>(j_is_gzip_compressed);
  return seed;
}

void ClearJavaFirstRunPrefs() {
  JNIEnv* env = AttachCurrentThread();
  Java_VariationsSeedBridge_clearFirstRunPrefs(env);
}

void MarkVariationsSeedAsStored() {
  JNIEnv* env = AttachCurrentThread();
  Java_VariationsSeedBridge_markVariationsSeedAsStored(env);
}

void SetJavaFirstRunPrefsForTesting(const std::string& seed_data,
                                    const std::string& seed_signature,
                                    const std::string& seed_country,
                                    const std::string& response_date,
                                    bool is_gzip_compressed) {
  JNIEnv* env = AttachCurrentThread();
  Java_VariationsSeedBridge_setVariationsFirstRunSeed(
      env, JNI_VariationsSeedBridge_StringToJavaByteArray(env, seed_data),
      ConvertUTF8ToJavaString(env, seed_signature),
      ConvertUTF8ToJavaString(env, seed_country),
      ConvertUTF8ToJavaString(env, response_date),
      static_cast<jboolean>(is_gzip_compressed));
}

}  // namespace android
}  // namespace variations
