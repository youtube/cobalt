// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/search_engines/search_engines_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/whats_new/feature_flags.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kPageLoadedString[] = "Page loaded!";
const char kPageURL[] = "/test-page.html";
const char kPageTitle[] = "Page title!";

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kPageURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("<html><head><title>" + std::string(kPageTitle) +
                             "</title></head><body>" +
                             std::string(kPageLoadedString) + "</body></html>");
  return std::move(http_response);
}

// Returns a matcher, which is true if the view has its width equals to `width`.
id<GREYMatcher> OmniboxWidth(CGFloat width) {
  GREYMatchesBlock matches = ^BOOL(UIView* view) {
    return fabs(view.bounds.size.width - width) < 0.001;
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description
        appendText:[NSString stringWithFormat:@"Omnibox has correct width: %g",
                                              width]];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

// Returns a matcher, which is true if the view has its width equals to `width`
// plus or minus `margin`.
id<GREYMatcher> OmniboxWidthBetween(CGFloat width, CGFloat margin) {
  GREYMatchesBlock matches = ^BOOL(UIView* view) {
    return view.bounds.size.width >= width - margin &&
           view.bounds.size.width <= width + margin;
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description
        appendText:[NSString
                       stringWithFormat:
                           @"Omnibox has correct width: %g with margin: %g",
                           width, margin]];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

// Returns a matcher which is true if the view is not practically visible.
// Sometimes grey_notVisible() fails because the view is 0.0000XYZ percent
// visible, but not actually zero, probably due to some sort of floating
// point calculation.
id<GREYMatcher> notPracticallyVisible() {
  return grey_not(grey_minimumVisiblePercent(0.01));
}
}

// Test case for the NTP home UI. More precisely, this tests the positions of
// the elements after interacting with the device.
@interface NTPHomeTestCase : ChromeTestCase

@property(nonatomic, strong) NSString* defaultSearchEngine;

@end

@implementation NTPHomeTestCase

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [NTPHomeTestCase setUpHelper];
}

+ (void)setUpHelper {
  // Clear the pasteboard in case there is a URL copied, triggering an omnibox
  // suggestion.
  UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
  [pasteboard setValue:@"" forPasteboardType:UIPasteboardNameGeneral];

  [self closeAllTabs];
}

+ (void)tearDown {
  [self closeAllTabs];

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to enable the Discover feed for this test case.
  // Disabled elsewhere to account for possible flakiness.
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.additional_args.push_back(std::string("--") +
                                   switches::kEnableDiscoverFeed);
  // Show doodle to make sure tests cover async callback logic updating logo.
  config.additional_args.push_back(
      std::string("-google-doodle-url=https://www.gstatic.com/chrome/ntp/"
                  "doodle_test/ddljson_android0.json"));
  config.features_disabled.push_back(kEnableFeedAblation);
  // TODO(crbug.com/1403077): Scrolling issues when promo is enabled.
  config.features_disabled.push_back(kEnableDiscoverFeedTopSyncPromo);
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGreyAppInterface
      setBoolValue:YES
       forUserPref:base::SysUTF8ToNSString(prefs::kArticlesForYouEnabled)];
  [ChromeEarlGreyAppInterface
      setBoolValue:YES
       forUserPref:base::SysUTF8ToNSString(feed::prefs::kArticlesListVisible)];

  self.defaultSearchEngine = [SearchEnginesAppInterface defaultSearchEngine];
}

- (void)tearDown {
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                                error:nil];
  [SearchEnginesAppInterface setSearchEngineTo:self.defaultSearchEngine];

  [super tearDown];
}

#pragma mark - Tests

// Tests that all items are accessible on the home page.
// This is currently needed to prevent this test case from being ignored.
- (void)testAccessibility {
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that the collections shortcut are displayed and working.
- (void)testCollectionShortcuts {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_disabled.push_back(kWhatsNewIOS);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Check the Bookmarks.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the ReadingList.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_TOOLS_MENU_READING_LIST)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the RecentTabs.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the History.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_HISTORY)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_HISTORY_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that when loading an invalid URL, the NTP is still displayed.
// Prevents regressions from https://crbug.com/1063154 .
- (void)testInvalidURL {
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad, because key '-' could not be "
                            @"found on the keyboard.");
  }
