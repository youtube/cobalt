// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "base/string_piece.h"
#include "base/utf_offset_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

static const size_t kNpos = std::wstring::npos;

// Given a null-terminated string of wchar_t with each wchar_t representing
// a UTF-16 code unit, returns a string16 made up of wchar_t's in the input.
// Each wchar_t should be <= 0xFFFF and a non-BMP character (> U+FFFF)
// should be represented as a surrogate pair (two UTF-16 units)
// *even* where wchar_t is 32-bit (Linux and Mac).
//
// This is to help write tests for functions with string16 params until
// the C++ 0x UTF-16 literal is well-supported by compilers.
string16 BuildString16(const wchar_t* s) {
#if defined(WCHAR_T_IS_UTF16)
  return string16(s);
#elif defined(WCHAR_T_IS_UTF32)
  string16 u16;
  while (*s != 0) {
    DCHECK(static_cast<unsigned int>(*s) <= 0xFFFFu);
    u16.push_back(*s++);
  }
  return u16;
#endif
}

}  // namespace

TEST(UTFOffsetStringConversionsTest, AdjustOffset) {
  struct UTF8ToWideCase {
    const char* utf8;
    size_t input_offset;
    size_t output_offset;
  } utf8_to_wide_cases[] = {
    {"", 0, kNpos},
    {"\xe4\xbd\xa0\xe5\xa5\xbd", 1, kNpos},
    {"\xe4\xbd\xa0\xe5\xa5\xbd", 3, 1},
    {"\xed\xb0\x80z", 3, 1},
    {"A\xF0\x90\x8C\x80z", 1, 1},
    {"A\xF0\x90\x8C\x80z", 2, kNpos},
#if defined(WCHAR_T_IS_UTF16)
    {"A\xF0\x90\x8C\x80z", 5, 3},
#elif defined(WCHAR_T_IS_UTF32)
    {"A\xF0\x90\x8C\x80z", 5, 2},
#endif
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(utf8_to_wide_cases); ++i) {
    size_t offset = utf8_to_wide_cases[i].input_offset;
    UTF8ToWideAndAdjustOffset(utf8_to_wide_cases[i].utf8, &offset);
    EXPECT_EQ(utf8_to_wide_cases[i].output_offset, offset);
  }

#if defined(WCHAR_T_IS_UTF32)
  struct UTF16ToWideCase {
    const wchar_t* wide;
    size_t input_offset;
    size_t output_offset;
  } utf16_to_wide_cases[] = {
    {L"\xD840\xDC00\x4E00", 0, 0},
    {L"\xD840\xDC00\x4E00", 1, kNpos},
    {L"\xD840\xDC00\x4E00", 2, 1},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(utf16_to_wide_cases); ++i) {
    size_t offset = utf16_to_wide_cases[i].input_offset;
    UTF16ToWideAndAdjustOffset(BuildString16(utf16_to_wide_cases[i].wide),
                               &offset);
    EXPECT_EQ(utf16_to_wide_cases[i].output_offset, offset);
  }
#endif
}

TEST(UTFOffsetStringConversionsTest, LimitOffsets) {
  const size_t kLimit = 10;
  const size_t kItems = 20;
  std::vector<size_t> size_ts;
  for (size_t t = 0; t < kItems; ++t)
    size_ts.push_back(t);
  std::for_each(size_ts.begin(), size_ts.end(),
                LimitOffset<std::wstring>(kLimit));
  size_t unlimited_count = 0;
  for (std::vector<size_t>::iterator ti = size_ts.begin(); ti != size_ts.end();
       ++ti) {
    if (*ti < kLimit && *ti != kNpos)
      ++unlimited_count;
  }
  EXPECT_EQ(10U, unlimited_count);

  // Reverse the values in the vector and try again.
  size_ts.clear();
  for (size_t t = kItems; t > 0; --t)
    size_ts.push_back(t - 1);
  std::for_each(size_ts.begin(), size_ts.end(),
                LimitOffset<std::wstring>(kLimit));
  unlimited_count = 0;
  for (std::vector<size_t>::iterator ti = size_ts.begin(); ti != size_ts.end();
       ++ti) {
    if (*ti < kLimit && *ti != kNpos)
      ++unlimited_count;
  }
  EXPECT_EQ(10U, unlimited_count);
}

TEST(UTFOffsetStringConversionsTest, AdjustOffsets) {
  // Imagine we have strings as shown in the following cases where the
  // X's represent encoded characters.
  // 1: abcXXXdef ==> abcXdef
  std::vector<size_t> offsets;
  for (size_t t = 0; t < 9; ++t)
    offsets.push_back(t);
  AdjustOffset::Adjustments adjustments;
  adjustments.push_back(AdjustOffset::Adjustment(3, 3, 1));
  std::for_each(offsets.begin(), offsets.end(), AdjustOffset(adjustments));
  size_t expected_1[] = {0, 1, 2, 3, kNpos, kNpos, 4, 5, 6};
  EXPECT_EQ(offsets.size(), arraysize(expected_1));
  for (size_t i = 0; i < arraysize(expected_1); ++i)
    EXPECT_EQ(expected_1[i], offsets[i]);

  // 2: XXXaXXXXbcXXXXXXXdefXXX ==> XaXXbcXXXXdefX
  offsets.clear();
  for (size_t t = 0; t < 23; ++t)
    offsets.push_back(t);
  adjustments.clear();
  adjustments.push_back(AdjustOffset::Adjustment(0, 3, 1));
  adjustments.push_back(AdjustOffset::Adjustment(4, 4, 2));
  adjustments.push_back(AdjustOffset::Adjustment(10, 7, 4));
  adjustments.push_back(AdjustOffset::Adjustment(20, 3, 1));
  std::for_each(offsets.begin(), offsets.end(), AdjustOffset(adjustments));
  size_t expected_2[] = {0, kNpos, kNpos, 1, 2, kNpos, kNpos, kNpos, 4, 5, 6,
                         kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 10, 11, 12,
                         13, kNpos, kNpos};
  EXPECT_EQ(offsets.size(), arraysize(expected_2));
  for (size_t i = 0; i < arraysize(expected_2); ++i)
    EXPECT_EQ(expected_2[i], offsets[i]);

  // 3: XXXaXXXXbcdXXXeXX ==> aXXXXbcdXXXe
  offsets.clear();
  for (size_t t = 0; t < 17; ++t)
    offsets.push_back(t);
  adjustments.clear();
  adjustments.push_back(AdjustOffset::Adjustment(0, 3, 0));
  adjustments.push_back(AdjustOffset::Adjustment(4, 4, 4));
  adjustments.push_back(AdjustOffset::Adjustment(11, 3, 3));
  adjustments.push_back(AdjustOffset::Adjustment(15, 2, 0));
  std::for_each(offsets.begin(), offsets.end(), AdjustOffset(adjustments));
  size_t expected_3[] = {kNpos, kNpos, kNpos, 0, 1, kNpos, kNpos, kNpos, 5, 6,
                         7, 8, kNpos, kNpos, 11, kNpos, kNpos};
  EXPECT_EQ(offsets.size(), arraysize(expected_3));
  for (size_t i = 0; i < arraysize(expected_3); ++i)
    EXPECT_EQ(expected_3[i], offsets[i]);
}

}  // namaspace base
