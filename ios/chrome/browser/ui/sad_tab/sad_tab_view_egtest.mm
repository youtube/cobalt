// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// A matcher for the main title of the Sad Tab in 'reload' mode.
id<GREYMatcher> reloadSadTabTitleText() {
  return chrome_test_util::ContainsPartialText(
      l10n_util::GetNSString(IDS_SAD_TAB_MESSAGE));
}

// A matcher for the main title of the Sad Tab in 'feedback' mode.
id<GREYMatcher> feedbackSadTabTitleContainsText() {
  return chrome_test_util::ContainsPartialText(
      l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_TRY));
}

// A matcher for a help string suggesting the user use Incognito Mode.
id<GREYMatcher> incognitoHelpContainsText() {
  return chrome_test_util::ContainsPartialText(
      l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_INCOGNITO));
}
}  // namespace

// Sad Tab View integration tests for Chrome.
@interface SadTabViewTestCase : ChromeTestCase
@end

@implementation SadTabViewTestCase

// Verifies initial and repeated visits to the Sad Tab.
// N.B. There is a mechanism which changes the Sad Tab UI if a crash URL is
// visited within 60 seconds, for this reason this one test can not
// be easily split up across multiple tests
// as visiting Sad Tab may not be idempotent.
// TODO(crbug.com/1047238): Test fails when run on iOS 13.
- (void)DISABLED_testSadTabView {
  // Prepare a simple but known URL to avoid testing from the NTP.
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL simple_URL = self.testServer->GetURL("/destination.html");

  // Prepare a helper block to test Sad Tab navigating from and to normal pages.
  void (^loadAndCheckSimpleURL)() = ^void() {
    [ChromeEarlGrey loadURL:simple_URL];
    [ChromeEarlGrey waitForWebStateContainingText:"You've arrived"];
    [[EarlGrey selectElementWithMatcher:reloadSadTabTitleText()]
        assertWithMatcher:grey_nil()];
    [[EarlGrey selectElementWithMatcher:feedbackSadTabTitleContainsText()]
        assertWithMatcher:grey_nil()];
  };

  loadAndCheckSimpleURL();

  // Navigate to the chrome://crash URL which should show the Sad Tab.
  const GURL crash_URL = GURL("chrome://crash");
  [ChromeEarlGrey loadURL:crash_URL waitForCompletion:NO];
  [[EarlGrey selectElementWithMatcher:reloadSadTabTitleText()]
      assertWithMatcher:grey_notNil()];

  // Ensure user can navigate away from Sad Tab, and the Sad Tab content
  // is no longer visible.
  loadAndCheckSimpleURL();

  // A second visit to the crashing URL should show a feedback message.
  // It should also show help messages including an invitation to use
  // Incognito Mode.
  [ChromeEarlGrey loadURL:crash_URL waitForCompletion:NO];
  [[EarlGrey selectElementWithMatcher:feedbackSadTabTitleContainsText()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:incognitoHelpContainsText()]
      assertWithMatcher:grey_notNil()];

  // Again ensure a user can navigate away from Sad Tab, and the Sad Tab content
  // is no longer visible.
  loadAndCheckSimpleURL();

  // Open an Incognito tab and browse somewhere, the repeated crash UI changes
  // dependent on the Incognito mode.
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> newIncognitoTabButtonMatcher =
      grey_accessibilityID(kToolsMenuNewIncognitoTabId);
  [[EarlGrey selectElementWithMatcher:newIncognitoTabButtonMatcher]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  loadAndCheckSimpleURL();

  // Test an initial crash, and then a second crash in Incognito mode, as above.
  // Incognito mode should not be suggested if already in Incognito mode.
  [ChromeEarlGrey loadURL:crash_URL waitForCompletion:NO];
  [[EarlGrey selectElementWithMatcher:reloadSadTabTitleText()]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey loadURL:crash_URL waitForCompletion:NO];
  [[EarlGrey selectElementWithMatcher:feedbackSadTabTitleContainsText()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:incognitoHelpContainsText()]
      assertWithMatcher:grey_nil()];

  // Finally, ensure that the user can browse away from the Sad Tab page
  // in Incognito Mode.
  loadAndCheckSimpleURL();
}

@end
