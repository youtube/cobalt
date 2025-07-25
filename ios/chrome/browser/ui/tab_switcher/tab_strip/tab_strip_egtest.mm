// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

// Tests for the tab strip shown on iPad.
@interface TabStripTestCase : ChromeTestCase
@end

@implementation TabStripTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kModernTabStrip);
  return config;
}

// Tests switching tabs using the tab strip.
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

@end
