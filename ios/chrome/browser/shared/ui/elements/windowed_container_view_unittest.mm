// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/windowed_container_view.h"

#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Fixture to test WindowedContainerView.
class WindowedContainerViewTest : public PlatformTest {
 public:
  WindowedContainerViewTest()
      : windowed_container_([[WindowedContainerView alloc] init]) {}

 protected:
  ScopedKeyWindow scoped_key_window_;
  WindowedContainerView* windowed_container_ = nil;
};

// Tests that the container correctly adds itself to the window as hidden and
// resigns it's subview's first responder status.
TEST_F(WindowedContainerViewTest, Foo) {
  ASSERT_NE(scoped_key_window_.Get(), windowed_container_.window);

  UITextField* text_field = [[UITextField alloc] init];
  [scoped_key_window_.Get() addSubview:text_field];
  [text_field becomeFirstResponder];
  ASSERT_TRUE([text_field isFirstResponder]);

  [windowed_container_ addSubview:text_field];
  EXPECT_EQ(scoped_key_window_.Get(), windowed_container_.window);
  ASSERT_TRUE(windowed_container_.hidden);
  ASSERT_FALSE([text_field isFirstResponder]);
}

}  // namespace