#endif  // !TARGET_IPHONE_SIMULATOR

  NSString* URL = @"app-settings://test/";

  // The URL needs to be typed to trigger the bug.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(URL)];

  // The first suggestion is a search, the second suggestion is the URL.
  id<GREYMatcher> rowMatcher = grey_allOf(
      grey_accessibilityID(@"omnibox suggestion 0 1"),
      chrome_test_util::OmniboxPopupRow(),
      grey_descendant(chrome_test_util::StaticTextWithAccessibilityLabel(URL)),
      grey_sufficientlyVisible(), nil);

  [[EarlGrey selectElementWithMatcher:rowMatcher] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}
// Tests that the Search Widget URL loads the NTP with the Omnibox focused.
- (void)testOpenSearchWidget {
  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://search-widget/search")];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the fake omnibox width is correctly updated after a rotation.
- (void)testOmniboxWidthRotation {

  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }
  [ChromeEarlGreyUI waitForAppToIdle];
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  UIEdgeInsets safeArea = collectionView.safeAreaInsets;
  CGFloat collectionWidth =
      CGRectGetWidth(UIEdgeInsetsInsetRect(collectionView.bounds, safeArea));
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");
  CGFloat fakeOmniboxWidth = [NewTabPageAppInterface
      searchFieldWidthForCollectionWidth:collectionWidth
                         traitCollection:collectionView.traitCollection];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  [ChromeEarlGreyUI waitForAppToIdle];

  collectionView = [NewTabPageAppInterface collectionView];
  safeArea = collectionView.safeAreaInsets;
  CGFloat collectionWidthAfterRotation =
      CGRectGetWidth(UIEdgeInsetsInsetRect(collectionView.bounds, safeArea));
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");
  fakeOmniboxWidth = [NewTabPageAppInterface
      searchFieldWidthForCollectionWidth:collectionWidthAfterRotation
                         traitCollection:collectionView.traitCollection];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];
}

// Tests that the fake omnibox width is correctly updated after a rotation done
// while the settings screen is shown.
- (void)testOmniboxWidthRotationBehindSettings {

  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if ([ChromeEarlGrey isRegularXRegularSizeClass]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }
  [ChromeEarlGreyUI waitForAppToIdle];
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  UIEdgeInsets safeArea = collectionView.safeAreaInsets;
  CGFloat collectionWidth =
      CGRectGetWidth(UIEdgeInsetsInsetRect(collectionView.bounds, safeArea));
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");
  CGFloat fakeOmniboxWidth = [NewTabPageAppInterface
      searchFieldWidthForCollectionWidth:collectionWidth
                         traitCollection:collectionView.traitCollection];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];

  [ChromeEarlGreyUI openSettingsMenu];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  collectionView = [NewTabPageAppInterface collectionView];
  safeArea = collectionView.safeAreaInsets;
  CGFloat collectionWidthAfterRotation =
      CGRectGetWidth(UIEdgeInsetsInsetRect(collectionView.bounds, safeArea));
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");
  fakeOmniboxWidth = [NewTabPageAppInterface
      searchFieldWidthForCollectionWidth:collectionWidthAfterRotation
                         traitCollection:collectionView.traitCollection];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];
}

// Tests that the fake omnibox width is correctly updated after a rotation done
// while the fake omnibox is pinned to the top.
- (void)testOmniboxPinnedWidthRotation {
  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if ([ChromeEarlGrey isRegularXRegularSizeClass]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  [ChromeEarlGreyUI waitForAppToIdle];
  CGFloat collectionWidth =
      [NewTabPageAppInterface collectionView].bounds.size.width;
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");

  // The fake omnibox might be slightly bigger than the screen in order to cover
  // it for all screen scale.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:OmniboxWidthBetween(collectionWidth + 1, 2)];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];

  [ChromeEarlGreyUI waitForAppToIdle];
  CGFloat collectionWidthAfterRotation =
      [NewTabPageAppInterface collectionView].bounds.size.width;
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");
}

// Tests that the fake omnibox remains visible when scrolling, by pinning itself
// to the top of the NTP. Also ensures that NTP minimum height is respected.
- (void)testOmniboxPinsToTop {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(
        @"Disabled for iPad since it does not pin the omnibox.");
  }

  UIView* fakeOmnibox = [NewTabPageAppInterface fakeOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertTrue(fakeOmnibox.frame.origin.x > 1,
                 @"The omnibox is pinned to top before scrolling down.");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  [ChromeEarlGreyUI waitForAppToIdle];

  // After scrolling down, the omnibox should be pinned and visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertTrue(fakeOmnibox.frame.origin.x < 1,
                 @"The omnibox is not pinned to top when scrolling down, or "
                 @"the NTP cannot scroll.");
}

