// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/xhr/xhr_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace xhr {
namespace {

TEST(XhrSettingsImplTest, Empty) {
  XhrSettingsImpl impl;

  EXPECT_FALSE(impl.IsFetchBufferPoolEnabled());
  EXPECT_FALSE(impl.GetDefaultFetchBufferSize());
  EXPECT_FALSE(impl.IsTryLockForProgressCheckEnabled());
}

TEST(XhrSettingsImplTest, SunnyDay) {
  XhrSettingsImpl impl;

  ASSERT_TRUE(impl.Set("XHR.EnableFetchBufferPool", 1));
  ASSERT_TRUE(impl.Set("XHR.DefaultFetchBufferSize", 1024 * 1024));
  ASSERT_TRUE(impl.Set("XHR.EnableTryLockForProgressCheck", 1));

  EXPECT_TRUE(impl.IsFetchBufferPoolEnabled().value());
  EXPECT_EQ(impl.GetDefaultFetchBufferSize().value(), 1024 * 1024);
  EXPECT_TRUE(impl.IsTryLockForProgressCheckEnabled());
}

TEST(XhrSettingsImplTest, RainyDay) {
  XhrSettingsImpl impl;

  ASSERT_FALSE(impl.Set("XHR.EnableFetchBufferPool", 2));
  ASSERT_FALSE(impl.Set("XHR.DefaultFetchBufferSize", -100));
  ASSERT_FALSE(impl.Set("XHR.EnableTryLockForProgressCheck", 3));

  EXPECT_FALSE(impl.IsFetchBufferPoolEnabled());
  EXPECT_FALSE(impl.GetDefaultFetchBufferSize());
  EXPECT_FALSE(impl.IsTryLockForProgressCheckEnabled());
}

TEST(XhrSettingsImplTest, ZeroValuesWork) {
  XhrSettingsImpl impl;

  ASSERT_TRUE(impl.Set("XHR.EnableFetchBufferPool", 0));
  // O is an invalid value for "XHR.DefaultFetchBufferSize".
  ASSERT_TRUE(impl.Set("XHR.EnableTryLockForProgressCheck", 0));

  EXPECT_FALSE(impl.IsFetchBufferPoolEnabled().value());
  EXPECT_FALSE(impl.IsTryLockForProgressCheckEnabled().value());
}

TEST(XhrSettingsImplTest, Updatable) {
  XhrSettingsImpl impl;

  ASSERT_TRUE(impl.Set("XHR.EnableFetchBufferPool", 0));
  ASSERT_TRUE(impl.Set("XHR.DefaultFetchBufferSize", 1024));
  ASSERT_TRUE(impl.Set("XHR.EnableTryLockForProgressCheck", 0));

  ASSERT_TRUE(impl.Set("XHR.EnableFetchBufferPool", 1));
  ASSERT_TRUE(impl.Set("XHR.DefaultFetchBufferSize", 1024 * 2));
  ASSERT_TRUE(impl.Set("XHR.EnableTryLockForProgressCheck", 1));

  EXPECT_TRUE(impl.IsFetchBufferPoolEnabled().value());
  EXPECT_EQ(impl.GetDefaultFetchBufferSize().value(), 1024 * 2);
  EXPECT_TRUE(impl.IsTryLockForProgressCheckEnabled().value());
}

TEST(XhrSettingsImplTest, InvalidSettingNames) {
  XhrSettingsImpl impl;

  ASSERT_FALSE(impl.Set("XHR.Invalid", 0));
  ASSERT_FALSE(impl.Set("Invalid.EnableFetchBufferPool", 0));
}

}  // namespace
}  // namespace xhr
}  // namespace cobalt
