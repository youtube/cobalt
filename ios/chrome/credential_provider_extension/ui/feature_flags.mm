// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_field_trial_version.h"
#import "ios/chrome/common/credential_provider/constants.h"

BOOL IsAutomaticPasskeyUpgradeEnabled() {
  return [[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled()]
      boolValue];
}

BOOL IsPasskeyPRFEnabled() {
  return [[app_group::GetGroupUserDefaults()
      objectForKey:AppGroupUserDefaulsCredentialProviderPasskeyPRFEnabled()]
      boolValue];
}

BOOL IsPasswordCreationUserEnabled() {
  return [[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaultsCredentialProviderSavingPasswordsEnabled()]
      boolValue];
}

BOOL IsPasswordCreationManaged() {
  return [[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaultsCredentialProviderSavingPasswordsManaged()]
      boolValue];
}

BOOL IsPasswordSyncEnabled() {
  return [[app_group::GetGroupUserDefaults()
      objectForKey:AppGroupUserDefaultsCredentialProviderPasswordSyncSetting()]
      boolValue];
}

BOOL IsPasskeyCreationAllowedByPolicy() {
  return [[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaultsCredentialProviderSavingPasskeysEnabled()]
      boolValue];
}

BOOL IsPasskeysM2Enabled() {
  return [[app_group::GetGroupUserDefaults()
      objectForKey:AppGroupUserDefaultsCredentialProviderPasskeysM2Enabled()]
      boolValue];
}