// Tests that the fake omnibox animation works, increasing the width of the
// omnibox.
- (void)testOmniboxWidthChangesWithScroll {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(
        @"Disabled for iPad since the width does not change for it.");
  }

  CGFloat omniboxWidthBeforeScrolling =
      [NewTabPageAppInterface fakeOmnibox].frame.size.width;
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  [ChromeEarlGreyUI waitForAppToIdle];

  CGFloat omniboxWidthAfterScrolling =
      [NewTabPageAppInterface fakeOmnibox].frame.size.width;

  GREYAssertTrue(
      omniboxWidthAfterScrolling > omniboxWidthBeforeScrolling,
      @"Fake omnibox width did not animate properly when scrolling.");
}

// Tests that the tap gesture recognizer that dismisses the keyboard and
// defocuses the omnibox works.
- (void)testDefocusOmniboxTapWorks {
  [self focusFakebox];
  // Tap on a space in the collectionView that is not a Feed card.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(ntp_home::DiscoverHeaderTitleAccessibilityID())]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];
  // Check the fake omnibox is displayed again at the same position.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the app doesn't crash when opening multiple tabs.
- (void)testOpenMultipleTabs {
  NSInteger numberOfTabs = 10;
  for (NSInteger i = 0; i < numberOfTabs; i++) {
    [ChromeEarlGreyUI openNewTab];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_accessibilityValue([NSString
                            stringWithFormat:@"%@", @(numberOfTabs + 1)])];
}

// Tests that rotating to landscape and scrolling into the feed, opening another
// NTP, and then swtiching back retains the scroll position.
- (void)testOpenMultipleTabsandChangeOrientation {
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  [self testNTPInitialPositionAndContent:collectionView];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  [self testNTPInitialPositionAndContent:collectionView];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  CGFloat yOffsetBeforeSwitch = collectionView.contentOffset.y;

  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  GREYAssertTrue(yOffsetBeforeSwitch ==
                     [NewTabPageAppInterface collectionView].contentOffset.y,
                 @"NTP scroll position not saved properly.");
}

// Tests that the pull to refresh (iphone) or the refresh button (ipad) lands
// the user on the top of the NTP even with a previously saved scroll position.
- (void)testReload {
  // Scroll to have a position to restored.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Save the position before navigating.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat previousPosition = collectionView.contentOffset.y;

  // Navigate and come back.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  // Check that the new position is the same.
  GREYAssertEqual(previousPosition, collectionView.contentOffset.y,
                  @"NTP is not at the same position.");

  if ([ChromeEarlGrey isIPadIdiom]) {
    // Have to scroll up to the top since tapping on reload button does not
    // automatically scroll to the top when feed is off or if feed returns no
    // contents (e.g. upstream bots). TODO(crbug.com/1406940): Look into why the
    // Feed only scrolls up when there is content.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
    // Tap on reload button.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ReloadButton()]
        performAction:grey_tap()];
  } else {
    // Get back to the top of the page and then pull down to trigger Pull To
    // Refresh
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];
  }
  [ChromeEarlGreyUI waitForAppToIdle];
  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];
}

// Tests that the position of the collection view is restored when navigating
// back to the NTP.
- (void)testPositionRestored {
  // Scroll to have a position to restored.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Save the position before navigating.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat previousPosition = collectionView.contentOffset.y;

  // Navigate and come back.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  // Check that the new position is the same.
  GREYAssertEqual(previousPosition, collectionView.contentOffset.y,
                  @"NTP is not at the same position.");
}

// Tests that when navigating back to the NTP while having the omnibox focused
// and moved up, the scroll position restored is the position before the omnibox
// is selected.
- (void)testPositionRestoredWithShiftingOffset {
  [self addMostVisitedTile];
  // Scroll to have a position to restored.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 50)];

  // Save the position before navigating.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat previousPosition = collectionView.contentOffset.y;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Navigate and come back.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(
                     base::SysUTF8ToNSString(kPageTitle))]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the new position is the same.
  collectionView = [NewTabPageAppInterface collectionView];
  GREYAssertEqual(
      previousPosition, collectionView.contentOffset.y,
      @"NTP is not at the same position as before tapping the omnibox");
}

// Tests that when navigating back to the NTP while having the omnibox focused
// does not consider the shifting offset in the instance the omnibox was already
// pinned to the top of the page before focusing.
- (void)testPositionRestoredWithoutShiftingOffset {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Pinning Fake Omnibox to top of surface is only on iphone");
  }
  // Scroll enough to naturally pin the omnibox to the top.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Save the position before navigating.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat previousPosition = collectionView.contentOffset.y;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Ensure that focusing the omnibox doesn't change the scroll position.
  GREYAssertEqual(previousPosition, collectionView.contentOffset.y,
                  @"Focusing the omnibox changed the scroll position.");

  // Navigate and come back.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [ChromeEarlGrey goBack];

  // Check that the new position is the same.
  collectionView = [NewTabPageAppInterface collectionView];
  GREYAssertEqual(previousPosition, collectionView.contentOffset.y,
                  @"NTP is not at the same position");
}

