// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class NewTabPageBottomSheetViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[NewTabPageBottomSheetViewController alloc] init];
  }

 protected:
  NewTabPageBottomSheetViewController* view_controller_;
};

// Tests that the view controller loads its view correctly.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestLoadView) {
  [view_controller_ loadViewIfNeeded];
  EXPECT_NE(nil, view_controller_.view);
}

// Tests that tapping the fake location bar invokes the delegate method.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestDelegateCallback) {
  id delegate_mock =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = delegate_mock;

  OCMExpect([delegate_mock
      bottomSheetViewControllerDidTapFakeLocationBar:view_controller_]);

  [view_controller_ loadViewIfNeeded];

  // Find fake location bar in subviews by accessibilityIdentifier and simulate
  // tap.
  UIView* contentView =
      ((UIVisualEffectView*)view_controller_.view).contentView;
  UIControl* fakeLocationBar = nil;
  for (UIView* subview in contentView.subviews) {
    if ([subview.accessibilityIdentifier
            isEqualToString:@"ntp-redesign-fake-omnibox"] &&
        [subview isKindOfClass:[UIControl class]]) {
      fakeLocationBar = static_cast<UIControl*>(subview);
      break;
    }
  }
  EXPECT_NE(nil, fakeLocationBar);
  [fakeLocationBar sendActionsForControlEvents:UIControlEventTouchUpInside];

  [delegate_mock verify];
}
