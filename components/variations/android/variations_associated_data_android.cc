// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_string.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_http_header_provider.h"
#include "jni/VariationsAssociatedData_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace variations {
namespace android {

ScopedJavaLocalRef<jstring> JNI_VariationsAssociatedData_GetVariationParamValue(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& jtrial_name,
    const JavaParamRef<jstring>& jparam_name) {
  std::string trial_name(ConvertJavaStringToUTF8(env, jtrial_name));
  std::string param_name(ConvertJavaStringToUTF8(env, jparam_name));
  std::string param_value =
      variations::GetVariationParamValue(trial_name, param_name);
  return ConvertUTF8ToJavaString(env, param_value);
}

ScopedJavaLocalRef<jstring> JNI_VariationsAssociatedData_GetFeedbackVariations(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  const std::string values =
      VariationsHttpHeaderProvider::GetInstance()->GetVariationsString();
  return ConvertUTF8ToJavaString(env, values);
}

}  // namespace android
}  // namespace variations
