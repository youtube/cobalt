// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/tests_hook.h"

#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#import "ios/chrome/test/wpt/cwt_constants.h"
#import "ios/chrome/test/wpt/cwt_webdriver_app_interface.h"
#import "ios/third_party/edo/src/Service/Sources/EDOHostService.h"

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
bool DisableFirstRun() {
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
bool DisableMainThreadFreezeDetection() {
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
std::unique_ptr<password_manager::BulkLeakCheckServiceInterface>
GetOverriddenBulkLeakCheckService() {
  return nullptr;
}
std::unique_ptr<password_manager::RecipientsFetcher>
GetOverriddenRecipientsFetcher() {
  return nullptr;
}
void SetUpTestsIfPresent() {
  CWTWebDriverAppInterface* appInterface =
      [[CWTWebDriverAppInterface alloc] init];
  [EDOHostService serviceWithPort:kCwtEdoPortNumber
                       rootObject:appInterface
                            queue:[appInterface executingQueue]];
}

void RunTestsIfPresent() {}

}  // namespace tests_hook
