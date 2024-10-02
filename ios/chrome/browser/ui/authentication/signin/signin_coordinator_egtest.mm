// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/policy/policy_constants.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/metrics/metrics_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/test_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_constants.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BookmarksNavigationBarDoneButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsCollectionView;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsImportDataContinueButton;
using chrome_test_util::SettingsImportDataImportButton;
using chrome_test_util::SettingsImportDataKeepSeparateButton;
using chrome_test_util::SettingsLink;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;
using testing::ButtonWithAccessibilityLabel;

typedef NS_ENUM(NSInteger, OpenSigninMethod) {
  OpenSigninMethodFromSettings,
  OpenSigninMethodFromBookmarks,
  OpenSigninMethodFromRecentTabs,
  OpenSigninMethodFromTabSwitcher,
};

namespace {

// Label used to find the 'Learn more' link.
NSString* const kLearnMoreLabel = @"Learn More";

// Text displayed in the chrome://management page.
char const kManagedText[] = "Your browser is managed by your administrator.";

NSString* const kPassphrase = @"hello";

// Timeout in seconds to wait for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(5);

// Sets parental control capability for the given identity.
void SetParentalControlsCapabilityForIdentity(FakeSystemIdentity* identity) {
  // The identity must exist in the test storage to be able to set capabilities
  // through the fake identity service.
  [SigninEarlGrey addFakeIdentity:identity];

  ios::CapabilitiesDict* capabilities = @{
    @(kIsSubjectToParentalControlsCapabilityName) :
        @(static_cast<int>(SystemIdentityCapabilityResult::kTrue))
  };
  [SigninEarlGrey setCapabilities:capabilities forIdentity:identity];
}
// Closes the sign-in import data dialog and choose either to combine the data
// or keep the data separate.
void CloseImportDataDialog(id<GREYMatcher> choiceButtonMatcher) {
  // Select the import data choice.
  [[EarlGrey selectElementWithMatcher:choiceButtonMatcher]
      performAction:grey_tap()];

  // Close the import data dialog.
  [[EarlGrey selectElementWithMatcher:SettingsImportDataContinueButton()]
      performAction:grey_tap()];
}

// Signs in with one account, signs out, and then signs in with a second
// account.
void ChooseImportOrKeepDataSepareteDialog(id<GREYMatcher> choiceButtonMatcher) {
  // Set up the fake identities.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign in to `fakeIdentity1`.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];

  // Sign in with `fakeIdentity2`.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SecondarySignInButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // Switch Sync account to `fakeIdentity2` should ask whether date should be
  // imported or kept separate. Choose to keep data separate.
  CloseImportDataDialog(choiceButtonMatcher);

  // Check the signed-in user did change.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity2];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

void ExpectSigninConsentHistogram(
    signin_metrics::SigninAccountType signinAccountType) {
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"Signin.AccountType.SigninConsent"];
  GREYAssertNil(error, @"Failed to record show count histogram");
  error = [MetricsAppInterface expectCount:1
                                 forBucket:static_cast<int>(signinAccountType)
                              forHistogram:@"Signin.AccountType.SigninConsent"];
  GREYAssertNil(error, @"Failed to record show count histogram");
}

void ExpectSyncConsentHistogram(
    signin_metrics::SigninAccountType signinAccountType) {
  NSError* error =
      [MetricsAppInterface expectTotalCount:1
                               forHistogram:@"Signin.AccountType.SyncConsent"];
  GREYAssertNil(error, @"Failed to record show count histogram");
  error = [MetricsAppInterface expectCount:1
                                 forBucket:static_cast<int>(signinAccountType)
                              forHistogram:@"Signin.AccountType.SyncConsent"];
  GREYAssertNil(error, @"Failed to record show count histogram");
}

// Sets up the sign-in policy value dynamically at runtime.
void SetSigninEnterprisePolicyValue(BrowserSigninMode signinMode) {
  policy_test_utils::SetPolicy(static_cast<int>(signinMode),
                               policy::key::kBrowserSignin);
}

}  // namespace

// Sign-in interaction tests that work both with Unified Consent enabled or
// disabled.
@interface SigninCoordinatorTestCase : WebHttpServerChromeTestCase
@end

