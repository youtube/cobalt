// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/i18n/number_formatting.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "unicode/locid.h"

namespace {

void SetICUDefaultLocale(const std::string& locale_string) {
  icu::Locale locale(locale_string.c_str());
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(locale, error_code);
  EXPECT_TRUE(U_SUCCESS(error_code));
}

}  // namespace

TEST(NumberFormattingTest, FormatNumber) {
  static const struct {
    int64 number;
    const char* expected_english;
    const char* expected_german;
  } cases[] = {
    {0, "0", "0"},
    {1024, "1,024", "1.024"},
    {std::numeric_limits<int64>::max(),
        "9,223,372,036,854,775,807", "9.223.372.036.854.775.807"},
    {std::numeric_limits<int64>::min(),
        "-9,223,372,036,854,775,808", "-9.223.372.036.854.775.808"},
    {-42, "-42", "-42"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SetICUDefaultLocale("en");
    base::testing::ResetFormatters();
    EXPECT_EQ(cases[i].expected_english,
              UTF16ToUTF8(base::FormatNumber(cases[i].number)));
    SetICUDefaultLocale("de");
    base::testing::ResetFormatters();
    EXPECT_EQ(cases[i].expected_german,
              UTF16ToUTF8(base::FormatNumber(cases[i].number)));
  }
}

TEST(NumberFormattingTest, FormatDouble) {
  static const struct {
    double number;
    int frac_digits;
    const char* expected_english;
    const char* expected_german;
  } cases[] = {
    {0.0, 0, "0", "0"},
#if !defined(OS_ANDROID)
    // Bionic can't printf negative zero correctly.
    {-0.0, 4, "-0.0000", "-0,0000"},
#endif
    {1024.2, 0, "1,024", "1.024"},
    {-1024.223, 2, "-1,024.22", "-1.024,22"},
    {std::numeric_limits<double>::max(), 6,
        "179,769,313,486,232,000,000,000,000,000,000,000,000,000,000,000,000,"
        "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
        "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
        "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
        "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
        "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
        "000.000000",
        "179.769.313.486.232.000.000.000.000.000.000.000.000.000.000.000.000."
        "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
        "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
        "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
        "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
        "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
        "000,000000"},
    {std::numeric_limits<double>::min(), 2, "0.00", "0,00"},
    {-42.7, 3, "-42.700", "-42,700"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
  SetICUDefaultLocale("en");
  base::testing::ResetFormatters();
  EXPECT_EQ(cases[i].expected_english,
            UTF16ToUTF8(base::FormatDouble(cases[i].number,
                                           cases[i].frac_digits)));
  SetICUDefaultLocale("de");
  base::testing::ResetFormatters();
  EXPECT_EQ(cases[i].expected_german,
            UTF16ToUTF8(base::FormatDouble(cases[i].number,
                                           cases[i].frac_digits)));
  }
}
