// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/build_info.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "jni/build_info_jni.h"

namespace base {
namespace android {

BuildInfo::BuildInfo() {
  JNIEnv* env = AttachCurrentThread();

  // The const char* pointers initialized below will be owned by the
  // resultant BuildInfo.
  std::string device_str =
      ConvertJavaStringToUTF8(Java_BuildInfo_getDevice(env));
  device_ = strdup(device_str.c_str());

  std::string model_str =
      ConvertJavaStringToUTF8(Java_BuildInfo_getDeviceModel(env));
  model_ = strdup(model_str.c_str());

  std::string brand_str =
      ConvertJavaStringToUTF8(Java_BuildInfo_getBrand(env));
  brand_ = strdup(brand_str.c_str());

  std::string android_build_id_str =
      ConvertJavaStringToUTF8(Java_BuildInfo_getAndroidBuildId(env));
  android_build_id_ = strdup(android_build_id_str.c_str());

  std::string android_build_fp_str =
      ConvertJavaStringToUTF8(Java_BuildInfo_getAndroidBuildFingerprint(env));
  android_build_fp_ = strdup(android_build_fp_str.c_str());

  jobject app_context = GetApplicationContext();
  std::string package_version_code_str =
      ConvertJavaStringToUTF8(Java_BuildInfo_getPackageVersionCode(
          env, app_context));
  package_version_code_ = strdup(package_version_code_str.c_str());

  std::string package_version_name_str =
      ConvertJavaStringToUTF8(
          Java_BuildInfo_getPackageVersionName(env, app_context));
  package_version_name_ = strdup(package_version_name_str.c_str());
}

// static
BuildInfo* BuildInfo::GetInstance() {
  return Singleton<BuildInfo, LeakySingletonTraits<BuildInfo> >::get();
}

bool RegisterBuildInfo(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace base
