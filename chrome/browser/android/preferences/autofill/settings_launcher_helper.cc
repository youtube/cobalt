// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/autofill/settings_launcher_helper.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/SettingsLauncherHelper_jni.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

void ShowAutofillProfileSettings(content::WebContents* web_contents) {
  Java_SettingsLauncherHelper_showAutofillProfileSettings(
      base::android::AttachCurrentThread(), web_contents->GetJavaWebContents());
}

void ShowAutofillCreditCardSettings(content::WebContents* web_contents) {
  Java_SettingsLauncherHelper_showAutofillCreditCardSettings(
      base::android::AttachCurrentThread(), web_contents->GetJavaWebContents());
}

}  // namespace autofill
