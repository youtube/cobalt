// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <map>
#import <string>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/history/history_ui_constants.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_app_interface.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::RecentTabsDestinationButton;

namespace {
const char kURLOfTestPage[] = "http://testPage";
const char kHTMLOfTestPage[] =
    "<head><title>TestPageTitle</title></head><body>hello</body>";
NSString* const kTitleOfTestPage = @"TestPageTitle";

// Makes sure at least one tab is opened and opens the recent tab panel.
void OpenRecentTabsPanel() {
  // At least one tab is needed to be able to open the recent tabs panel.
  if ([ChromeEarlGrey isIncognitoMode]) {
    if ([ChromeEarlGrey incognitoTabCount] == 0)
      [ChromeEarlGrey openNewIncognitoTab];
  } else {
    if ([ChromeEarlGrey mainTabCount] == 0)
      [ChromeEarlGrey openNewTab];
  }

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:RecentTabsDestinationButton()];
}

// Returns the matcher for the Recent Tabs table.
id<GREYMatcher> RecentTabsTable() {
  return grey_accessibilityID(
      kRecentTabsTableViewControllerAccessibilityIdentifier);
}

// Returns the matcher for the entry of the page in the recent tabs panel.
id<GREYMatcher> TitleOfTestPage() {
  return grey_allOf(
      grey_ancestor(RecentTabsTable()),
      chrome_test_util::StaticTextWithAccessibilityLabel(kTitleOfTestPage),
      grey_sufficientlyVisible(), nil);
}

GURL TestPageURL() {
  return web::test::HttpServer::MakeUrl(kURLOfTestPage);
}

}  // namespace

// Earl grey integration tests for Recent Tabs Panel Controller.
@interface RecentTabsTestCase : WebHttpServerChromeTestCase
@end

@implementation RecentTabsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector(testSyncTypesListDisabled)]) {
    // Configure the policy.
    config.additional_args.push_back(
        "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
    config.additional_args.push_back(
        "<dict><key>SyncTypesListDisabled</key><array><string>tabs</"
        "string></array></dict>");
  }
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];
  web::test::SetUpSimpleHttpServer(std::map<GURL, std::string>{{
      TestPageURL(),
      std::string(kHTMLOfTestPage),
  }});
  [RecentTabsAppInterface clearCollapsedListViewSectionStates];
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  [PolicyAppInterface clearPolicies];
}

// Tests that a closed tab appears in the Recent Tabs panel, and that tapping
// the entry in the Recent Tabs panel re-opens the closed tab.
- (void)testClosedTabAppearsInRecentTabsPanel {
  const GURL testPageURL = TestPageURL();

  // Open the test page in a new tab.
  [ChromeEarlGrey loadURL:testPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello"];

  // Open the Recent Tabs panel, check that the test page is not
  // present.
  OpenRecentTabsPanel();
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      assertWithMatcher:grey_nil()];
  [self closeRecentTabs];

  // Close the tab containing the test page and wait for the stack view to
  // appear.
  [ChromeEarlGrey closeCurrentTab];

  // Open the Recent Tabs panel and check that the test page is present.
  OpenRecentTabsPanel();
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      assertWithMatcher:grey_notNil()];

  // Tap on the entry for the test page in the Recent Tabs panel and check that
  // a tab containing the test page was opened.
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(
                            testPageURL.GetContent())];
}

// Tests that tapping "Show Full History" open the history.
- (void)testOpenHistory {
  OpenRecentTabsPanel();

  // Tap "Show Full History"
  id<GREYMatcher> showHistoryMatcher =
      grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_HISTORY_SHOWFULLHISTORY_LINK),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:showHistoryMatcher]
      performAction:grey_tap()];

  // Make sure history is opened.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              l10n_util::GetNSString(
                                                  IDS_HISTORY_TITLE)),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitHeader),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Close History.
  id<GREYMatcher> exitMatcher =
      grey_accessibilityID(kHistoryNavigationControllerDoneButtonIdentifier);
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];

  // Close tab.
  [ChromeEarlGrey closeCurrentTab];
}

// Tests that the sign-in promo can be reloaded correctly.
- (void)testRecentTabSigninPromoReloaded {
  OpenRecentTabsPanel();

  // Scroll to sign-in promo, if applicable.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Sign-in promo should be visible with no accounts on the device.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:NO];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Sign-in promo should be visible with an account on the device.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount
                           closeButton:NO];
  [self closeRecentTabs];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
}

// Tests that the sign-in promo can be reloaded correctly while being hidden.
// crbug.com/776939
- (void)testRecentTabSigninPromoReloadedWhileHidden {
  OpenRecentTabsPanel();

  // Scroll to sign-in promo, if applicable
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:NO];

  // Tap on "Other Devices", to hide the sign-in promo.
  NSString* otherDevicesLabel =
      l10n_util::GetNSString(IDS_IOS_RECENT_TABS_OTHER_DEVICES);
  id<GREYMatcher> otherDevicesMatcher = grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(otherDevicesLabel),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:otherDevicesMatcher]
      performAction:grey_tap()];
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Add an account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Tap on "Other Devices", to show the sign-in promo.
  [[EarlGrey selectElementWithMatcher:otherDevicesMatcher]
      performAction:grey_tap()];
  // Scroll to sign-in promo, if applicable
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount
                           closeButton:NO];
  [self closeRecentTabs];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
}

