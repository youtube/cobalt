// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/ui/tabs/tab_strip_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::LongPressLinkAndDragToView;

namespace {

// URL to a destination page with a static message.
const char kDestinationPageUrl[] = "/destination";
// HTML content of the destination page.
const char kDestinationHtml[] =
    "<html><head><meta name='viewport' content='width=device-width, "
    "initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
    "/></head><body><script>document.title='new doc'</script>"
    "<center><span id=\"message\">You made it!</span></center>"
    "</body></html>";

// URL to an initial page with a link to the destination page.
const char kInitialPageUrl[] = "/scenarioDragURLToTabStrip";
// HTML content of an initial page with a link to the destination page.
// Note that this string contains substrings that must exactly match the next
// two constants (`kInitialPageLinkId` and `kInitialPageDestinationLinkText`).
// If these were more complex or more liable to change, a sprintf template and
// runtime composition should be used instead.
const char kInitialPageHtml[] =
    "<html><head><meta name='viewport' content='width=device-width, "
    "initial-scale=1.0, maximum-scale=1.0, user-scalable=no' /></head><body><a "
    "style='margin-left:150px' href='/destination' id='link' aria-label='link'>"
    "link</a></body></html>";

// Accessibility ID of the link on the initial page. The `aria-label` attribute
// makes this visible to UIKit as an accessibility ID.
NSString* const kInitialPageLinkId = @"link";

// The text of the link to the destination page.
const char kInitialPageDestinationLinkText[] = "link";

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kInitialPageUrl) {
    http_response->set_content(kInitialPageHtml);
  } else if (request.relative_url == kDestinationPageUrl) {
    http_response->set_content(kDestinationHtml);
  } else {
    return nullptr;
  }

  return std::move(http_response);
}

}

// Tests for the tab strip shown on iPad.
@interface TabStripTestCase : ChromeTestCase
@end

@implementation TabStripTestCase

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Test switching tabs using the tab strip.
- (void)testTabStripSwitchTabs {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // TODO(crbug.com/238112):  Make this test also handle the 'collapsed' tab
  // case.
  const int kNumberOfTabs = 3;
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Note that the tab ordering wraps.  E.g. if A, B, and C are open,
  // and C is the current tab, the 'next' tab is 'A'.
  for (int i = 0; i < kNumberOfTabs + 1; i++) {
    GREYAssertTrue([ChromeEarlGrey mainTabCount] > 1,
                   [ChromeEarlGrey mainTabCount] ? @"Only one tab open."
                                                 : @"No more tabs.");
    NSString* nextTabTitle = [ChromeEarlGrey nextTabTitle];

    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_text(nextTabTitle),
                                            grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];

    GREYAssertEqualObjects([ChromeEarlGrey currentTabTitle], nextTabTitle,
                           @"The selected tab did not change to the next tab.");
  }
}

// Tests dragging URL into regular tab strip.
- (void)testDragAndDropURLIntoRegularTabStrip {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Drag and drop the link on the page to the tab strip. This should open
  // a new regular tab.
  GREYAssert(LongPressLinkAndDragToView(kInitialPageLinkId, kRegularTabStripId),
             @"Failed to drag link to tab strip");

  GREYWaitForAppToIdle(@"App failed to idle");

  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests dragging URL into incognito tab strip.
- (void)testDragAndDropURLIntoIncognitoTabStrip {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey openNewIncognitoTab];

  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Drag and drop the link on the page to the tab strip. This should open
  // a new incognito tab.
  GREYAssert(
      LongPressLinkAndDragToView(kInitialPageLinkId, kIncognitoTabStripId),
      @"Failed to drag link to tab strip");

  GREYWaitForAppToIdle(@"App failed to idle");

  [ChromeEarlGrey waitForIncognitoTabCount:2];
}

@end
