// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sensitive_content/android/android_sensitive_content_client.h"

#include "base/android/jni_android.h"
#include "base/notreached.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/sensitive_content/jni_headers/SensitiveContentClient_jni.h"

namespace sensitive_content {

using base::android::ScopedJavaLocalRef;

AndroidSensitiveContentClient::AndroidSensitiveContentClient(
    content::WebContents* web_contents,
    std::string histogram_prefix)
    : content::WebContentsUserData<AndroidSensitiveContentClient>(
          *web_contents),
      manager_(web_contents, this),
      histogram_prefix_(std::move(histogram_prefix)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_sensitive_content_client_ = Java_SensitiveContentClient_Constructor(
      env, web_contents->GetJavaWebContents());
  if (web_contents->GetNativeView()) {
    web_contents->GetNativeView()->AddObserver(this);
  }
}

AndroidSensitiveContentClient::~AndroidSensitiveContentClient() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SensitiveContentClient_destroy(env, java_sensitive_content_client_);
}

base::android::JavaRef<jobject>&
AndroidSensitiveContentClient::GetJavaObject() {
  return java_sensitive_content_client_;
}

void AndroidSensitiveContentClient::SetContentSensitivity(
    bool content_is_sensitive) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SensitiveContentClient_setContentSensitivity(
      env, java_sensitive_content_client_, content_is_sensitive);
}

std::string_view AndroidSensitiveContentClient::GetHistogramPrefix() {
  return histogram_prefix_;
}

void AndroidSensitiveContentClient::OnDelegateSet() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SensitiveContentClient_onViewAndroidDelegateSet(
      env, java_sensitive_content_client_);
}

static ScopedJavaLocalRef<jobject>
JNI_SensitiveContentClient_GetJavaSensitiveContentClientFromWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  AndroidSensitiveContentClient* android_sensitive_content_client =
      AndroidSensitiveContentClient::FromWebContents(
          content::WebContents::FromJavaWebContents(java_web_contents));
  return ScopedJavaLocalRef<jobject>(
      android_sensitive_content_client->GetJavaObject());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AndroidSensitiveContentClient);

}  // namespace sensitive_content
