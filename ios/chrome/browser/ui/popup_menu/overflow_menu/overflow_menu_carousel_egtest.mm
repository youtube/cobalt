// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/sync/base/features.h"
#import "ios/chrome/browser/metrics/metrics_app_interface.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/whats_new/constants.h"
#import "ios/chrome/browser/ui/whats_new/feature_flags.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kOverflowMenuSkipTestMessage =
    @"Can only test Overflow Menu destinations where the feature is supported";

NSString* const kPassphrase = @"hello";

// Get the accessibility ID of a destination with an error badge.
NSString* GetDestinationIdWithErrorBadge(NSString* identifier) {
  return [@[ identifier, @"errorBadge" ] componentsJoinedByString:@"-"];
}

// Get the accessibility ID of a destination with a promo badge.
NSString* GetDestinationIdWithPromoBadge(NSString* identifier) {
  return [@[ identifier, @"promoBadge" ] componentsJoinedByString:@"-"];
}

// Get the accessibility ID of a destination with a "New" badge.
NSString* GetDestinationIdWithNewBadge(NSString* identifier) {
  return [@[ identifier, @"newBadge" ] componentsJoinedByString:@"-"];
}

// Get the matcher for the Settings destination with a promo badge.
id<GREYMatcher> GetSettingsDestinationWithPromoBadgeMatcher() {
  return grey_accessibilityID(
      GetDestinationIdWithPromoBadge(kToolsMenuSettingsId));
}

// Get the matcher for the Settings destination with an error badge.
id<GREYMatcher> GetSettingsDestinationWithErrorBadgeMatcher() {
  return grey_accessibilityID(
      GetDestinationIdWithErrorBadge(kToolsMenuSettingsId));
}

// Get the matcher for the What's New destination with a "New" badge.
id<GREYMatcher> GetWhatsNewDestinationWithNewBadgeMatcher() {
  return grey_accessibilityID(
      GetDestinationIdWithNewBadge(kToolsMenuWhatsNewId));
}

// Cleans up the data related to the destination badge highlight features, e.g.,
// to highlight What's New with a badge.
void CleanupDestinationsHighlightFeaturesData() {
  // Clean up Overflow Menu usage history ranking data.
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kOverflowMenuDestinationUsageHistory];
  [ChromeEarlGrey
      resetDataForLocalStatePref:prefs::kOverflowMenuNewDestinations];

  // Clean up What's New destination promo data.
  [ChromeEarlGrey removeUserDefaultObjectForKey:kWhatsNewUsageEntryKey];
}

// Resolves the passphrase error from the Overflow Menu.
void ResolvePassphraseErrorFromOverflowMenu() {
  // Tap on the Settings destination that has an error badge.
  [[EarlGrey
      selectElementWithMatcher:GetSettingsDestinationWithErrorBadgeMatcher()]
      performAction:grey_tap()];

  // Enter passphrase to resolve the identity error.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON))]
      performAction:grey_tap()];
  [SigninEarlGreyUI submitSyncPassphrase:kPassphrase];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

}  // namespace

// Tests for the Overflow Menu carousel features that don't directly touch the
// destination usage history ranking API.
@interface OverflowMenuTestCase : ChromeTestCase
@end

@implementation OverflowMenuTestCase

#pragma mark - ChromeTestCase overrides

- (void)setUp {
  [super setUp];

  CleanupDestinationsHighlightFeaturesData();

  [ChromeEarlGrey
      waitForSyncEngineInitialized:NO
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
}

- (void)tearDown {
  // Clean up sign-in and Sync data.
  [SigninEarlGrey signOut];
  [ChromeEarlGrey
      waitForSyncEngineInitialized:NO
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
  [ChromeEarlGrey clearSyncServerData];

  CleanupDestinationsHighlightFeaturesData();

  [super tearDown];
}

#pragma mark - Tests

// Tests, in Butter mode, that the Settings destination is highlighted with an
// error badge when there is an identity error, and that the error badge is
// dismissed when the error is resolved.
//
// Emulates a passphrase error in the signed in account to trigger the identity
// error indicators (in butter mode, no Sync).
- (void)testSettingsDestinationWithIdentityErrorInButterMode {
  if (![ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_SKIPPED(kOverflowMenuSkipTestMessage)
  }

  AppLaunchConfiguration config;
  // Enable Overflow Menu identity error indicators.
  config.features_enabled.push_back(kIndicateSyncErrorInOverflowMenu);
  config.features_enabled.push_back(
      syncer::kIndicateAccountStorageErrorInAccountCell);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Encrypt synced data with a passphrase to enable passphrase encryption for
  // the signed in account.
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];

  // Sign in in butter mode while keeping sync disabled.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  // Verify that the error badge is shown.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey
      selectElementWithMatcher:GetSettingsDestinationWithErrorBadgeMatcher()]
      assertWithMatcher:grey_notNil()];

  ResolvePassphraseErrorFromOverflowMenu();

  // Verify that the promo badge is shown once the identity error is resolved
  // and the error badge dismissed.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsDestinationButton()]
      assertWithMatcher:grey_notNil()];
}

// Tests that the Sync error is indicated on the Settings destination (without
// Butter).
//
// Emulates a passphrase error in the synced account to trigger the error
// indicator.
- (void)testSettingsDestinationIdentityErrorBadgeWithSync {
  if (![ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_SKIPPED(kOverflowMenuSkipTestMessage)
  }

  AppLaunchConfiguration config;
  // Enable Overflow Menu indicators.
  config.features_enabled.push_back(kIndicateSyncErrorInOverflowMenu);
  config.features_enabled.push_back(
      syncer::kIndicateAccountStorageErrorInAccountCell);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Encrypt synced data with a passphrase to enable passphrase encryption for
  // the signed in account.
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];

  // Sign in and Sync account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Verifies that the error badge is shown.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey
      selectElementWithMatcher:GetSettingsDestinationWithErrorBadgeMatcher()]
      assertWithMatcher:grey_notNil()];
}

// Tests non-error destination highlights.
// TODO(crbug.com/1431012): This test is very flaky. Fails especially on
// devices.
- (void)FLAKY_testNonErrorDestinationHighlights {
  if (![ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_SKIPPED(kOverflowMenuSkipTestMessage)
  }

  AppLaunchConfiguration config;
  // Enable Overflow Menu destinations highlight features.
  config.features_enabled.push_back(kWhatsNewIOS);
  config.additional_args.push_back(
      "--enable-features=IPH_DemoMode:chosen_feature"
      "/IPH_iOSDefaultBrowserOverflowMenuBadge");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Wait to make sure that the page had the time to load after the browser
  // loaded. The test would be flaky otherwise.
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  [ChromeEarlGreyUI openToolsMenu];

  // Verifies that both destinations are there with the right badge.
  [[EarlGrey
      selectElementWithMatcher:GetSettingsDestinationWithPromoBadgeMatcher()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:GetWhatsNewDestinationWithNewBadgeMatcher()]
      assertWithMatcher:grey_notNil()];
}

@end