// Tests that tapping the fake omnibox focuses the real omnibox.
- (void)testTapFakeOmnibox {
  // Setup the server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  NSString* URL = base::SysUTF8ToNSString(pageURL.spec());
  // Tap the fake omnibox, type the URL in the real omnibox and navigate to the
  // page.
  [self focusFakebox];

  // Check the fake omnibox is not visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText([URL stringByAppendingString:@"\n"])];

  // Check that the page is loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
}

// Tests that tapping the fake omnibox moves the collection.
- (void)testTapFakeOmniboxScroll {
  // Get the collection and its layout.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];

  // Offset before the tap.
  CGPoint origin = collectionView.contentOffset;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Offset after the fake omnibox has been tapped.
  CGPoint offsetAfterTap = collectionView.contentOffset;

  // Make sure the fake omnibox has been hidden and the collection has moved.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  GREYAssertTrue(offsetAfterTap.y >= origin.y,
                 @"The collection has not moved.");

  // Unfocus the omnibox.
  [self unfocusFakeBox];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check the fake omnibox is displayed again at the same position.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  GREYAssertEqual(
      origin.y, collectionView.contentOffset.y,
      @"The collection is not scrolled back to its previous position");
}

// Tests that tapping the fake omnibox and then scrolling defocuses the the
// omnibox.
- (void)testTapFakeOmniboxAndScrollDefocuses {
  // Clear pasteboard so that omnibox doesn't cover the NTP on focus.
  [ChromeEarlGrey clearPasteboard];
  // Get the collection and its layout.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];

  // Offset before the tap.
  CGPoint origin = collectionView.contentOffset;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Offset after the fake omnibox has been tapped.
  CGPoint offsetAfterTap = collectionView.contentOffset;

  // Make sure the fake omnibox has been hidden and the collection has moved.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertTrue(offsetAfterTap.y >= origin.y,
                 @"The collection has not moved.");

  // Scroll up.
  if ([ChromeEarlGrey isIPadIdiom]) {
    // iPad needs more scrolling to see entire fake omnibox since it appears
    // from under the toolbar.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_scrollInDirection(kGREYDirectionUp, 100)];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_scrollInDirection(kGREYDirectionUp, 50)];
  }

  // Check the fake omnibox is displayed again.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that tapping the fake omnibox then unfocusing it moves the collection
// back to where it was.
- (void)testTapFakeOmniboxScrollScrolled {
  // Get the collection and its layout.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];

  // Scroll to have a position different from the default.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 50)];

  // Offset before the tap.
  CGPoint origin = collectionView.contentOffset;

  // Tap the omnibox to focus it.
  [self focusFakebox];

  // Unfocus the omnibox.
  [self unfocusFakeBox];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check the fake omnibox is displayed again at the same position.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The collection might be slightly moved on iPhone.
  GREYAssertTrue(
      collectionView.contentOffset.y >= origin.y &&
          collectionView.contentOffset.y <= origin.y + 6,
      @"The collection is not scrolled back to its previous position");
}

- (void)testOpeningNewTab {
  [ChromeEarlGreyUI openNewTab];

  // Check that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_accessibilityValue(
                            [NSString stringWithFormat:@"%i", 2])];

  // Test the same thing after opening a tab from the tab grid.
  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_accessibilityValue(
                            [NSString stringWithFormat:@"%i", 3])];
}

- (void)testFavicons {
  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }
  // Scroll down if the shortcuts may not be completely in view due to Trending
  // Queries.
  [[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID([NSString
                  stringWithFormat:
                      @"%@0",
                      kContentSuggestionsShortcutsAccessibilityIdentifierPrefix]),
              grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100.0f)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      assertWithMatcher:grey_notNil()];
  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsShortcutsAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Change the Search Engine to Yahoo!.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:grey_accessibilityID(kSettingsSearchEngineCellId)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Yahoo!")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Check again the favicons.
  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }
  // Scroll down if the shortcuts may not be completely in view due to Trending
  // Queries.
  [[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID([NSString
                  stringWithFormat:
                      @"%@0",
                      kContentSuggestionsShortcutsAccessibilityIdentifierPrefix]),
              grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100.0f)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      assertWithMatcher:grey_notNil()];
  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsShortcutsAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Change the Search Engine to Google.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:grey_accessibilityID(kSettingsSearchEngineCellId)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Google")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