@implementation SigninCoordinatorTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  if ([self
          isRunningTest:@selector
          (testOpenManageSyncSettingsFromNTPWhenSigninIsNotAllowedByPolicy)] ||
      [self isRunningTest:@selector
            (testOpenManageSyncSettingsFromNTPWhenSyncDisabledByPolicy)] ||
      [self isRunningTest:@selector(testOpenSignInFromNTP)]) {
    config.features_enabled.push_back(switches::kIdentityStatusConsistency);
  }
  return config;
}

- (void)setUp {
  [super setUp];
  // Remove closed tab history to make sure the sign-in promo is always visible
  // in recent tabs.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");
}

- (void)tearDown {
  [super tearDown];
  [BookmarkEarlGrey clearBookmarksPositionCache];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
}

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is already an identity on the device.
- (void)testSignInOneUser {
  // Set up a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Check `fakeIdentity` is signed-in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  ExpectSigninConsentHistogram(signin_metrics::SigninAccountType::kRegular);
  ExpectSyncConsentHistogram(signin_metrics::SigninAccountType::kRegular);
}

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is a supervised user identity on the device.
- (void)testSignInSupervisedUser {
  // Set up a fake supervised identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  SetParentalControlsCapabilityForIdentity(fakeIdentity);

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that signing out from an unsupervised account without clearing data
// and then signing in to a supervised account performs a local data clearing
// on sign-in.
- (void)testSwitchToSupervisedUser {
  // Add a fake supervised identity to the device.
  FakeSystemIdentity* fakeSupervisedIdentity =
      [FakeSystemIdentity fakeIdentity1];
  SetParentalControlsCapabilityForIdentity(fakeSupervisedIdentity);

  // Add a fake identity to the device.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in with fake identity.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out with option to keep local data.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];

  // Sign in with fake supervised identity.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeSupervisedIdentity];

  // Verify bookmarks are cleared.
  [BookmarkEarlGreyUI openBookmarks];
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_text(GetNSString(
                                            IDS_IOS_BOOKMARK_EMPTY_TITLE))]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Waiting for bookmarks to be cleared");
}

// Tests that signing out a supervised user account with the keep local data
// option is honored.
- (void)testSignOutWithKeepDataForSupervisedUser {
  // Sign in with a fake supervised identity.
  FakeSystemIdentity* fakeSupervisedIdentity =
      [FakeSystemIdentity fakeIdentity1];
  SetParentalControlsCapabilityForIdentity(fakeSupervisedIdentity);
  [SigninEarlGreyUI signinWithFakeIdentity:fakeSupervisedIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out from the supervised account with option to keep local data.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];

  // Verify bookmarks are available.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests that signing out a supervised user account with the clear local data
// option is honored.
- (void)testSignOutWithClearDataForSupervisedUser {
  // Sign in with a fake supervised identity.
  FakeSystemIdentity* fakeSupervisedIdentity =
      [FakeSystemIdentity fakeIdentity1];
  SetParentalControlsCapabilityForIdentity(fakeSupervisedIdentity);
  [SigninEarlGreyUI signinWithFakeIdentity:fakeSupervisedIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out from the supervised account with option to clear local data.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];

  // Verify bookmarks are cleared.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

// Tests signing in with one account, switching sync account to a second and
// choosing to keep the browsing data separate during the switch.
- (void)testSignInSwitchAccountsAndKeepDataSeparate {
  ChooseImportOrKeepDataSepareteDialog(SettingsImportDataKeepSeparateButton());
}

// Tests signing in with one account, switching sync account to a second and
// choosing to import the browsing data during the switch.
- (void)testSignInSwitchAccountsAndImportData {
  ChooseImportOrKeepDataSepareteDialog(SettingsImportDataImportButton());
}

// Tests that signing out from the Settings works correctly.
- (void)testSignInDisconnectFromChrome {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];
}

// Tests that signing out of a managed account from the Settings works
// correctly.
- (void)testSignInDisconnectFromChromeManaged {
  // Sign-in with a managed account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  ExpectSigninConsentHistogram(signin_metrics::SigninAccountType::kManaged);
  ExpectSyncConsentHistogram(signin_metrics::SigninAccountType::kManaged);

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];
}

