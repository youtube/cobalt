// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/rtl.h"

#include <algorithm>
#include <unicode/usearch.h>

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {
base::i18n::TextDirection GetTextDirection(const char* locale_name) {
  return base::i18n::GetTextDirectionForLocale(locale_name);
}
}

class RTLTest : public PlatformTest {
};

typedef struct {
  const wchar_t* text;
  base::i18n::TextDirection direction;
} TextAndDirection;

TEST_F(RTLTest, GetFirstStrongCharacterDirection) {
  const TextAndDirection test_data[] = {
    // Test pure LTR string.
    { L"foo bar", base::i18n::LEFT_TO_RIGHT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type L.
    { L"foo \x05d0 bar", base::i18n::LEFT_TO_RIGHT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type R.
    { L"\x05d0 foo bar", base::i18n::RIGHT_TO_LEFT },
    // Test bidi string which starts with a character with weak directionality
    // and in which the first character with strong directionality is a
    // character with type L.
    { L"!foo \x05d0 bar", base::i18n::LEFT_TO_RIGHT },
    // Test bidi string which starts with a character with weak directionality
    // and in which the first character with strong directionality is a
    // character with type R.
    { L",\x05d0 foo bar", base::i18n::RIGHT_TO_LEFT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type LRE.
    { L"\x202a \x05d0 foo  bar", base::i18n::LEFT_TO_RIGHT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type LRO.
    { L"\x202d \x05d0 foo  bar", base::i18n::LEFT_TO_RIGHT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type RLE.
    { L"\x202b foo \x05d0 bar", base::i18n::RIGHT_TO_LEFT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type RLO.
    { L"\x202e foo \x05d0 bar", base::i18n::RIGHT_TO_LEFT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type AL.
    { L"\x0622 foo \x05d0 bar", base::i18n::RIGHT_TO_LEFT },
    // Test a string without strong directionality characters.
    { L",!.{}", base::i18n::LEFT_TO_RIGHT },
    // Test empty string.
    { L"", base::i18n::LEFT_TO_RIGHT },
    // Test characters in non-BMP (e.g. Phoenician letters. Please refer to
    // http://demo.icu-project.org/icu-bin/ubrowse?scr=151&b=10910 for more
    // information).
    {
#if defined(WCHAR_T_IS_UTF32)
      L" ! \x10910" L"abc 123",
#elif defined(WCHAR_T_IS_UTF16)
      L" ! \xd802\xdd10" L"abc 123",
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
      base::i18n::RIGHT_TO_LEFT },
    {
#if defined(WCHAR_T_IS_UTF32)
      L" ! \x10401" L"abc 123",
#elif defined(WCHAR_T_IS_UTF16)
      L" ! \xd801\xdc01" L"abc 123",
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
      base::i18n::LEFT_TO_RIGHT },
   };

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    EXPECT_EQ(test_data[i].direction,
              base::i18n::GetFirstStrongCharacterDirection(
                  WideToUTF16(test_data[i].text)));
  }
}

TEST_F(RTLTest, WrapPathWithLTRFormatting) {
  const wchar_t* test_data[] = {
    // Test common path, such as "c:\foo\bar".
    L"c:/foo/bar",
    // Test path with file name, such as "c:\foo\bar\test.jpg".
    L"c:/foo/bar/test.jpg",
    // Test path ending with punctuation, such as "c:\(foo)\bar.".
    L"c:/(foo)/bar.",
    // Test path ending with separator, such as "c:\foo\bar\".
    L"c:/foo/bar/",
    // Test path with RTL character.
    L"c:/\x05d0",
    // Test path with 2 level RTL directory names.
    L"c:/\x05d0/\x0622",
    // Test path with mixed RTL/LTR directory names and ending with punctuation.
    L"c:/\x05d0/\x0622/(foo)/b.a.r.",
    // Test path without driver name, such as "/foo/bar/test/jpg".
    L"/foo/bar/test.jpg",
    // Test path start with current directory, such as "./foo".
    L"./foo",
    // Test path start with parent directory, such as "../foo/bar.jpg".
    L"../foo/bar.jpg",
    // Test absolute path, such as "//foo/bar.jpg".
    L"//foo/bar.jpg",
    // Test path with mixed RTL/LTR directory names.
    L"c:/foo/\x05d0/\x0622/\x05d1.jpg",
    // Test empty path.
    L""
  };
  for (size_t i = 0; i < arraysize(test_data); ++i) {
    FilePath path;
#if defined(OS_WIN)
    std::wstring win_path(test_data[i]);
    std::replace(win_path.begin(), win_path.end(), '/', '\\');
    path = FilePath(win_path);
    std::wstring wrapped_expected =
        std::wstring(L"\x202a") + win_path + L"\x202c";
#else
    path = FilePath(base::SysWideToNativeMB(test_data[i]));
    std::wstring wrapped_expected =
        std::wstring(L"\x202a") + test_data[i] + L"\x202c";
#endif
    string16 localized_file_path_string;
    base::i18n::WrapPathWithLTRFormatting(path, &localized_file_path_string);

    std::wstring wrapped_actual = UTF16ToWide(localized_file_path_string);
    EXPECT_EQ(wrapped_expected, wrapped_actual);
  }
}

typedef struct {
  const wchar_t* raw_filename;
  const wchar_t* display_string;
} StringAndLTRString;

TEST_F(RTLTest, GetDisplayStringInLTRDirectionality) {
  const StringAndLTRString test_data[] = {
    { L"test", L"\x202atest\x202c" },
    { L"test.html", L"\x202atest.html\x202c" },
    { L"\x05d0\x05d1\x05d2", L"\x202a\x05d0\x05d1\x05d2\x202c" },
    { L"\x05d0\x05d1\x05d2.txt", L"\x202a\x05d0\x05d1\x05d2.txt\x202c" },
    { L"\x05d0"L"abc", L"\x202a\x05d0"L"abc\x202c" },
    { L"\x05d0"L"abc.txt", L"\x202a\x05d0"L"abc.txt\x202c" },
    { L"abc\x05d0\x05d1", L"\x202a"L"abc\x05d0\x05d1\x202c" },
    { L"abc\x05d0\x05d1.jpg", L"\x202a"L"abc\x05d0\x05d1.jpg\x202c" },
  };
  for (size_t i = 0; i < arraysize(test_data); ++i) {
    string16 input = WideToUTF16(test_data[i].raw_filename);
    string16 expected = base::i18n::GetDisplayStringInLTRDirectionality(input);
    if (base::i18n::IsRTL())
      EXPECT_EQ(expected, WideToUTF16(test_data[i].display_string));
    else
      EXPECT_EQ(expected, input);
  }
}

TEST_F(RTLTest, GetTextDirection) {
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, GetTextDirection("ar"));
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, GetTextDirection("ar_EG"));
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, GetTextDirection("he"));
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, GetTextDirection("he_IL"));
  // iw is an obsolete code for Hebrew.
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, GetTextDirection("iw"));
  // Although we're not yet localized to Farsi and Urdu, we
  // do have the text layout direction information for them.
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, GetTextDirection("fa"));
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, GetTextDirection("ur"));
#if 0
  // Enable these when we include the minimal locale data for Azerbaijani
  // written in Arabic and Dhivehi. At the moment, our copy of
  // ICU data does not have entries for them.
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, GetTextDirection("az_Arab"));
  // Dhivehi that uses Thaana script.
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, GetTextDirection("dv"));
#endif
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, GetTextDirection("en"));
  // Chinese in China with '-'.
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, GetTextDirection("zh-CN"));
  // Filipino : 3-letter code
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, GetTextDirection("fil"));
  // Russian
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, GetTextDirection("ru"));
  // Japanese that uses multiple scripts
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, GetTextDirection("ja"));
}

