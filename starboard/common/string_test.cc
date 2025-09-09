// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/string.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr int kDestSize = 10;

TEST(FormatStringTest, HandlesEmptyFormatString) {
  EXPECT_EQ("", FormatString(""));
}

TEST(FormatStringTest, HandlesFormatStringWithNoArguments) {
  EXPECT_EQ("Hello", FormatString("Hello"));
}

TEST(FormatStringTest, FormatsStringWithSingleStringArgument) {
  EXPECT_EQ("Hello, World!", FormatString("Hello, %s!", "World"));
}

TEST(FormatStringTest, FormatsStringWithSingleIntegerArgument) {
  EXPECT_EQ("Value: 42", FormatString("Value: %d", 42));
}

TEST(FormatStringTest, FormatsStringWithMultipleArguments) {
  EXPECT_EQ("Test: a, 1, b", FormatString("Test: %c, %d, %c", 'a', 1, 'b'));
}

TEST(FormatStringTest, HandlesEmptyStringArgument) {
  EXPECT_EQ("", FormatString("%s", ""));
}

TEST(HexEncodeTest, EncodesDataWithoutDelimiter) {
  const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  EXPECT_EQ("deadbeef", HexEncode(data, sizeof(data)));
}

TEST(HexEncodeTest, EncodesDataWithDelimiter) {
  const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  EXPECT_EQ("de:ad:be:ef", HexEncode(data, sizeof(data), ":"));
}

TEST(HexEncodeTest, HandlesEmptyData) {
  EXPECT_EQ("", HexEncode(nullptr, 0));
}

TEST(HexEncodeTest, EncodesSingleByte) {
  const uint8_t data[] = {0xFF};
  EXPECT_EQ("ff", HexEncode(data, sizeof(data)));
}

TEST(StrlcpyTest, CopiesFullStringSuccessfully) {
  char dest[kDestSize];

  int len = strlcpy(dest, "test", kDestSize);

  EXPECT_STREQ("test", dest);
  EXPECT_EQ(4, len);
}

TEST(StrlcpyTest, TruncatesWhenSourceIsTooLong) {
  char dest[kDestSize];

  int len = strlcpy(dest, "this is too long", kDestSize);

  EXPECT_STREQ("this is t", dest);
  EXPECT_EQ(16, len);
}

TEST(StrlcpyTest, HandlesEmptySourceString) {
  char dest[kDestSize];

  int len = strlcpy(dest, "", kDestSize);

  EXPECT_STREQ("", dest);
  EXPECT_EQ(0, len);
}

TEST(StrlcpyTest, HandlesZeroSizedDestination) {
  char dest[1];  // Dummy buffer, won't be written to.

  int len = strlcpy(dest, "non-empty", 0);

  EXPECT_EQ(9, len);
}

TEST(StrlcpyTest, HandlesDestinationSizeOfOne) {
  char dest[1];

  int len = strlcpy(dest, "test", 1);

  EXPECT_STREQ("", dest);
  EXPECT_EQ(4, len);
}

TEST(StrlcatTest, ConcatenatesSuccessfully) {
  char dest[kDestSize];
  strlcpy(dest, "abc", kDestSize);

  int len = strlcat(dest, "def", kDestSize);

  EXPECT_STREQ("abcdef", dest);
  EXPECT_EQ(6, len);
}

TEST(StrlcatTest, TruncatesWhenResultIsTooLong) {
  char dest[kDestSize];
  strlcpy(dest, "12345", kDestSize);

  int len = strlcat(dest, "67890abc", kDestSize);

  EXPECT_STREQ("123456789", dest);
  EXPECT_EQ(13, len);
}

TEST(StrlcatTest, ConcatenatesToEmptyString) {
  char dest[kDestSize];
  dest[0] = '\0';

  int len = strlcat(dest, "test", kDestSize);

  EXPECT_STREQ("test", dest);
  EXPECT_EQ(4, len);
}

TEST(StrlcatTest, ConcatenatesEmptyString) {
  char dest[kDestSize];
  strlcpy(dest, "test", kDestSize);

  int len = strlcat(dest, "", kDestSize);

  EXPECT_STREQ("test", dest);
  EXPECT_EQ(4, len);
}

TEST(StringTest, FormatWithDigitSeparators_Zero) {
  EXPECT_EQ("0", FormatWithDigitSeparators(0));
}

TEST(StringTest, FormatWithDigitSeparators_PositiveNumber) {
  EXPECT_EQ("123", FormatWithDigitSeparators(123));
  EXPECT_EQ("1'234", FormatWithDigitSeparators(1234));
  EXPECT_EQ("12'345", FormatWithDigitSeparators(12345));
  EXPECT_EQ("123'456", FormatWithDigitSeparators(123456));
  EXPECT_EQ("1'234'567", FormatWithDigitSeparators(1234567));
  EXPECT_EQ("12'345'678", FormatWithDigitSeparators(12345678));
  EXPECT_EQ("123'456'789", FormatWithDigitSeparators(123456789));
}

TEST(StringTest, FormatWithDigitSeparators_NegativeNumber) {
  EXPECT_EQ("-123", FormatWithDigitSeparators(-123));
  EXPECT_EQ("-1'234", FormatWithDigitSeparators(-1234));
  EXPECT_EQ("-12'345", FormatWithDigitSeparators(-12345));
  EXPECT_EQ("-123'456", FormatWithDigitSeparators(-123456));
  EXPECT_EQ("-1'234'567", FormatWithDigitSeparators(-1234567));
  EXPECT_EQ("-12'345'678", FormatWithDigitSeparators(-12345678));
  EXPECT_EQ("-123'456'789", FormatWithDigitSeparators(-123456789));
}

}  // namespace
}  // namespace starboard
