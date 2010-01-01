// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/string_piece.h"
#include "base/utf_offset_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

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
    {"", 0, std::wstring::npos},
    {"\xe4\xbd\xa0\xe5\xa5\xbd", 1, std::wstring::npos},
    {"\xe4\xbd\xa0\xe5\xa5\xbd", 3, 1},
    {"\xed\xb0\x80z", 3, 1},
    {"A\xF0\x90\x8C\x80z", 1, 1},
    {"A\xF0\x90\x8C\x80z", 2, std::wstring::npos},
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
    {L"\xD840\xDC00\x4E00", 1, std::wstring::npos},
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

}  // namaspace base