- (void)testMinimumHeight {
  [ChromeEarlGreyAppInterface
      setBoolValue:NO
       forUserPref:base::SysUTF8ToNSString(prefs::kArticlesForYouEnabled)];

  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Ensures that tiles are still all visible with feed turned off after
  // scrolling.
  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }
  for (NSInteger index = 0; index < 4; index++) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID([NSString
                stringWithFormat:
                    @"%@%li",
                    kContentSuggestionsShortcutsAccessibilityIdentifierPrefix,
                    index])] assertWithMatcher:grey_sufficientlyVisible()];
  }
  // Ensures that fake omnibox visibility is correct.
  // On iPads, fake omnibox disappears and becomes real omnibox. On other
  // devices, fake omnibox persists and sticks to top.
  if ([ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
        assertWithMatcher:grey_notVisible()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Ensures that logo/doodle is no longer visible when scrolled down.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:notPracticallyVisible()];
}

// Test to ensure that initial position and content are maintained when rotating
// the device back and forth.
- (void)testInitialPositionAndOrientationChange {
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];

  [self testNTPInitialPositionAndContent:collectionView];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];

  [self testNTPInitialPositionAndContent:collectionView];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];

  [self testNTPInitialPositionAndContent:collectionView];
}

// Test to ensure that feed can be collapsed/shown and that feed header changes
// accordingly.
- (void)testToggleFeedVisible {
  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];

  // Check feed label and if NTP is scrollable.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];

  // Hide feed.
  [self hideFeedFromNTPMenu];

  // Check feed label and if NTP is scrollable.
  [self checkFeedLabelForFeedVisible:NO];
  [self checkIfNTPIsScrollable];

  // Show feed again.
  [self showFeedFromNTPMenu];

  // Check feed label and if NTP is scrollable.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];
}

// Test to ensure that feed can be enabled/disabled and that feed header changes
// accordingly.
- (void)testToggleFeedEnabled {
  // Ensure that label is visible with correct text for enabled feed, and that
  // the NTP is scrollable.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];

  // Disable feed.
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                           kSettingsArticleSuggestionsCellId,
                                           /*is_toggled_on=*/YES,
                                           /*enabled=*/YES)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 350)
      onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Ensure that label is no longer visible and that the NTP is still
  // scrollable.
  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_notVisible()];
  [self checkIfNTPIsScrollable];

  // Re-enable feed.
  [ChromeEarlGreyUI openSettingsMenu];
  // Why do we we not reset the scroll position after bringing back the feed as
  // the new parent collectionview? on iphone 8 the logo is cut off.
  [[[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                           kSettingsArticleSuggestionsCellId,
                                           /*is_toggled_on=*/NO,
                                           /*enabled=*/YES)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 350)
      onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Ensure that label is once again visible and that the NTP is still
  // scrollable.
  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];
}

// Test to ensure that NTP for incognito mode works properly.
- (void)testIncognitoMode {
  // Checks that default NTP is not incognito.
  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];

  // Open tools menu and open incognito tab.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kToolsMenuNewIncognitoTabId)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Ensure that incognito view is visible and that the regular NTP is not.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPIncognitoView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      assertWithMatcher:grey_notVisible()];

  // Reload page, then check if incognito view is still visible.
  if ([ChromeEarlGrey isNewOverflowMenuEnabled] &&
      UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad) {
    // In the new
    // overflow menu on iPad, the reload button is only on the toolbar.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ReloadButton()]
        performAction:grey_tap()];
  } else {
    [ChromeEarlGreyUI openToolsMenu];
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kToolsMenuReload)]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPIncognitoView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Shortcuts module is shown when the Magic Stack feature is
// enabled.
- (void)testMagicStackShortcuts {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kMagicStack);
  config.features_disabled.push_back(kWhatsNewIOS);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Verify Module title is visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(l10n_util::GetNSString(
                     IDS_IOS_CONTENT_SUGGESTIONS_SHORTCUTS_MODULE_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check the Bookmarks.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_BOOKMARKS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the ReadingList.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_READING_LIST)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_TOOLS_MENU_READING_LIST)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the RecentTabs.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_RECENT_TABS)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check the History.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_HISTORY)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HeaderWithAccessibilityLabelId(
                                   IDS_HISTORY_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Test that signing in and signing out results in the NTP scrolled to the top
// and not in some unexpected layout state.
// TODO(crbug.com/1433014): Non-stop animation on discover feed after signing
// out.
- (void)FLAKY_testSignInSignOutScrolledToTop {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:identity];
  [SigninEarlGreyUI signinWithFakeIdentity:identity];
  GREYWaitForAppToIdle(@"App failed to idle");

  // Verify Identity Disc is visible since it is the top-most element and should
  // be showing now.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSStringF(
                                   IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL,
                                   base::SysNSStringToUTF16(
                                       identity.userFullName),
                                   base::SysNSStringToUTF16(
                                       identity.userEmail)))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Sign out
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - New Tab menu tests

