// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/locale_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "jni/locale_utils_jni.h"
#include "unicode/uloc.h"

namespace base {
namespace android {

std::string GetDefaultLocale() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> locale = Java_LocaleUtils_getDefaultLocale(env);
  return ConvertJavaStringToUTF8(locale);
}

namespace {

// Common prototype of ICU uloc_getXXX() functions.
typedef int32_t (*UlocGetComponentFunc)(const char*, char*, int32_t,
                                        UErrorCode*);

std::string GetLocaleComponent(const std::string& locale,
                               UlocGetComponentFunc uloc_func,
                               int32_t max_capacity) {
  std::string result;
  UErrorCode error = U_ZERO_ERROR;
  int32_t actual_length = uloc_func(locale.c_str(),
                                    WriteInto(&result, max_capacity),
                                    max_capacity,
                                    &error);
  DCHECK(U_SUCCESS(error));
  DCHECK(actual_length < max_capacity);
  result.resize(actual_length);
  return result;
}

ScopedJavaLocalRef<jobject> NewJavaLocale(
    JNIEnv* env,
    ScopedJavaLocalRef<jclass> locale_class,
    jmethodID constructor_id,
    const std::string& locale) {
  // TODO(wangxianzhu): Use new Locale API once Android supports scripts.
  std::string language = GetLocaleComponent(
      locale, uloc_getLanguage, ULOC_LANG_CAPACITY);
  std::string country = GetLocaleComponent(
      locale, uloc_getCountry, ULOC_COUNTRY_CAPACITY);
  std::string variant = GetLocaleComponent(
      locale, uloc_getVariant, ULOC_FULLNAME_CAPACITY);
  return ScopedJavaLocalRef<jobject>(
      env, env->NewObject(
          locale_class.obj(), constructor_id,
          ConvertUTF8ToJavaString(env, language).obj(),
          ConvertUTF8ToJavaString(env, country).obj(),
          ConvertUTF8ToJavaString(env, variant).obj()));
}

}  // namespace

string16 GetDisplayNameForLocale(const std::string& locale,
                                 const std::string& display_locale) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jclass> locale_class = GetClass(env, "java/util/Locale");
  jmethodID constructor_id = GetMethodID(
      env, locale_class, "<init>",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  ScopedJavaLocalRef<jobject> java_locale = NewJavaLocale(
      env, locale_class, constructor_id, locale);
  ScopedJavaLocalRef<jobject> java_display_locale = NewJavaLocale(
      env, locale_class, constructor_id, display_locale);

  jmethodID method_id = GetMethodID(env, locale_class, "getDisplayName",
                                    "(Ljava/util/Locale;)Ljava/lang/String;");
  ScopedJavaLocalRef<jstring> java_result(
      env,
      static_cast<jstring>(env->CallObjectMethod(java_locale.obj(), method_id,
                                                 java_display_locale.obj())));
  return ConvertJavaStringToUTF16(java_result);
}

bool RegisterLocaleUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace base
