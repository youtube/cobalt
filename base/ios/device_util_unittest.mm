// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/ios/device_util.h"
#include "base/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {
// The behavior of most of these utility functions depends on what they are run
// on, so there is not much to unittest them. The APIs are run to make sure they
// don't choke. Additional checks are added for particular APIs when needed.

typedef PlatformTest DeviceUtilTest;

TEST_F(DeviceUtilTest, GetPlatform) {
  GTEST_ASSERT_GT(ios::device_util::GetPlatform().length(), 0U);
}

TEST_F(DeviceUtilTest, IsRunningOnHighRamDevice) {
  ios::device_util::IsRunningOnHighRamDevice();
}

TEST_F(DeviceUtilTest, IsSingleCoreDevice) {
  ios::device_util::IsSingleCoreDevice();
}

TEST_F(DeviceUtilTest, GetMacAddress) {
  GTEST_ASSERT_GT(ios::device_util::GetMacAddress("en0").length(), 0U);
}

TEST_F(DeviceUtilTest, GetRandomId) {
  GTEST_ASSERT_GT(ios::device_util::GetRandomId().length(), 0U);
}

TEST_F(DeviceUtilTest, GetDeviceIdentifier) {
  std::string default_id = ios::device_util::GetDeviceIdentifier(NULL);
  std::string other_id = ios::device_util::GetDeviceIdentifier("ForTest");
  EXPECT_NE(default_id, other_id);

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:@"ChromiumClientID"];
  [defaults synchronize];

  std::string new_default_id = ios::device_util::GetDeviceIdentifier(NULL);
  EXPECT_NE(default_id, new_default_id);
}

}  // namespace