// Tests the "new search" menu item from the new tab menu.
- (void)testNewSearchFromNewTabMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"New Search is only available in phone layout.");
  }

  [ChromeEarlGreyUI openNewTabMenu];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_TOOLS_MENU_NEW_SEARCH)]
      performAction:grey_tap()];
  GREYWaitForAppToIdle(@"App failed to idle");

  // Check that there's now a new tab, that the new (second) tab is the active
  // one, and the that the omnibox is first responder.
  [ChromeEarlGrey waitForMainTabCount:2];

  GREYAssertEqual(1, [ChromeEarlGrey indexOfActiveNormalTab],
                  @"Tab 1 should be active after starting a new search.");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:grey_notVisible()];
  // Fakebox should be covered.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_notVisible()];
  GREYWaitForAppToIdle(@"App failed to idle");
}

// Tests the "new search" menu item from the new tab menu after disabling the
// feed.
- (void)testNewSearchFromNewTabMenuAfterTogglingFeed {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"New Search is only available in phone layout.");
  }

  // Hide feed.
  [self hideFeedFromNTPMenu];

  [ChromeEarlGreyUI openNewTabMenu];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_TOOLS_MENU_NEW_SEARCH)]
      performAction:grey_tap()];
  GREYWaitForAppToIdle(@"App failed to idle");

  // Check that there's now a new tab, that the new (third) tab is the active
  // one, and the that the omnibox is first responder.
  [ChromeEarlGrey waitForMainTabCount:2];

  GREYAssertEqual(1, [ChromeEarlGrey indexOfActiveNormalTab],
                  @"Tab 1 should be active after starting a new search.");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:grey_notVisible()];

  // Fakebox should be covered.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_notVisible()];
  GREYWaitForAppToIdle(@"App failed to idle");
}

