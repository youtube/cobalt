// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"

namespace base {
namespace android {

std::string ConvertJavaStringToUTF8(JNIEnv* env, jstring str) {
  // JNI's GetStringUTFChars() returns strings in Java-modified UTF8, so we
  // instead get the String in UTF16 and convert using our own utility function.
  return UTF16ToUTF8(ConvertJavaStringToUTF16(env, str));
}

jstring ConvertUTF8ToJavaString(JNIEnv* env, const std::string& str) {
  jstring result = env->NewStringUTF(str.c_str());
  CheckException(env);
  return result;
}

string16 ConvertJavaStringToUTF16(JNIEnv* env, jstring str) {
  const jchar* chars = env->GetStringChars(str, NULL);
  DCHECK(chars);
  // GetStringChars isn't required to NULL-terminate the strings
  // it returns, so the length must be explicitly checked.
  string16 result(chars, env->GetStringLength(str));
  env->ReleaseStringChars(str, chars);
  CheckException(env);
  return result;
}

jstring ConvertUTF16ToJavaString(JNIEnv* env, const string16& str) {
  jstring result = env->NewString(str.data(), str.length());
  CheckException(env);
  return result;
}

}  // namespace android
}  // namespace base
