// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  ScopedJavaLocalRef<jstring> path =
      Java_PathUtils_getDataDirectory(env, GetApplicationContext());
  return ConvertJavaStringToUTF8(path);
}

std::string GetCacheDirectory() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> path =
      Java_PathUtils_getCacheDirectory(env, GetApplicationContext());
  return ConvertJavaStringToUTF8(path);
}

std::string GetDownloadsDirectory() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> path =
      Java_PathUtils_getDownloadsDirectory(env, GetApplicationContext());
  return ConvertJavaStringToUTF8(path);
}

std::string GetNativeLibraryDirectory() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> path =
      Java_PathUtils_getNativeLibraryDirectory(env, GetApplicationContext());
  return ConvertJavaStringToUTF8(path);
}

bool RegisterPathUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace base