// Tests that the scroll position is maintained when switching from the Discover
// feed to the Following feed without fully scrolling into the feed.
// TODO(crbug.com/1364725): Re-enable when fixed.
- (void)DISABLED_testScrollPositionMaintainedWhenSwitchingFeedAboveFeed {
  if (![ChromeEarlGrey isWebChannelsEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Only applicable with Web Channels enabled.");
  }

  // Sign in to enable Following.
  [self signInThroughSettingsMenu];

  // Scrolls down a bit, not fully into the feed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 50)];

  // Saves the content offset and switches to the Following feed.
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat yOffsetBeforeSwitchingFeed = collectionView.contentOffset.y;

  [[EarlGrey selectElementWithMatcher:FeedHeaderSegmentFollowing()]
      performAction:grey_tap()];

  // Ensures that the new content offset is the same.
  collectionView = [NewTabPageAppInterface collectionView];
  GREYAssertEqual(yOffsetBeforeSwitchingFeed, collectionView.contentOffset.y,
                  @"Content offset is not the same after switching feeds.");
}

// Tests that the regular feed header is visible when signed out, and is swapped
// for the Following feed header after signing in.
// TODO(crbug.com/1364725): Re-enable when fixed.
- (void)DISABLED_testFollowingFeedHeaderIsVisibleWhenSignedIn {
  if (![ChromeEarlGrey isWebChannelsEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Only applicable with Web Channels enabled.");
  }

  // Check that regular feed header is visible when signed out, and not
  // Following header.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPFeedHeaderSegmentedControlIdentifier)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Sign in to enable Following feed.
  [self signInThroughSettingsMenu];

  // Check that Following header is now visible, and not regular feed header.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPFeedHeaderSegmentedControlIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Tests that feed ablation successfully hides the feed from the NTP and the
// toggle from the Chrome settings.
// TODO(crbug.com/1350826): Test fails on small form factors.
- (void)DISABLED_testFeedAblationHidesFeed {
  // Relaunch the app with trending queries disabled, to ensure that the
  // discover feed is always present.
  // TODO(crbug.com/1350826): Trending queries is configured as a
  // first-run trial, and one of the arms removes the discover
  // feed. Fix these tests to force an appropriate configuration or
  // otherwise support the various possible experiment arms.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Ensures that feed header is visible before enabling ablation.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Opens settings menu and ensures that Discover setting is present.
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsArticleSuggestionsCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 250)
      onElementWithMatcher:grey_allOf(
                               grey_accessibilityID(kSettingsTableViewId),
                               grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Relaunch the app with ablation enabled.
  config.features_enabled.push_back(kEnableFeedAblation);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Ensures that feed header is not visible with ablation enabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Opens settings menu and ensures that Discover setting is not present.
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsArticleSuggestionsCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 250)
      onElementWithMatcher:grey_allOf(
                               grey_accessibilityID(kSettingsTableViewId),
                               grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Tests that content suggestions are hidden for supervised users on sign-in.
// When the supervised user signs out the active policy should apply to the NTP.
- (void)testFeedHiddenForSupervisedUser {
  // TODO(crbug.com/1438444): Re-enable the test on iPad once the test is fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad as the test fails.");
  }

  // Disable trending queries experiment to ensure that the Discover feed is
  // visible when first opening the NTP.
  // TODO(crbug.com/1350826): Adapt the test with launch of trending queries.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  // TODO(crbug.com/1403077): Reenable the discover feed sync promo feature
  config.features_disabled.push_back(kEnableDiscoverFeedTopSyncPromo);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self
      testNTPInitialPositionAndContent:[NewTabPageAppInterface collectionView]];

  // Ensure that label is visible with correct text for enabled feed, and that
  // the NTP is scrollable.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];

  // Opens settings menu and ensures that Discover setting is present.
  [self checkDiscoverSettingsToggleVisible:YES];

  // The identity must exist in the test storage to be able to set capabilities
  // through the fake identity service.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:identity];

  ios::CapabilitiesDict* capabilities = @{
    @(kIsSubjectToParentalControlsCapabilityName) :
        @(static_cast<int>(SystemIdentityCapabilityResult::kTrue))
  };
  [SigninEarlGrey setCapabilities:capabilities forIdentity:identity];

  [SigninEarlGreyUI signinWithFakeIdentity:identity];

  // Check feed label and if NTP is scrollable.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DiscoverHeaderLabel()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  [self checkIfNTPIsScrollable];

  // Check that the fake omnibox is visible.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::FakeOmnibox()];

  // Opens settings menu and ensures that Discover setting is not present.
  [self checkDiscoverSettingsToggleVisible:NO];

  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];

  // The feed label should be visible on sign-out.
  [self checkFeedLabelForFeedVisible:YES];
  [self checkIfNTPIsScrollable];

  // Opens settings menu and ensures that Discover setting is present.
  [self checkDiscoverSettingsToggleVisible:YES];
}

// Tests that the feed top sync promo is visible when conditions are met, and
// that pressing the dismiss button makes it disappear.
// TODO(crbug.com/1406885): Enable test when feed is supported.
- (void)DISABLED_testFeedTopSyncPromoIsVisibleAndDismiss {
  // Scroll into feed to trigger engagement condition.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Relaunch the app
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kEnableDiscoverFeedTopSyncPromo);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Scroll down a bit and check that the promo is visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 100)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kSigninPromoViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the dismiss button and check that the promo is no longer visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSigninPromoCloseButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kSigninPromoViewId)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

#pragma mark - Helpers

// Opens the Settings menu and ensures that the visibility of the Discover
// option matches the `visible` parameter.
- (void)checkDiscoverSettingsToggleVisible:(BOOL)visible {
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsArticleSuggestionsCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 250)
      onElementWithMatcher:grey_allOf(
                               grey_accessibilityID(kSettingsTableViewId),
                               grey_sufficientlyVisible(), nil)]
      assertWithMatcher:visible ? grey_notNil() : grey_nil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

