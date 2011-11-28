// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/i18n/rtl.h"
#include "base/i18n/string_search.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "unicode/usearch.h"

namespace base {
namespace i18n {

// Note on setting default locale for testing: The current default locale on
// the Mac trybot is en_US_POSIX, with which primary-level collation strength
// string search is case-sensitive, when normally it should be
// case-insensitive. In other locales (including en_US which English speakers
// in the U.S. use), this search would be case-insensitive as expected.

TEST(StringSearchTest, ASCII) {
  std::string default_locale(uloc_getDefault());
  bool locale_is_posix = (default_locale == "en_US_POSIX");
  if (locale_is_posix)
    SetICUDefaultLocale("en_US");

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      ASCIIToUTF16("hello"), ASCIIToUTF16("hello world")));

  EXPECT_FALSE(StringSearchIgnoringCaseAndAccents(
      ASCIIToUTF16("h    e l l o"), ASCIIToUTF16("h   e l l o")));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      ASCIIToUTF16("aabaaa"), ASCIIToUTF16("aaabaabaaa")));

  EXPECT_FALSE(StringSearchIgnoringCaseAndAccents(
      ASCIIToUTF16("searching within empty string"), string16()));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      string16(), ASCIIToUTF16("searching for empty string")));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      ASCIIToUTF16("case insensitivity"), ASCIIToUTF16("CaSe InSeNsItIvItY")));

  if (locale_is_posix)
    SetICUDefaultLocale(default_locale.data());
}

TEST(StringSearchTest, UnicodeLocaleIndependent) {
  // Base characters
  const string16 e_base = WideToUTF16(L"e");
  const string16 E_base = WideToUTF16(L"E");
  const string16 a_base = WideToUTF16(L"a");

  // Composed characters
  const string16 e_with_accute_accent = WideToUTF16(L"\u00e9");
  const string16 E_with_accute_accent = WideToUTF16(L"\u00c9");
  const string16 e_with_grave_accent = WideToUTF16(L"\u00e8");
  const string16 E_with_grave_accent = WideToUTF16(L"\u00c8");
  const string16 a_with_accute_accent = WideToUTF16(L"\u00e1");

  // Decomposed characters
  const string16 e_with_accute_combining_mark = WideToUTF16(L"e\u0301");
  const string16 E_with_accute_combining_mark = WideToUTF16(L"E\u0301");
  const string16 e_with_grave_combining_mark = WideToUTF16(L"e\u0300");
  const string16 E_with_grave_combining_mark = WideToUTF16(L"E\u0300");
  const string16 a_with_accute_combining_mark = WideToUTF16(L"a\u0301");

  std::string default_locale(uloc_getDefault());
  bool locale_is_posix = (default_locale == "en_US_POSIX");
  if (locale_is_posix)
    SetICUDefaultLocale("en_US");

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      e_base, e_with_accute_accent));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      e_with_accute_accent, e_base));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      e_base, e_with_accute_combining_mark));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      e_with_accute_combining_mark, e_base));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      e_with_accute_combining_mark, e_with_accute_accent));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      e_with_accute_accent, e_with_accute_combining_mark));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      e_with_accute_combining_mark, e_with_grave_combining_mark));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      e_with_grave_combining_mark, e_with_accute_combining_mark));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      e_with_accute_combining_mark, e_with_grave_accent));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      e_with_grave_accent, e_with_accute_combining_mark));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      E_with_accute_accent, e_with_accute_accent));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      E_with_grave_accent, e_with_accute_accent));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      E_with_accute_combining_mark, e_with_grave_accent));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      E_with_grave_combining_mark, e_with_accute_accent));

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      E_base, e_with_grave_accent));

  EXPECT_FALSE(StringSearchIgnoringCaseAndAccents(
      a_with_accute_accent, e_with_accute_accent));

  EXPECT_FALSE(StringSearchIgnoringCaseAndAccents(
      a_with_accute_combining_mark, e_with_accute_combining_mark));

  if (locale_is_posix)
    SetICUDefaultLocale(default_locale.data());
}

TEST(StringSearchTest, UnicodeLocaleDependent) {
  // Base characters
  const string16 a_base = WideToUTF16(L"a");

  // Composed characters
  const string16 a_with_ring = WideToUTF16(L"\u00e5");

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(
      a_base, a_with_ring));

  const char* default_locale = uloc_getDefault();
  SetICUDefaultLocale("da");

  EXPECT_FALSE(StringSearchIgnoringCaseAndAccents(
      a_base, a_with_ring));

  SetICUDefaultLocale(default_locale);
}

}  // namespace i18n
}  // namespace base