// Opens the sign in screen and then cancel it by opening a new tab. Ensures
// that the sign in screen is correctly dismissed. crbug.com/462200
- (void)testSignInCancelIdentityPicker {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  // Waits until the UI is fully presented before opening an URL.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Open new tab to cancel sign-in.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];

  // Verifies that the Chrome sign-in view is visible.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Starts an authentication flow and cancel it by opening a new tab. Ensures
// that the authentication flow is correctly canceled and dismissed.
// crbug.com/462202
- (void)testSignInCancelAuthenticationFlow {
  // Set up the fake identities.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // This signs in `fakeIdentity2` first, ensuring that the "Clear Data Before
  // Syncing" dialog is shown during the second sign-in. This dialog will
  // effectively block the authentication flow, ensuring that the authentication
  // flow is always still running when the sign-in is being cancelled.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity2];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];
  // Sign in with `fakeIdentity1`.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SecondarySignInButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity1.userEmail)]
      performAction:grey_tap()];

  // The authentication flow is only created when the confirm button is
  // selected. Note that authentication flow actually blocks as the
  // "Clear Browsing Before Syncing" dialog is presented.
  [SigninEarlGreyUI tapSigninConfirmationDialog];
  // Waits until the merge/delete data panel is shown.
  [[EarlGrey selectElementWithMatcher:SettingsImportDataKeepSeparateButton()]
      assertWithMatcher:grey_interactable()];

  // Open new tab to cancel sign-in.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity1.userEmail)]
      performAction:grey_tap()];

  // Verifies that the Chrome sign-in view is visible.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];
}

