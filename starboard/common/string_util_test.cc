// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/string_util.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(StringUtilTest, StartsWith) {
  EXPECT_TRUE(StartsWith("vp09.00.41.08", "vp09."));
  EXPECT_TRUE(StartsWith("vp09", "vp09"));
  EXPECT_TRUE(StartsWith("vp09", ""));
  EXPECT_TRUE(StartsWith("", ""));
  EXPECT_FALSE(StartsWith("vp9", "vp09"));
  EXPECT_FALSE(StartsWith("", "vp09"));
  EXPECT_FALSE(StartsWith("avc1.640028", "avc3."));
  // Only the characters in the view are compared.
  EXPECT_FALSE(StartsWith(std::string_view("vp09.", 4), "vp09."));
}

TEST(StringUtilTest, EndsWith) {
  EXPECT_TRUE(EndsWith("libcobalt.lz4", ".lz4"));
  EXPECT_TRUE(EndsWith(".lz4", ".lz4"));
  EXPECT_TRUE(EndsWith("lz4", ""));
  EXPECT_TRUE(EndsWith("", ""));
  EXPECT_FALSE(EndsWith("lz4", ".lz4"));
  EXPECT_FALSE(EndsWith("", ".lz4"));
  EXPECT_FALSE(EndsWith("libcobalt.zst", ".lz4"));
}

TEST(StringUtilTest, IsAsciiWhitespace) {
  static_assert(IsAsciiWhitespace(' '), "");
  static_assert(IsAsciiWhitespace('\t'), "");
  static_assert(IsAsciiWhitespace('\n'), "");
  static_assert(IsAsciiWhitespace('\r'), "");
  static_assert(IsAsciiWhitespace('\f'), "");
  static_assert(IsAsciiWhitespace('\v'), "");
  static_assert(!IsAsciiWhitespace('\0'), "");
  static_assert(!IsAsciiWhitespace('\x08'), "");
  static_assert(!IsAsciiWhitespace('\x0e'), "");
  static_assert(!IsAsciiWhitespace('a'), "");
  static_assert(!IsAsciiWhitespace('0'), "");
  static_assert(!IsAsciiWhitespace(static_cast<char>(0xa0)), "");
  static_assert(!IsAsciiWhitespace(static_cast<char>(0xff)), "");

  EXPECT_TRUE(IsAsciiWhitespace(' '));
  EXPECT_FALSE(IsAsciiWhitespace(static_cast<char>(0xa0)));
  EXPECT_FALSE(IsAsciiWhitespace(static_cast<char>(0xff)));
}

TEST(StringUtilTest, IsAsciiUpperAndLower) {
  static_assert(IsAsciiUpper('A'), "");
  static_assert(IsAsciiUpper('Z'), "");
  static_assert(!IsAsciiUpper('@'), "");
  static_assert(!IsAsciiUpper('['), "");
  static_assert(!IsAsciiUpper('a'), "");
  static_assert(!IsAsciiUpper(static_cast<char>(0xff)), "");

  static_assert(IsAsciiLower('a'), "");
  static_assert(IsAsciiLower('z'), "");
  static_assert(!IsAsciiLower('`'), "");
  static_assert(!IsAsciiLower('{'), "");
  static_assert(!IsAsciiLower('A'), "");
  static_assert(!IsAsciiLower(static_cast<char>(0xff)), "");

  for (char c = 'A'; c <= 'Z'; ++c) {
    EXPECT_TRUE(IsAsciiUpper(c));
    EXPECT_FALSE(IsAsciiLower(c));
  }
  for (char c = 'a'; c <= 'z'; ++c) {
    EXPECT_TRUE(IsAsciiLower(c));
    EXPECT_FALSE(IsAsciiUpper(c));
  }
}

TEST(StringUtilTest, IsAsciiAlpha) {
  static_assert(IsAsciiAlpha('A'), "");
  static_assert(IsAsciiAlpha('Z'), "");
  static_assert(IsAsciiAlpha('a'), "");
  static_assert(IsAsciiAlpha('z'), "");
  static_assert(!IsAsciiAlpha('@'), "");
  static_assert(!IsAsciiAlpha('['), "");
  static_assert(!IsAsciiAlpha('`'), "");
  static_assert(!IsAsciiAlpha('{'), "");
  static_assert(!IsAsciiAlpha('0'), "");
  static_assert(!IsAsciiAlpha(static_cast<char>(0x80)), "");
  static_assert(!IsAsciiAlpha(static_cast<char>(0xc3)), "");
  static_assert(!IsAsciiAlpha(static_cast<char>(0xff)), "");

  for (char c = 'A'; c <= 'Z'; ++c) {
    EXPECT_TRUE(IsAsciiAlpha(c));
  }
  for (char c = 'a'; c <= 'z'; ++c) {
    EXPECT_TRUE(IsAsciiAlpha(c));
  }
  EXPECT_FALSE(IsAsciiAlpha('5'));
  EXPECT_FALSE(IsAsciiAlpha(static_cast<char>(0xff)));
}

