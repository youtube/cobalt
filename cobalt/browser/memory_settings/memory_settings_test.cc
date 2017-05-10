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

TEST(IntSetting, ParseFromString) {
  scoped_ptr<IntSetting> int_setting(new IntSetting("dummy"));
  EXPECT_EQ(std::string("dummy"), int_setting->name());
  ASSERT_TRUE(int_setting->TryParseValue(MemorySetting::kCmdLine, "123"));
  EXPECT_EQ(123, int_setting->value());
  EXPECT_EQ(MemorySetting::kCmdLine, int_setting->source_type());
  EXPECT_EQ(std::string("123"), int_setting->ValueToString());
}

TEST(IntSetting, ParseFromKbString) {
  scoped_ptr<IntSetting> int_setting(new IntSetting("dummy"));
  EXPECT_EQ(std::string("dummy"), int_setting->name());
  std::vector<std::string> one_kb_values;
  one_kb_values.push_back("1KB");
  one_kb_values.push_back("1Kb");
  one_kb_values.push_back("1kB");
  one_kb_values.push_back("1kb");

  for (size_t i = 0; i < one_kb_values.size(); ++i) {
    const std::string& value = one_kb_values[i];
    ASSERT_TRUE(int_setting->TryParseValue(MemorySetting::kCmdLine, value));
    EXPECT_EQ(1024, int_setting->value());
    EXPECT_EQ(MemorySetting::kCmdLine, int_setting->source_type());
    EXPECT_EQ(std::string("1024"), int_setting->ValueToString());
  }
}

TEST(IntSetting, ParseFromMbString) {
  scoped_ptr<IntSetting> int_setting(new IntSetting("dummy"));
  EXPECT_EQ(std::string("dummy"), int_setting->name());
  std::vector<std::string> one_mb_values;
  one_mb_values.push_back("1MB");
  one_mb_values.push_back("1Mb");
  one_mb_values.push_back("1mB");
  one_mb_values.push_back("1mb");
  for (size_t i = 0; i < one_mb_values.size(); ++i) {
    const std::string& value = one_mb_values[i];
    ASSERT_TRUE(int_setting->TryParseValue(MemorySetting::kCmdLine, value));
    EXPECT_EQ(1024*1024, int_setting->value());
    EXPECT_EQ(MemorySetting::kCmdLine, int_setting->source_type());
    EXPECT_EQ(std::string("1048576"), int_setting->ValueToString());
  }
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
