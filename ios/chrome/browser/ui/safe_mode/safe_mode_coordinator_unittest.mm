// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/safe_mode/safe_mode_coordinator.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using SafeModeCoordinatorTest = PlatformTest;

TEST_F(SafeModeCoordinatorTest, RootVC) {
  // Expect that starting a safe mode coordinator will populate the root view
  // controller.
  UIWindow* window =
      [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  EXPECT_TRUE([window rootViewController] == nil);
  SafeModeCoordinator* safe_mode_coordinator =
      [[SafeModeCoordinator alloc] initWithWindow:window];
  [safe_mode_coordinator start];
  EXPECT_FALSE([window rootViewController] == nil);
}