TEST_F(RTLTest, UnadjustStringForLocaleDirection) {
  // These test strings are borrowed from WrapPathWithLTRFormatting
  const wchar_t* test_data[] = {
    L"foo bar",
    L"foo \x05d0 bar",
    L"\x05d0 foo bar",
    L"!foo \x05d0 bar",
    L",\x05d0 foo bar",
    L"\x202a \x05d0 foo  bar",
    L"\x202d \x05d0 foo  bar",
    L"\x202b foo \x05d0 bar",
    L"\x202e foo \x05d0 bar",
    L"\x0622 foo \x05d0 bar",
  };

  const char* default_locale = uloc_getDefault();

  for (size_t i = 0; i < 2; i++) {
    // Try in LTR and RTL.
    std::string locale(i == 0 ? "en_US" : "he_IL");
    base::i18n::SetICUDefaultLocale(locale);

    for (size_t i = 0; i < arraysize(test_data); ++i) {
      string16 test_case = WideToUTF16(test_data[i]);
      string16 adjusted_string = test_case;

      if (!base::i18n::AdjustStringForLocaleDirection(&adjusted_string))
        continue;

      EXPECT_NE(test_case, adjusted_string);
      EXPECT_TRUE(base::i18n::UnadjustStringForLocaleDirection(
                      &adjusted_string));
      EXPECT_EQ(test_case, adjusted_string) << " for test case " << test_case
                                            << " and locale " << locale;
    }
  }

  base::i18n::SetICUDefaultLocale(default_locale);
}