// Opens the sign in screen from the bookmarks and then cancel it by tapping on
// done. Ensures that the sign in screen is correctly dismissed.
// Regression test for crbug.com/596029.
- (void)testSignInCancelFromBookmarks {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Open Bookmarks and tap on Sign In promo button.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::BookmarksDestinationButton()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Assert sign-in screen was shown.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];

  // Verifies that the Chrome sign-in view is visible.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open new tab to cancel sign-in.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::BookmarksDestinationButton()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];

  // Verifies that the Chrome sign-in view is visible.
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Bookmarks.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Dismiss tests

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: user consent.
- (void)testDismissSigninFromSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromSettings
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: advanced sign-in.
- (void)testDismissAdvancedSigninSettingsFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromSettings
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: bookmark view.
// Interrupted at: user consent.
- (void)testDismissSigninFromBookmarks {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromBookmarks
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: bookmark view.
// Interrupted at: advanced sign-in.
- (void)testDismissAdvancedSigninBookmarksFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromBookmarks
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: user consent.
- (void)testDismissSigninFromRecentTabs {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromRecentTabs
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: advanced sign-in.
- (void)testDismissSigninFromRecentTabsFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromRecentTabs
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: user consent.
- (void)testDismissSigninFromTabSwitcher {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromTabSwitcher
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: advanced sign-in.
- (void)testDismissSigninFromTabSwitcherFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromTabSwitcher
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: identity picker.
- (void)testDismissSigninFromTabSwitcherFromIdentityPicker {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [self openSigninFromView:OpenSigninMethodFromTabSwitcher tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];

  // Open the URL as if it was opened from another app.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  [SigninEarlGrey verifySignedOut];
}

// Opens the reauth dialog and interrupts it by open an URL from an external
// app.
- (void)testInterruptReauthSignin {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyAppInterface triggerReauthDialogWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Open the URL as if it was opened from another app.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];
}

// Verifies that the user is signed in when selecting "Yes I'm In", after the
// advanced settings were swiped to dismiss.
// TODO(crbug.com/1277545): Flaky on iOS simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testSwipeDownInAdvancedSettings \
  DISABLED_testSwipeDownInAdvancedSettings
#else
#define MAYBE_testSwipeDownInAdvancedSettings \
  testSwipeDownInAdvancedSettings
#endif
- (void)MAYBE_testSwipeDownInAdvancedSettings {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:YES];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  [SigninEarlGreyUI tapSigninConfirmationDialog];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

#pragma mark - Utils

// Opens sign-in view.
// `openSigninMethod` is the way to start the sign-in.
// `tapSettingsLink` if YES, the setting link is tapped before opening the URL.
- (void)openSigninFromView:(OpenSigninMethod)openSigninMethod
           tapSettingsLink:(BOOL)tapSettingsLink {
  switch (openSigninMethod) {
    case OpenSigninMethodFromSettings:
      [ChromeEarlGreyUI openSettingsMenu];
      [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
      break;
    case OpenSigninMethodFromBookmarks:
      [ChromeEarlGreyUI openToolsMenu];
      [ChromeEarlGreyUI
          tapToolsMenuButton:chrome_test_util::BookmarksDestinationButton()];
      [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
          performAction:grey_tap()];
      break;
    case OpenSigninMethodFromRecentTabs:
      [SigninEarlGreyUI tapPrimarySignInButtonInRecentTabs];
      break;
    case OpenSigninMethodFromTabSwitcher:
      [SigninEarlGreyUI tapPrimarySignInButtonInTabSwitcher];
      break;
  }
  if (tapSettingsLink) {
    [ChromeEarlGreyUI waitForAppToIdle];
    [[EarlGrey selectElementWithMatcher:SettingsLink()]
        performAction:grey_tap()];
  }
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Starts the sign-in workflow, and simulates opening an URL from another app.
// `openSigninMethod` is the way to start the sign-in.
// `tapSettingsLink` if YES, the setting link is tapped before opening the URL.
- (void)assertOpenURLWhenSigninFromView:(OpenSigninMethod)openSigninMethod
                        tapSettingsLink:(BOOL)tapSettingsLink {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [self openSigninFromView:openSigninMethod tapSettingsLink:tapSettingsLink];
  // Open the URL as if it was opened from another app.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];
  // Should be not signed in, after being interrupted.
  [SigninEarlGrey verifySignedOut];
}

// Checks that the fake SSO screen shown on adding an account is visible
// onscreen.
- (void)assertFakeSSOScreenIsVisible {
  // Check for the fake SSO screen.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAuthActivityViewIdentifier)];
  // Close the SSO view controller.
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityID(kFakeAuthCancelButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Make sure the SSO view controller is fully removed before ending the test.
  // The tear down needs to remove other view controllers, and it cannot be done
  // during the animation of the SSO view controler.
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests the "ADD ACCOUNT" button in the identity chooser view controller.
- (void)testAddAccountAutomatically {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Tap on "ADD ACCOUNT".
  [SigninEarlGreyUI tapAddAccountButton];

  [self assertFakeSSOScreenIsVisible];
  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests that an add account operation triggered from the web is handled.
// Regression test for crbug.com/1054861.
- (void)testSigninAddAccountFromWeb {
  [ChromeEarlGrey simulateAddAccountFromWeb];

  [self assertFakeSSOScreenIsVisible];
}

// Tests to remove the last identity in the identity chooser.
- (void)testRemoveLastAccount {
  // Set up a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Open the identity chooser.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [ChromeEarlGrey
      waitForMatcher:IdentityCellMatcherForEmail(fakeIdentity.userEmail)];

  // Remove the fake identity.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];

  // Check that the identity has been removed.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      assertWithMatcher:grey_notVisible()];
}

// Opens the add account screen and then cancels it by opening a new tab.
// Ensures that the add account screen is correctly dismissed. crbug.com/462200
- (void)testSignInCancelAddAccount {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Open Add Account screen.
  id<GREYMatcher> add_account_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT);
  [[EarlGrey selectElementWithMatcher:add_account_matcher]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Open new tab to cancel sign-in.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];

  // Verifies that the Chrome sign-in view is visible.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Simulates a potential race condition in which the account is invalidated
// after the user taps the Settings button to navigate to the identity
// choosing UI. Depending on the timing, the account removal may occur after
// the UI has retrieved the list of valid accounts. The test ensures that in
// this case no account is presented to the user.
- (void)testAccountInvalidatedDuringSignin {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Invalidate account after menu generation. If the underlying code does not
  // handle the race condition of removing an identity while showing menu is in
  // progress this test will likely be flaky.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  // Check that the identity has been removed.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the sign-in coordinator isn't started when sign-in is disabled by
// policy.
- (void)testSigninDisabledByPolicy {
  // Disable browser sign-in only after the "Sign in to Chrome" button is
  // visible.
  [ChromeEarlGreyUI openSettingsMenu];

  // Disable sign-in with policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kDisabled);

  // Verify the sign-in view isn't showing.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_notVisible()];
}

// Tests that a signed-in user can open "Settings" screen from the NTP.
- (void)testOpenManageSyncSettingsFromNTP {
  // Sign in to Chrome.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Select the identity disc particle.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(GetNSStringF(
                                   IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL,
                                   base::SysNSStringToUTF16(
                                       fakeIdentity.userFullName),
                                   base::SysNSStringToUTF16(
                                       fakeIdentity.userEmail)))]
      performAction:grey_tap()];

  // Ensure the Settings menu is displayed.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the accessibility string for a signed-in user whose name is not
// available yet.
- (void)testAccessiblityStringForSignedInUserWithoutName {
  NSString* email = @"test@test.com";
  NSString* gaiaID = @"gaiaID";
  // Sign in to Chrome.
  FakeSystemIdentity* fakeIdentity =
      [FakeSystemIdentity identityWithEmail:email gaiaID:gaiaID name:nil];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Select the identity disc particle with the correct accessibility string.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(GetNSStringF(
                                          IDS_IOS_IDENTITY_DISC_WITH_EMAIL,
                                          base::SysNSStringToUTF16(
                                              fakeIdentity.userEmail)))]
      performAction:grey_tap()];

  // Ensure the Settings menu is displayed.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a signed-in user can open "Settings" screen from the NTP.
- (void)testOpenManageSyncSettingsFromNTPWhenSigninIsNotAllowedByPolicy {
  // Disable sign-in with policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kDisabled);

  // Select the identity disc particle.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(GetNSString(
                                          IDS_IOS_IDENTITY_DISC_SIGNED_OUT))]
      performAction:grey_tap()];

  // Ensure the Settings menu is displayed.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a signed-in user can open "Settings" screen from the NTP.
