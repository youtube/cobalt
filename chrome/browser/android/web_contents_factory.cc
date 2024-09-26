// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/WebContentsFactory_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/web_contents.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jobject> JNI_WebContentsFactory_CreateWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jboolean initially_hidden,
    jboolean initialize_renderer,
    const JavaParamRef<jthrowable>& j_creator_location) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  content::WebContents::CreateParams params(profile);
  params.initially_hidden = static_cast<bool>(initially_hidden);
  params.desired_renderer_state =
      static_cast<bool>(initialize_renderer)
          ? content::WebContents::CreateParams::
                kInitializeAndWarmupRendererProcess
          : content::WebContents::CreateParams::kOkayToHaveRendererProcess;
  params.java_creator_location = j_creator_location;

  // Ownership is passed into java, and then to TabAndroid::InitWebContents.
  return content::WebContents::Create(params).release()->GetJavaWebContents();
}
