// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/cookie_controls_service_bridge.h"

#include <memory>
#include "chrome/android/chrome_jni_headers/CookieControlsServiceBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"

using base::android::JavaParamRef;

CookieControlsServiceBridge::CookieControlsServiceBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj)
    : jobject_(obj) {}

void CookieControlsServiceBridge::UpdateServiceIfNecessary() {
  // This class is only for the incognito NTP, so it is safe to always use the
  // primary OTR profile.
  Profile* profile = ProfileManager::GetLastUsedProfile()->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
  CookieControlsService* new_service =
      CookieControlsServiceFactory::GetForProfile(profile);
  // Update the service only if it is for a new profile
  if (new_service != service_) {
    service_ = new_service;
    service_->AddObserver(this);
    SendCookieControlsUIChanges();
  }
}

void CookieControlsServiceBridge::HandleCookieControlsToggleChanged(
    JNIEnv* env,
    jboolean checked) {
  UpdateServiceIfNecessary();
  service_->HandleCookieControlsToggleChanged(checked);
}

void CookieControlsServiceBridge::UpdateServiceIfNecessary(JNIEnv* env) {
  UpdateServiceIfNecessary();
}

void CookieControlsServiceBridge::SendCookieControlsUIChanges() {
  bool checked = service_->GetToggleCheckedValue();
  CookieControlsEnforcement enforcement =
      service_->GetCookieControlsEnforcement();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CookieControlsServiceBridge_sendCookieControlsUIChanges(
      env, jobject_, checked, static_cast<int>(enforcement));
}

void CookieControlsServiceBridge::OnThirdPartyCookieBlockingPrefChanged() {
  SendCookieControlsUIChanges();
}

void CookieControlsServiceBridge::OnThirdPartyCookieBlockingPolicyChanged() {
  SendCookieControlsUIChanges();
}

CookieControlsServiceBridge::~CookieControlsServiceBridge() = default;

void CookieControlsServiceBridge::Destroy(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  delete this;
}

static jlong JNI_CookieControlsServiceBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new CookieControlsServiceBridge(env, obj));
}