- (void)testOpenManageSyncSettingsFromNTPWhenSyncDisabledByPolicy {
  // Disable sync by policy.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(GetNSString(
                                       IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE)),
                                   grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];

  // Select the identity disc particle.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(GetNSString(
                                          IDS_IOS_IDENTITY_DISC_SIGNED_OUT))]
      performAction:grey_tap()];

  // Ensure the Settings menu is displayed.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a signed-out user can open "Sign in" screen from the NTP.
- (void)testOpenSignInFromNTP {
  // Select the identity disc particle.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(GetNSString(
                                          IDS_IOS_IDENTITY_DISC_SIGNED_OUT))]
      performAction:grey_tap()];

  // Ensure the sign-in menu is displayed. The existence of the skip
  // accessibility button on screen verifies that tha sign-in screen was
  // shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];
}

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is already an identity on the device.
- (void)testSignInFromSettingsMenu {
  // Set up a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Check `fakeIdentity` is signed-in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Check the Settings Menu labels for sync state.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityValue(
                                       GetNSString(IDS_IOS_SETTING_ON)),
                                   grey_accessibilityID(
                                       kSettingsGoogleSyncAndServicesCellId),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsGoogleServicesCellId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that opening the sign-in screen from the Sync Off tab and signin in
// will turn Sync On.
- (void)testSignInFromSyncOffLink {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  [ChromeEarlGreyUI openSettingsMenu];
  // Check Sync Off label is visible and user is signed in.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityValue(
                                       GetNSString(IDS_IOS_SETTING_OFF)),
                                   grey_accessibilityID(
                                       kSettingsGoogleSyncAndServicesCellId),
                                   nil)] performAction:grey_tap()];

  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // Check Sync On label is visible and user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityValue(
                                       GetNSString(IDS_IOS_SETTING_ON)),
                                   grey_accessibilityID(
                                       kSettingsGoogleSyncAndServicesCellId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that opening the sign-in screen from the Sync Off tab and canceling the
// sign-in flow will leave a signed-in with sync off user in the same state.
- (void)testCancelFromSyncOffLink {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  [ChromeEarlGreyUI openSettingsMenu];
  // Check Sync Off label is visible and user is signed in.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityValue(
                                       GetNSString(IDS_IOS_SETTING_OFF)),
                                   grey_accessibilityID(
                                       kSettingsGoogleSyncAndServicesCellId),
                                   nil)] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];

  // Check Sync Off label is visible and user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityValue(
                                       GetNSString(IDS_IOS_SETTING_OFF)),
                                   grey_accessibilityID(
                                       kSettingsGoogleSyncAndServicesCellId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the sign-in promo for no identities is displayed in Settings when
// the user is signed out and has not added any identities to the device.
- (void)testSigninPromoWithNoIdentitiesOnDevice {
  [ChromeEarlGreyUI openSettingsMenu];

  [SigninEarlGrey verifySignedOut];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
}

// Tests that the sign-in promo with user name is displayed in Settings when the
// user is signed out.
- (void)testSigninPromoWhenSignedOut {
  // Add identity to the device.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];

  [SigninEarlGrey verifySignedOut];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
}

// Tests that the sign-in promo is removed from Settings when the user
// is signed out and has closed the sign-in promo with user name.
- (void)testSigninPromoClosedWhenSignedOut {
  // Add identity to the device.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount
                           closeButton:YES];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoCloseButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that the sign-in promo for Sync is displayed when the user is signed in
// with Sync off.
- (void)testSigninPromoWhenSyncOff {
  // Add identity to the device.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
}

// Tests that no sign-in promo for Sync is displayed when the user is signed in
// with Sync off and has closed the sign-in promo for Sync.
- (void)testSigninPromoClosedWhenSyncOff {
  // Add identity to the device.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  // Tap on dismiss button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoCloseButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that Sync is on when introducing passphrase from settings, after
// logging in.
// TODO(crbug.com/1277545): Flaky on iOS simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testSyncOnWhenPassphraseIntroducedAfterSignIn \
  DISABLED_testSyncOnWhenPassphraseIntroducedAfterSignIn
#else
#define MAYBE_testSyncOnWhenPassphraseIntroducedAfterSignIn \
  testSyncOnWhenPassphraseIntroducedAfterSignIn
#endif
- (void)MAYBE_testSyncOnWhenPassphraseIntroducedAfterSignIn {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // Give the Sync state a chance to finish UI updates.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      GoogleServicesSettingsButton()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityValue(GetNSString(
                                       IDS_IOS_SYNC_ENCRYPTION_DESCRIPTION)),
                                   grey_accessibilityID(
                                       kSettingsGoogleSyncAndServicesCellId),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Scroll to bottom of Manage Sync Settings, if necessary.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kManageSyncTableViewAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Select Encryption item.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_MANAGE_SYNC_ENCRYPTION)]
      performAction:grey_tap()];

  // Type and submit the sync passphrase.
  [SigninEarlGreyUI submitSyncPassphrase:kPassphrase];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SettingsDoneButton()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI openSettingsMenu];

  // Check Sync On label is visible.
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

