// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/path_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"

#include "jni/path_utils_jni.h"

namespace base {
namespace android {

std::string GetDataDirectory() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> path(env, Java_PathUtils_getDataDirectory(
      env, base::android::GetApplicationContext()));
  return base::android::ConvertJavaStringToUTF8(env, path.obj());
}

std::string GetCacheDirectory() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> path(env, Java_PathUtils_getCacheDirectory(
      env, base::android::GetApplicationContext()));
  return base::android::ConvertJavaStringToUTF8(env, path.obj());
}

bool RegisterPathUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace base
