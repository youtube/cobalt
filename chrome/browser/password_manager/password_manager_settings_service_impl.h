// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SETTINGS_SERVICE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SETTINGS_SERVICE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"

class PrefService;

// Service responsible for responding to password manager settings queries
// for all platforms except Android when UPM is enabled.
class PasswordManagerSettingsServiceImpl
    : public PasswordManagerSettingsService {
 public:
  explicit PasswordManagerSettingsServiceImpl(PrefService* pref_service);
  PasswordManagerSettingsServiceImpl(
      const PasswordManagerSettingsServiceImpl&) = delete;
  PasswordManagerSettingsServiceImpl(PasswordManagerSettingsServiceImpl&&) =
      delete;
  PasswordManagerSettingsServiceImpl& operator=(
      const PasswordManagerSettingsServiceImpl&) = delete;
  PasswordManagerSettingsServiceImpl& operator=(
      const PasswordManagerSettingsServiceImpl&&) = delete;
  ~PasswordManagerSettingsServiceImpl() override = default;

  bool IsSettingEnabled(
      password_manager::PasswordManagerSetting setting) const override;
  void TurnOffAutoSignIn() override;

  void RequestSettingsFromBackend() override;

 private:
  raw_ptr<PrefService> pref_service_ = nullptr;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_SERVICE_IMPL_H_