// Tests to sign-in with one user, and then turn on syncn with a second account.
- (void)testSignInWithOneAccountStartSyncWithAnotherAccount {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign-in only with fakeIdentity1.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1 enableSync:NO];

  // Open turn on sync dialog.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  // Select fakeIdentity2.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // Check fakeIdentity2 is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity2];
}

// Tests that when the syncTypesListDisabled policy is enabled, a policy warning
// is displayed with a link to the policy management page.
- (void)testSyncTypesDisabledPolicy {
  // Set policy.
  base::Value::List list;
  list.Append("tabs");
  policy_test_utils::SetPolicy(base::Value(std::move(list)),
                               policy::key::kSyncTypesListDisabled);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [ChromeEarlGreyUI waitForAppToIdle];

  NSString* policyText =
      GetNSString(IDS_IOS_ENTERPRISE_MANAGED_SIGNIN_LEARN_MORE);
  policyText = [policyText stringByReplacingOccurrencesOfString:@"BEGIN_LINK"
                                                     withString:@""];
  policyText = [policyText stringByReplacingOccurrencesOfString:@"END_LINK"
                                                     withString:@""];

  // Check that the policy warning is presented.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(policyText),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Check that the "learn more link" works.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(kLearnMoreLabel),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitLink),
                                   nil)] performAction:grey_tap()];

  // Check that the policy management page was opened.
  [ChromeEarlGrey waitForWebStateContainingText:kManagedText];
}

// Tests that the sign-in promo disappear when sync is disabled and reappears
// when sync is enabled again.
// Related to crbug.com/1287465.
- (void)testTurnOffSyncDisablePolicy {
  // Disable sync by policy.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(GetNSString(
                                       IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE)),
                                   grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];
  // Open other device tab.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Check that the sign-in promo is not visible.
  [SigninEarlGreyUI verifySigninPromoNotVisible];
  // Add an identity to generate a SSO identity update notification.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Enable sync.
  policy_test_utils::SetPolicy(false, policy::key::kSyncDisabled);
  [ChromeEarlGreyUI waitForAppToIdle];
  // Check that the sign-in promo is visible.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollToContentEdgeWithStartPoint(
                               kGREYContentEdgeBottom, 0.5, 0.5)
      onElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kRecentTabsTableViewControllerAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests to dismiss the sign-in view by swipe down without an identity.
// See http://crbug.com/1434238.
- (void)testSwipeDownSignInViewWithoutAnIdentity {
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kUnifiedConsentScrollViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Test no crash.
}

@end
