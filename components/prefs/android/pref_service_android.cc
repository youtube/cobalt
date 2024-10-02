// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/android/pref_service_android.h"

#include <string>

#include "components/prefs/android/jni_headers/PrefService_jni.h"
#include "components/prefs/pref_service.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

PrefServiceAndroid::PrefServiceAndroid(PrefService* pref_service)
    : pref_service_(pref_service) {}

PrefServiceAndroid::~PrefServiceAndroid() {
  if (java_ref_) {
    Java_PrefService_clearNativePtr(base::android::AttachCurrentThread(),
                                    java_ref_);
    java_ref_.Reset();
  }
}

ScopedJavaLocalRef<jobject> PrefServiceAndroid::GetJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_ref_) {
    java_ref_.Reset(
        Java_PrefService_create(env, reinterpret_cast<intptr_t>(this)));
  }
  return ScopedJavaLocalRef<jobject>(java_ref_);
}

void PrefServiceAndroid::ClearPref(JNIEnv* env,
                                   const JavaParamRef<jstring>& j_preference) {
  pref_service_->ClearPref(
      base::android::ConvertJavaStringToUTF8(env, j_preference));
}

jboolean PrefServiceAndroid::HasPrefPath(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_preference) {
  return pref_service_->HasPrefPath(
      base::android::ConvertJavaStringToUTF8(env, j_preference));
}

jboolean PrefServiceAndroid::GetBoolean(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_preference) {
  return pref_service_->GetBoolean(
      base::android::ConvertJavaStringToUTF8(env, j_preference));
}

void PrefServiceAndroid::SetBoolean(JNIEnv* env,
                                    const JavaParamRef<jstring>& j_preference,
                                    const jboolean j_value) {
  pref_service_->SetBoolean(
      base::android::ConvertJavaStringToUTF8(env, j_preference), j_value);
}

jint PrefServiceAndroid::GetInteger(JNIEnv* env,
                                    const JavaParamRef<jstring>& j_preference) {
  return pref_service_->GetInteger(
      base::android::ConvertJavaStringToUTF8(env, j_preference));
}

void PrefServiceAndroid::SetInteger(JNIEnv* env,
                                    const JavaParamRef<jstring>& j_preference,
                                    const jint j_value) {
  pref_service_->SetInteger(
      base::android::ConvertJavaStringToUTF8(env, j_preference), j_value);
}

ScopedJavaLocalRef<jstring> PrefServiceAndroid::GetString(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_preference) {
  return base::android::ConvertUTF8ToJavaString(
      env, pref_service_->GetString(
               base::android::ConvertJavaStringToUTF8(env, j_preference)));
}

void PrefServiceAndroid::SetString(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_preference,
    const base::android::JavaParamRef<jstring>& j_value) {
  pref_service_->SetString(
      base::android::ConvertJavaStringToUTF8(env, j_preference),
      base::android::ConvertJavaStringToUTF8(env, j_value));
}

jboolean PrefServiceAndroid::IsManagedPreference(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_preference) {
  return pref_service_->IsManagedPreference(
      base::android::ConvertJavaStringToUTF8(env, j_preference));
}

jboolean PrefServiceAndroid::IsDefaultValuePreference(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_preference) {
  const PrefService::Preference* pref = pref_service_->FindPreference(
      base::android::ConvertJavaStringToUTF8(env, j_preference));
  return pref && pref->IsDefaultValue();
}