- (void)addMostVisitedTile {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // Clear history to ensure the tile will be shown.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  // After loading URL, need to do another action before opening a new tab
  // with the icon present.
  [ChromeEarlGrey goBack];
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

// Taps the fake omnibox and waits for the real omnibox to be visible.
- (void)focusFakebox {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

// Unfocus the omnibox.
- (void)unfocusFakeBox {
  if ([ChromeEarlGrey isIPadIdiom]) {
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:UIKeyInputEscape flags:0];
  } else {
    id<GREYMatcher> cancelButton =
        grey_accessibilityID(kToolbarCancelOmniboxEditButtonIdentifier);
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(cancelButton,
                                            grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];
  }
}

- (void)testNTPInitialPositionAndContent:(UICollectionView*)collectionView {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Check that feed label is visible with correct text for feed visibility.
- (void)checkFeedLabelForFeedVisible:(BOOL)visible {
  NSString* labelTextForVisibleFeed =
      l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE);
  NSString* labelTextForHiddenFeed =
      [NSString stringWithFormat:@"%@ – %@", labelTextForVisibleFeed,
                                 l10n_util::GetNSString(
                                     IDS_IOS_DISCOVER_FEED_TITLE_OFF_LABEL)];
  NSString* labelText =
      visible ? labelTextForVisibleFeed : labelTextForHiddenFeed;
  [EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::DiscoverHeaderLabel(),
                                   grey_sufficientlyVisible(), nil)];
  UILabel* discoverHeaderLabel = [NewTabPageAppInterface discoverHeaderLabel];
  GREYAssertTrue([discoverHeaderLabel.text isEqualToString:labelText],
                 @"Discover header label is incorrect");
}

// Check that NTP is scrollable by scrolling and comparing offsets, then return
// to top.
- (void)checkIfNTPIsScrollable {
  // The custom tab strip on iPad causes an infinite animation that blocks
  // EarlGrey from continuing.
  // TODO(crbug.com/1358829): Remove iPad condition when scrolling is fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  UICollectionView* collectionView = [NewTabPageAppInterface collectionView];
  CGFloat yOffsetBeforeScroll = collectionView.contentOffset.y;
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  GREYAssertTrue(yOffsetBeforeScroll != collectionView.contentOffset.y,
                 @"NTP cannot be scrolled.");

  // Scroll back to top of NTP.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Usually a fast swipe scrolls back up, but in case it doesn't, make sure
  // by scrolling again, then slowly scrolling to the top.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
}

- (void)showFeedFromNTPMenu {
  bool feed_visible =
      [ChromeEarlGrey userBooleanPref:feed::prefs::kArticlesListVisible];
  GREYAssertFalse(feed_visible, @"Expect feed to be hidden!");

  // The feed header button may be offscreen, so scroll to find it if needed.
  id<GREYMatcher> headerButton =
      grey_allOf(grey_accessibilityID(kNTPFeedHeaderMenuButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:headerButton]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100.0f)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NTPFeedMenuEnableButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  feed_visible =
      [ChromeEarlGrey userBooleanPref:feed::prefs::kArticlesListVisible];
  GREYAssertTrue(feed_visible, @"Expect feed to be visible!");
}

- (void)hideFeedFromNTPMenu {
  bool feed_visible =
      [ChromeEarlGrey userBooleanPref:feed::prefs::kArticlesListVisible];
  GREYAssertTrue(feed_visible, @"Expect feed to be visible!");

  // The feed header button may be offscreen, so scroll to find it if needed.
  id<GREYMatcher> headerButton =
      grey_allOf(grey_accessibilityID(kNTPFeedHeaderMenuButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:headerButton]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100.0f)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NTPFeedMenuDisableButton()]
      performAction:grey_tap()];
  // This ensures that the app is given time to update the pref before checking
  // its state.
  [ChromeEarlGreyUI waitForAppToIdle];
  feed_visible =
      [ChromeEarlGrey userBooleanPref:feed::prefs::kArticlesListVisible];
  GREYAssertFalse(feed_visible, @"Expect feed to be hidden!");
}

// Opens the settings menu, signs in using a fake identity, and closes the menu.
- (void)signInThroughSettingsMenu {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey selectElementWithMatcher:chrome_test_util::PrimarySignInButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionUp, 150)
      onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];
  [ChromeEarlGrey waitForMatcher:chrome_test_util::SettingsAccountButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Matchers

// Returns the segment of the feed header with a given title.
id<GREYMatcher> FeedHeaderSegmentedControlSegmentWithTitle(int title_id) {
  id<GREYMatcher> title_matcher =
      grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(title_id)),
                 grey_sufficientlyVisible(), nil);
  return grey_allOf(
      grey_accessibilityID(kNTPFeedHeaderSegmentedControlIdentifier),
      grey_descendant(title_matcher), grey_sufficientlyVisible(), nil);
}

// Returns the Discover segment of the feed header.
id<GREYMatcher> FeedHeaderSegmentDiscover() {
  return FeedHeaderSegmentedControlSegmentWithTitle(
      IDS_IOS_DISCOVER_FEED_TITLE);
}

// Returns the Following segment of the feed header.
id<GREYMatcher> FeedHeaderSegmentFollowing() {
  return FeedHeaderSegmentedControlSegmentWithTitle(
      IDS_IOS_FOLLOWING_FEED_TITLE);
}

@end
