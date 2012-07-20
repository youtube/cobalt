// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/path_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

typedef testing::Test PathUtilsTest;

TEST_F(PathUtilsTest, TestGetDataDirectory) {
  // The string comes from the Java side and depends on the APK
  // we are running in. Assumes that we are packaged in
  // org.chromium.native_test
  EXPECT_STREQ("/data/data/org.chromium.native_test/app_chrome",
               GetDataDirectory().c_str());
}

TEST_F(PathUtilsTest, TestGetCacheDirectory) {
  // The string comes from the Java side and depends on the APK
  // we are running in. Assumes that we are packaged in
  // org.chromium.native_test
  EXPECT_STREQ("/data/data/org.chromium.native_test/cache",
               GetCacheDirectory().c_str());
}

TEST_F(PathUtilsTest, TestGetNativeLibraryDirectory) {
  // The string comes from the Java side and depends on the APK
  // we are running in. Assumes that we are packaged in
  // org.chromium.native_test
  EXPECT_STREQ("/data/data/org.chromium.native_test/lib",
               GetNativeLibraryDirectory().c_str());
}

}  // namespace android
}  // namespace base
