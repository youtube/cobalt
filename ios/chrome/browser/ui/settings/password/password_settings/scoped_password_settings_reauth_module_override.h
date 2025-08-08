// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_SCOPED_PASSWORD_SETTINGS_REAUTH_MODULE_OVERRIDE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_SCOPED_PASSWORD_SETTINGS_REAUTH_MODULE_OVERRIDE_H_

#import "base/memory/raw_ptr.h"

@protocol ReauthenticationProtocol;

// Util class enabling a global override of the Reauthentication Module used in
// newly-constructed PasswordSettingsCoordinators, for testing purposes only.
class ScopedPasswordSettingsReauthModuleOverride {
 public:
  ~ScopedPasswordSettingsReauthModuleOverride();

  // Creates a scoped override so that the provided fake/mock/disarmed/etc
  // reauthentication module will be used in place of the production
  // implementation.
  // Newly created coordinators will use `module` as their reauthentication
  // module until the override is destroyed. Any coordinator created while an
  // override is active will hold a strong ref to `module`.
  static std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride>
  MakeAndArmForTesting(id<ReauthenticationProtocol> module);

  // Singleton instance of this class.
  static raw_ptr<ScopedPasswordSettingsReauthModuleOverride> instance;

  // The module to be used.
  id<ReauthenticationProtocol> module;

 private:
  ScopedPasswordSettingsReauthModuleOverride() = default;
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_SCOPED_PASSWORD_SETTINGS_REAUTH_MODULE_OVERRIDE_H_
