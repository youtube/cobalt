// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/tests_hook.h"

#import "base/time/time.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"

namespace tests_hook {

bool DisableAppGroupAccess() {
  return true;
}

bool DisableClientSideFieldTrials() {
  return true;
}

bool DisableContentSuggestions() {
  return true;
}

bool DisableDiscoverFeed() {
  return true;
}

bool DisableDefaultFirstRun() {
  return true;
}

bool DisableDefaultSearchEngineChoice() {
  return true;
}

bool DisableGeolocation() {
  return true;
}

bool DisablePromoManagerFullScreenPromos() {
  return true;
}

bool DisableUpgradeSigninPromo() {
  return true;
}

bool DisableUpdateService() {
  return true;
}

bool DelayAppLaunchPromos() {
  return true;
}

std::unique_ptr<ProfileOAuth2TokenService> GetOverriddenTokenService(
    PrefService* user_prefs,
    std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate) {
  return nullptr;
}

policy::ConfigurationPolicyProvider* GetOverriddenPlatformPolicyProvider() {
  return nullptr;
}

std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager() {
  return nullptr;
}

std::unique_ptr<TrustedVaultClientBackend> CreateTrustedVaultClientBackend() {
  return nullptr;
}

std::unique_ptr<tab_groups::TabGroupSyncService> CreateTabGroupSyncService(
    ProfileIOS* profile) {
  return nullptr;
}

std::unique_ptr<ShareKitService> CreateShareKitService() {
  return nullptr;
}

std::unique_ptr<password_manager::BulkLeakCheckServiceInterface>
GetOverriddenBulkLeakCheckService() {
  return nullptr;
}

std::unique_ptr<plus_addresses::PlusAddressService>
GetOverriddenPlusAddressService() {
  return nullptr;
}

std::unique_ptr<password_manager::RecipientsFetcher>
GetOverriddenRecipientsFetcher() {
  return nullptr;
}

void SetUpTestsIfPresent() {
  // No-op for XCUITest.
}

void RunTestsIfPresent() {
  // No-op for XCUITest.
}

void SignalAppLaunched() {
  // No-op for XCUITest.
}

base::TimeDelta PasswordCheckMinimumDuration() {
  // No artificial delays for tests.
  return base::Seconds(0);
}

base::TimeDelta GetOverriddenSnackbarDuration() {
  return base::Seconds(0);
}

std::unique_ptr<drive::DriveService> GetOverriddenDriveService() {
  return nullptr;
}

std::optional<std::string> FETDemoModeOverride() {
  return std::nullopt;
}

void WipeProfileIfRequested(int argc, char* argv[]) {
  // Do nothing.
}

}  // namespace tests_hook
