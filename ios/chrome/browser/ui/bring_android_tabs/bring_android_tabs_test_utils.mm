// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_test_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_app_interface.h"
#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_test_session.h"
#import "ios/chrome/browser/ui/bring_android_tabs/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

#import <memory>

namespace {

// Timeout in seconds to wait for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

// Returns GREYElementInteraction for `matcher`, using `scrollViewMatcher` to
// scroll.
void TapPromoStyleButton(NSString* buttonIdentifier) {
  id<GREYMatcher> buttonMatcher = grey_accessibilityID(buttonIdentifier);
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  id<GREYAction> searchAction = grey_scrollInDirection(kGREYDirectionDown, 200);
  GREYElementInteraction* element =
      [[EarlGrey selectElementWithMatcher:buttonMatcher]
             usingSearchAction:searchAction
          onElementWithMatcher:scrollViewMatcher];
  [element performAction:grey_tap()];
}

}  // namespace

AppLaunchConfiguration GetConfiguration(BOOL is_android_switcher) {
  AppLaunchConfiguration config;
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");
  if (is_android_switcher) {
    config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
    config.additional_args.push_back("AndroidPhone");
  }
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

void CompleteFREWithSyncEnabled(BOOL sync) {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fake_identity];
  // Sign in.
  TapPromoStyleButton(kPromoStylePrimaryActionAccessibilityIdentifier);
  // Sync.
  TapPromoStyleButton(sync ? kPromoStylePrimaryActionAccessibilityIdentifier
                           : kPromoStyleSecondaryActionAccessibilityIdentifier);
  // Default browser promo dismissal.
  TapPromoStyleButton(kPromoStyleSecondaryActionAccessibilityIdentifier);
  if (sync) {
    [ChromeEarlGrey
        waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
  }
}

void AddSessionToFakeSyncServerFromTestServer(
    BringAndroidTabsTestSession session,
    const GURL& test_server) {
  [BringAndroidTabsAppInterface
      addFakeSyncServerSession:session
                fromTestServer:base::SysUTF8ToNSString(test_server.spec())];
}

void SignInAndSync() {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fake_identity];
  [SigninEarlGreyUI signinWithFakeIdentity:fake_identity enableSync:YES];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
}

void VerifyConfirmationAlertPromptVisibility(BOOL visibility) {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBringAndroidTabsPromptConfirmationAlertAXId)]
      assertWithMatcher:visibility ? grey_sufficientlyVisible()
                                   : grey_notVisible()];
}

void VerifyTabListPromptVisibility(BOOL visibility) {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBringAndroidTabsPromptTabListAXId)]
      assertWithMatcher:visibility ? grey_sufficientlyVisible()
                                   : grey_notVisible()];
}

int GetTabCountOnPrompt() {
  return [BringAndroidTabsAppInterface tabsCountForPrompt];
}

void VerifyThatPromptDoesNotShowOnRestart(const GURL& test_server) {
  AppLaunchConfiguration config = GetConfiguration(/*is_android_switcher=*/YES);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  AddSessionToFakeSyncServerFromTestServer(
      BringAndroidTabsTestSession::kRecentFromAndroidPhone, test_server);
  CompleteFREWithSyncEnabled(YES);
  [ChromeEarlGreyUI openTabGrid];
  VerifyConfirmationAlertPromptVisibility(NO);
}

void CleanUp() {
  [SigninEarlGrey signOut];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey clearSyncServerData];
  [ChromeEarlGreyAppInterface
      clearUserPrefWithName:@"ios.bring_android_tabs.prompt_displayed"];
}
