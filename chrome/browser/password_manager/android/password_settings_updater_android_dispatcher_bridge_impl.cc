// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_settings_updater_android_dispatcher_bridge_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordSettingsUpdaterDispatcherBridge_jni.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_receiver_bridge.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

namespace {

using SyncingAccount =
    PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::SyncingAccount;

base::android::ScopedJavaLocalRef<jstring> GetJavaStringFromAccount(
    absl::optional<SyncingAccount> account) {
  if (!account.has_value()) {
    return nullptr;
  }
  return base::android::ConvertUTF8ToJavaString(
      base::android::AttachCurrentThread(), *account.value());
}

}  // namespace

// static
bool PasswordSettingsUpdaterAndroidDispatcherBridge::CanCreateAccessor() {
  return Java_PasswordSettingsUpdaterDispatcherBridge_canCreateAccessor(
      base::android::AttachCurrentThread());
}

// static
std::unique_ptr<PasswordSettingsUpdaterAndroidDispatcherBridge>
PasswordSettingsUpdaterAndroidDispatcherBridge::Create() {
  DCHECK(Java_PasswordSettingsUpdaterDispatcherBridge_canCreateAccessor(
      base::android::AttachCurrentThread()));
  return std::make_unique<PasswordSettingsUpdaterAndroidDispatcherBridgeImpl>();
}

PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::
    PasswordSettingsUpdaterAndroidDispatcherBridgeImpl() {
  DETACH_FROM_THREAD(thread_checker_);
  DCHECK(Java_PasswordSettingsUpdaterDispatcherBridge_canCreateAccessor(
      base::android::AttachCurrentThread()));
}

PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::
    ~PasswordSettingsUpdaterAndroidDispatcherBridgeImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::Init(
    base::android::ScopedJavaGlobalRef<jobject> receiver_bridge) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  java_object_ = Java_PasswordSettingsUpdaterDispatcherBridge_create(
      base::android::AttachCurrentThread(), receiver_bridge);
}

void PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::
    GetPasswordSettingValue(absl::optional<SyncingAccount> account,
                            PasswordManagerSetting setting) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Java_PasswordSettingsUpdaterDispatcherBridge_getSettingValue(
      base::android::AttachCurrentThread(), java_object_,
      GetJavaStringFromAccount(account), static_cast<int>(setting));
}

void PasswordSettingsUpdaterAndroidDispatcherBridgeImpl::
    SetPasswordSettingValue(absl::optional<SyncingAccount> account,
                            PasswordManagerSetting setting,
                            bool value) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Java_PasswordSettingsUpdaterDispatcherBridge_setSettingValue(
      base::android::AttachCurrentThread(), java_object_,
      GetJavaStringFromAccount(account), static_cast<int>(setting), value);
}

}  // namespace password_manager
