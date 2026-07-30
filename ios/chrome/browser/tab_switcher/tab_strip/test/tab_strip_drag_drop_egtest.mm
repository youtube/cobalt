// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using base::test::ios::kWaitForUIElementTimeout;
using ::base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Identifer for cell at given `index` in the tab grid.
NSString* IdentifierForRegularCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u",
                                    TabStripCollectionViewConstants
                                        .tabStripTabCellPrefixIdentifier,
                                    index];
}

// Matcher for the reguar cell at the given `index`.
id<GREYMatcher> RegularCellAtIndex(unsigned int index) {
  return grey_allOf(
      grey_accessibilityID(IdentifierForRegularCellAtIndex(index)),
      grey_kindOfClassName(@"TabStripTabCell"), grey_sufficientlyVisible(),
      nil);
}



// Checks that the regular cell matching `tab_title` moved to `tab_index`
void AssertRegularCellMovedToNewPosition(unsigned int tab_index,
                                         NSString* tab_title) {
  ConditionBlock condition = ^{
    NSError* error = nil;

    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     RegularCellAtIndex(tab_index),
                                     grey_descendant(grey_text(tab_title)),
                                     nil)] assertWithMatcher:grey_notNil()
                                                       error:&error];
    return !error;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"The drag drop action has failed.");
}

}  // namespace

// Tests for the tab strip drag drop interactions on iPad.
@interface TabStripDragDropTestCase : ChromeTestCase
@end

@implementation TabStripDragDropTestCase

// Checks that dragging a regular cell to a new position correctly moves the
// cell.
- (void)testDragTabStripTabCellInTabStripView {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Tab0: New Tab
  // Tab1: Chrome URLs
  // Tab2: About Version

  // Move Tab0 to Tab2.
  chrome_test_util::LongPressCellAndDragToOffsetOf(
      IdentifierForRegularCellAtIndex(0), /*src_window_number=*/0,
      IdentifierForRegularCellAtIndex(2), /*dst_window_number=*/0,
      CGVectorMake(0.5, 0.5));
  AssertRegularCellMovedToNewPosition(/*tab_index*/ 2,
                                      /*tab_title*/ @"New Tab");

  // Tab0: Chrome URLs
  // Tab1: About Version
  // Tab2: New Tab

  // Move Tab1 to Tab0.
  chrome_test_util::LongPressCellAndDragToOffsetOf(
      IdentifierForRegularCellAtIndex(1), /*src_window_number=*/0,
      IdentifierForRegularCellAtIndex(0), /*dst_window_number=*/0,
      CGVectorMake(0.5, 0.5));
  AssertRegularCellMovedToNewPosition(/*tab_index*/ 0,
                                      /*tab_title*/ @"About Version");
}

@end