TEST(StringUtilTest, IsAsciiDigit) {
  static_assert(IsAsciiDigit('0'), "");
  static_assert(IsAsciiDigit('9'), "");
  static_assert(!IsAsciiDigit('/'), "");
  static_assert(!IsAsciiDigit(':'), "");
  static_assert(!IsAsciiDigit('a'), "");
  static_assert(!IsAsciiDigit('\0'), "");
  static_assert(!IsAsciiDigit(static_cast<char>(0x80)), "");
  static_assert(!IsAsciiDigit(static_cast<char>(0xff)), "");

  for (char c = '0'; c <= '9'; ++c) {
    EXPECT_TRUE(IsAsciiDigit(c));
  }
  EXPECT_FALSE(IsAsciiDigit('/'));
  EXPECT_FALSE(IsAsciiDigit(':'));
  EXPECT_FALSE(IsAsciiDigit(static_cast<char>(0xff)));
}

TEST(StringUtilTest, IsHexDigit) {
  static_assert(IsHexDigit('0'), "");
  static_assert(IsHexDigit('9'), "");
  static_assert(IsHexDigit('A'), "");
  static_assert(IsHexDigit('F'), "");
  static_assert(IsHexDigit('a'), "");
  static_assert(IsHexDigit('f'), "");
  static_assert(!IsHexDigit('/'), "");
  static_assert(!IsHexDigit(':'), "");
  static_assert(!IsHexDigit('@'), "");
  static_assert(!IsHexDigit('G'), "");
  static_assert(!IsHexDigit('`'), "");
  static_assert(!IsHexDigit('g'), "");
  static_assert(!IsHexDigit(static_cast<char>(0xff)), "");

  for (char c = '0'; c <= '9'; ++c) {
    EXPECT_TRUE(IsHexDigit(c));
  }
  for (char c = 'A'; c <= 'F'; ++c) {
    EXPECT_TRUE(IsHexDigit(c));
  }
  for (char c = 'a'; c <= 'f'; ++c) {
    EXPECT_TRUE(IsHexDigit(c));
  }
  EXPECT_FALSE(IsHexDigit('G'));
  EXPECT_FALSE(IsHexDigit('g'));
  EXPECT_FALSE(IsHexDigit(static_cast<char>(0xff)));
}

TEST(StringUtilTest, IsAsciiPrintable) {
  static_assert(IsAsciiPrintable(' '), "");
  static_assert(IsAsciiPrintable('A'), "");
  static_assert(IsAsciiPrintable('~'), "");
  static_assert(!IsAsciiPrintable('\0'), "");
  static_assert(!IsAsciiPrintable('\x1f'), "");
  static_assert(!IsAsciiPrintable('\x7f'), "");
  static_assert(!IsAsciiPrintable(static_cast<char>(0x80)), "");
  static_assert(!IsAsciiPrintable(static_cast<char>(0xff)), "");

  for (char c = ' '; c <= '~'; ++c) {
    EXPECT_TRUE(IsAsciiPrintable(c));
  }
  EXPECT_FALSE(IsAsciiPrintable('\t'));
  EXPECT_FALSE(IsAsciiPrintable('\n'));
  EXPECT_FALSE(IsAsciiPrintable('\x7f'));
  EXPECT_FALSE(IsAsciiPrintable(static_cast<char>(0xff)));
}

TEST(StringUtilTest, ToLowerASCII) {
  static_assert(ToLowerASCII('A') == 'a', "");
  static_assert(ToLowerASCII('Z') == 'z', "");
  static_assert(ToLowerASCII('a') == 'a', "");
  static_assert(ToLowerASCII('z') == 'z', "");
  static_assert(ToLowerASCII('0') == '0', "");
  static_assert(ToLowerASCII('@') == '@', "");
  static_assert(ToLowerASCII('[') == '[', "");
  static_assert(
      ToLowerASCII(static_cast<char>(0xc3)) == static_cast<char>(0xc3), "");
  static_assert(
      ToLowerASCII(static_cast<char>(0xff)) == static_cast<char>(0xff), "");

  for (char c = 'A'; c <= 'Z'; ++c) {
    EXPECT_EQ(static_cast<char>(c + ('a' - 'A')), ToLowerASCII(c));
  }
  EXPECT_EQ('5', ToLowerASCII('5'));
  EXPECT_EQ(static_cast<char>(0xff), ToLowerASCII(static_cast<char>(0xff)));
}

}  // namespace
}  // namespace starboard