// Tests that the VC can be dismissed by swiping down.
- (void)testSwipeDownDismiss {
  // TODO(crbug.com/1129589): Test disabled on iOS14 iPhones.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iOS14 iPhones.");
  }
  OpenRecentTabsPanel();

  id<GREYMatcher> recentTabsViewController =
      grey_allOf(RecentTabsTable(), grey_sufficientlyVisible(), nil);

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:recentTabsViewController]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:recentTabsViewController]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:recentTabsViewController]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that the Recent Tabs can be opened while signed in (prevent regression
// for https://crbug.com/1056613).
- (void)testOpenWhileSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  OpenRecentTabsPanel();
}

// Tests that there is a text cell in the Recently Closed section when it's
// empty.
- (void)testRecentlyClosedEmptyState {
  OpenRecentTabsPanel();

  id<GREYInteraction> detailTextCell = [EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabelId(
                         IDS_IOS_RECENT_TABS_RECENTLY_CLOSED_EMPTY),
                     grey_sufficientlyVisible(), nil)];
  [detailTextCell assertWithMatcher:grey_notNil()];
}

// Tests that the Signin promo is visible in the Other Devices section and that
// the illustrated cell is present.
- (void)testOtherDevicesDefaultEmptyState {
  OpenRecentTabsPanel();

  id<GREYInteraction> illustratedCell = [EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kRecentTabsOtherDevicesIllustratedCellAccessibilityIdentifier),
              grey_sufficientlyVisible(), nil)];
  [illustratedCell assertWithMatcher:grey_notNil()];

  // Scroll to sign-in promo, if applicable
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:NO];
}

// Tests the Copy Link action on a recent tab's context menu.
- (void)testContextMenuCopyLink {
  [self loadTestURL];
  OpenRecentTabsPanel();
  [self longPressTestURLTab];

  GURL testURL = TestPageURL();
  [ChromeEarlGrey
      verifyCopyLinkActionWithText:[NSString
                                       stringWithUTF8String:testURL.spec()
                                                                .c_str()]];
}

// Tests the Open in New Tab action on a recent tab's context menu.
- (void)testContextMenuOpenInNewTab {
  [self loadTestURL];
  OpenRecentTabsPanel();
  [self longPressTestURLTab];

  [ChromeEarlGrey verifyOpenInNewTabActionWithURL:TestPageURL().GetContent()];

  // Verify that Recent Tabs closed.
  [[EarlGrey selectElementWithMatcher:RecentTabsTable()]
      assertWithMatcher:grey_notVisible()];
}

// Tests the Open in New Window action on a recent tab's context menu.
// TODO(crbug.com/1273942) Test is flaky.
- (void)FLAKY_testContextMenuOpenInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  [self loadTestURL];
  OpenRecentTabsPanel();

  [self longPressTestURLTab];

  [ChromeEarlGrey verifyOpenInNewWindowActionWithContent:"hello"];

  // Validate that Recent tabs was not closed in the original window. The
  // Accessibility Element matcher is added as there are other (non-accessible)
  // recent tabs tables in each window's TabGrid (but hidden).
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_accessibilityElement(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests the Share action on a recent tab's context menu.
- (void)testContextMenuShare {
  [self loadTestURL];
  OpenRecentTabsPanel();
  [self longPressTestURLTab];

  const GURL testPageURL = web::test::HttpServer::MakeUrl(kURLOfTestPage);
  [ChromeEarlGrey verifyShareActionWithURL:testPageURL
                                 pageTitle:kTitleOfTestPage];
}

#pragma mark Helper Methods

// Opens a new tab and closes it, to make sure it appears as a recently closed
// tab.
- (void)loadTestURL {
  const GURL testPageURL = web::test::HttpServer::MakeUrl(kURLOfTestPage);

  // Open the test page in a new tab.
  [ChromeEarlGrey loadURL:testPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello"];

  // Close the tab, making it appear in Recent Tabs.
  [ChromeEarlGrey closeCurrentTab];
}

// Long-presses on a recent tab entry.
- (void)longPressTestURLTab {
  // The test page may be there multiple times.
  [[[EarlGrey selectElementWithMatcher:TitleOfTestPage()] atIndex:0]
      performAction:grey_longPress()];
}

// Closes the recent tabs panel.
- (void)closeRecentTabs {
  id<GREYMatcher> exitMatcher =
      grey_accessibilityID(kTableViewNavigationDismissButtonId);
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];
  // Wait until the recent tabs panel is dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests that the sign-in promo isn't shown and the 'Other Devices' section is
// managed when the SyncDisabled policy is enabled.
- (void)testSyncDisabled {
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);

  // Dismiss the popup.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE)),
                            grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];

  OpenRecentTabsPanel();

  // Check that the sign-in promo is not visible.
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Check that the 'Other Devices' section is managed.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_RECENT_TABS_DISABLED_BY_ORGANIZATION)),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the sign-in promo isn't shown and the 'Other Devices' section is
// managed when the SyncTypesListDisabled tabs item policy is selected.
- (void)testSyncTypesListDisabled {
  OpenRecentTabsPanel();

  // Check that the sign-in promo is not visible.
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Check that the 'Other Devices' section is managed.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_RECENT_TABS_DISABLED_BY_ORGANIZATION)),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the sign-in promo isn't shown and the 'Other Devices' section has
// the managed notice footer when sign-in is disabled by the BrowserSignin
// policy.
- (void)testSignInDisabledByPolicy {
  policy_test_utils::SetPolicy(static_cast<int>(BrowserSigninMode::kDisabled),
                               policy::key::kBrowserSignin);

  OpenRecentTabsPanel();

  // Check that the sign-in promo is not visible.
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Check that the 'Other Devices' section has the managed notice.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_RECENT_TABS_DISABLED_BY_ORGANIZATION)),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

@end
