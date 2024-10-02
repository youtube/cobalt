// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/chrome_jni_headers/ReparentingTask_jni.h"
#include "chrome/browser/android/background_tab_manager.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

using base::android::JavaParamRef;
using chrome::android::BackgroundTabManager;
using content::WebContents;

static void JNI_ReparentingTask_AttachTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  auto* background_tab_manager = BackgroundTabManager::GetInstance();
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  if (background_tab_manager->IsBackgroundTab(web_contents)) {
    Profile* profile = background_tab_manager->GetProfile();
    background_tab_manager->CommitHistory(HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::IMPLICIT_ACCESS));
    background_tab_manager->UnregisterBackgroundTab();
  }
}
