/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/browser/memory_settings/memory_settings.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cobalt/browser/memory_settings/test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {
int64_t TestIntSettingParse(const std::string& value) {
  IntSetting int_setting("dummy");
  EXPECT_TRUE(int_setting.TryParseValue(MemorySetting::kCmdLine, value));
  return int_setting.value();
}
}  // namespace

TEST(IntSetting, ParseFromString) {
  scoped_ptr<IntSetting> int_setting(new IntSetting("dummy"));
  EXPECT_EQ(std::string("dummy"), int_setting->name());
  ASSERT_TRUE(int_setting->TryParseValue(MemorySetting::kCmdLine, "123"));
  EXPECT_EQ(123, int_setting->value());
  EXPECT_EQ(MemorySetting::kCmdLine, int_setting->source_type());
  EXPECT_EQ(std::string("123"), int_setting->ValueToString());
}

// Tests the expectation that numerous string variations (whole numbers vs
// fractions vs units) parse correctly.
TEST(IntSetting, ParseFromStrings) {
  // Bytes.
  EXPECT_EQ(1, TestIntSettingParse("1"));
  EXPECT_EQ(1, TestIntSettingParse("1B"));
  EXPECT_EQ(1, TestIntSettingParse("1b"));
  EXPECT_EQ(1, TestIntSettingParse("1B"));
  EXPECT_EQ(1, TestIntSettingParse("1b"));

  // Kilobytes and fractional amounts.
  EXPECT_EQ(1024, TestIntSettingParse("1KB"));
  EXPECT_EQ(1024, TestIntSettingParse("1Kb"));
  EXPECT_EQ(1024, TestIntSettingParse("1kB"));
  EXPECT_EQ(1024, TestIntSettingParse("1kb"));

  EXPECT_EQ(512, TestIntSettingParse(".5kb"));
  EXPECT_EQ(512, TestIntSettingParse("0.5kb"));
  EXPECT_EQ(1536, TestIntSettingParse("1.5kb"));
  EXPECT_EQ(1536, TestIntSettingParse("1.50kb"));

  // Megabytes and fractional amounts.
  EXPECT_EQ(1024*1024, TestIntSettingParse("1MB"));
  EXPECT_EQ(1024*1024, TestIntSettingParse("1Mb"));
  EXPECT_EQ(1024*1024, TestIntSettingParse("1mB"));
  EXPECT_EQ(1024*1024, TestIntSettingParse("1mb"));

  EXPECT_EQ(512*1024, TestIntSettingParse(".5mb"));
  EXPECT_EQ(512*1024, TestIntSettingParse("0.5mb"));
  EXPECT_EQ(1536*1024, TestIntSettingParse("1.5mb"));
  EXPECT_EQ(1536*1024, TestIntSettingParse("1.50mb"));

  // Gigabytes and fractional amounts.
  EXPECT_EQ(1024*1024*1024, TestIntSettingParse("1GB"));
  EXPECT_EQ(1024*1024*1024, TestIntSettingParse("1Gb"));
  EXPECT_EQ(1024*1024*1024, TestIntSettingParse("1gB"));
  EXPECT_EQ(1024*1024*1024, TestIntSettingParse("1gb"));

  EXPECT_EQ(512*1024*1024, TestIntSettingParse(".5gb"));
  EXPECT_EQ(1536*1024*1024, TestIntSettingParse("1.50gb"));
}

TEST(DimensionSetting, ParseFromString) {
  scoped_ptr<DimensionSetting> rect_setting(new TestDimensionSetting("dummy"));
  ASSERT_TRUE(
      rect_setting->TryParseValue(MemorySetting::kCmdLine, "1234x5678"));
  EXPECT_EQ(TextureDimensions(1234, 5678, 2), rect_setting->value());
  EXPECT_EQ(MemorySetting::kCmdLine, rect_setting->source_type());
  EXPECT_EQ(std::string("1234x5678x2"), rect_setting->ValueToString());
}

TEST(DimensionSetting, ParseFromStringCaseInsensitive) {
  scoped_ptr<DimensionSetting> rect_setting(new TestDimensionSetting("dummy"));
  ASSERT_TRUE(
      rect_setting->TryParseValue(MemorySetting::kCmdLine, "1234X5678"));
  EXPECT_EQ(TextureDimensions(1234, 5678, 2), rect_setting->value());
  EXPECT_EQ(MemorySetting::kCmdLine, rect_setting->source_type());
  EXPECT_EQ(std::string("1234x5678x2"), rect_setting->ValueToString());
}

TEST(DimensionSetting, ParseFromStringWithBytesPerPixel) {
  scoped_ptr<DimensionSetting> rect_setting(new TestDimensionSetting("dummy"));
  ASSERT_TRUE(
      rect_setting->TryParseValue(MemorySetting::kCmdLine, "1234x5678x12"));
  EXPECT_EQ(TextureDimensions(1234, 5678, 12), rect_setting->value());
  EXPECT_EQ(MemorySetting::kCmdLine, rect_setting->source_type());
  EXPECT_EQ(std::string("1234x5678x12"), rect_setting->ValueToString());
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
