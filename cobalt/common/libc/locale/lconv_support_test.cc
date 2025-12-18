// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License-for-dev at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/common/libc/locale/lconv_support.h"

#include <climits>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

// The test values for these lconvs were chosen based off of the glibc
// specification of these values.
class LconvSupportTest : public ::testing::Test {
 protected:
  void SetUp() override { lconv_.ResetToC(); }

  LconvImpl lconv_;
};

TEST_F(LconvSupportTest, InitialStateIsC) {
  EXPECT_STREQ(lconv_.result.decimal_point, ".");
  EXPECT_STREQ(lconv_.result.thousands_sep, "");
  EXPECT_STREQ(lconv_.result.grouping, "");
  EXPECT_STREQ(lconv_.result.int_curr_symbol, "");
  EXPECT_EQ(lconv_.result.frac_digits, CHAR_MAX);
  EXPECT_EQ(lconv_.current_numeric_locale, "C");
  EXPECT_EQ(lconv_.current_monetary_locale, "C");
}

TEST_F(LconvSupportTest, UpdateNumericLconvHandlesDeDe) {
  UpdateNumericLconv("de_DE", &lconv_);

  EXPECT_STREQ(lconv_.result.decimal_point, ",");
  EXPECT_STREQ(lconv_.result.thousands_sep, ".");

  ASSERT_TRUE(lconv_.result.grouping != nullptr);
  EXPECT_EQ(static_cast<int>(lconv_.result.grouping[0]), 3);
}

TEST_F(LconvSupportTest, UpdateNumericLconvCachesLocale) {
  UpdateNumericLconv("en_US", &lconv_);
  EXPECT_EQ(lconv_.current_numeric_locale, "en_US");
  const char* first_ptr = lconv_.result.decimal_point;

  UpdateNumericLconv("en_US", &lconv_);
  EXPECT_EQ(lconv_.result.decimal_point, first_ptr);
}

TEST_F(LconvSupportTest, ExhaustiveLconvFieldCheck) {
  UpdateNumericLconv("en_US", &lconv_);
  UpdateMonetaryLconv("en_US", &lconv_);

  EXPECT_STREQ(lconv_.result.decimal_point, ".");
  EXPECT_STREQ(lconv_.result.thousands_sep, ",");
  ASSERT_TRUE(lconv_.result.grouping != nullptr);
  EXPECT_EQ(static_cast<int>(lconv_.result.grouping[0]), 3);

  EXPECT_STREQ(lconv_.result.mon_decimal_point, ".");
  EXPECT_STREQ(lconv_.result.mon_thousands_sep, ",");
  ASSERT_TRUE(lconv_.result.mon_grouping != nullptr);
  EXPECT_EQ(static_cast<int>(lconv_.result.mon_grouping[0]), 3);
  EXPECT_STREQ(lconv_.result.currency_symbol, "$");
  EXPECT_STREQ(lconv_.result.int_curr_symbol, "USD ");

  EXPECT_STREQ(lconv_.result.positive_sign, "");
  EXPECT_STREQ(lconv_.result.negative_sign, "-");

  EXPECT_EQ(lconv_.result.frac_digits, 2);
  EXPECT_EQ(lconv_.result.int_frac_digits, 2);

  EXPECT_EQ(lconv_.result.p_cs_precedes, 1);
  EXPECT_EQ(lconv_.result.p_sep_by_space, 0);
  EXPECT_EQ(lconv_.result.p_sign_posn, 1);

  EXPECT_EQ(lconv_.result.n_cs_precedes, 1);
  EXPECT_EQ(lconv_.result.n_sep_by_space, 0);
  EXPECT_EQ(lconv_.result.n_sign_posn, 1);

  // Unfortunately, there is no consistent 1 : 1 translation for the
  // international values from ICU to POSIX, due to design differences in how
  // ICU holds these fields versus POSIX. The best we can do is ensure that
  // these values are actually changed.
  EXPECT_NE(lconv_.result.int_p_cs_precedes, CHAR_MAX);
  EXPECT_NE(lconv_.result.int_p_sep_by_space, CHAR_MAX);

  EXPECT_NE(lconv_.result.int_n_cs_precedes, CHAR_MAX);
  EXPECT_NE(lconv_.result.int_n_sep_by_space, CHAR_MAX);

  EXPECT_NE(lconv_.result.int_p_sign_posn, CHAR_MAX);
  EXPECT_NE(lconv_.result.int_n_sign_posn, CHAR_MAX);
}

TEST_F(LconvSupportTest, ExhaustiveSrRsLatinFieldCheck) {
  UpdateNumericLconv("sr_RS@latin", &lconv_);
  UpdateMonetaryLconv("sr_RS@latin", &lconv_);

  EXPECT_STREQ(lconv_.result.decimal_point, ",");
  EXPECT_STREQ(lconv_.result.thousands_sep, ".");
  ASSERT_TRUE(lconv_.result.grouping != nullptr);
  EXPECT_EQ(static_cast<int>(lconv_.result.grouping[0]), 3);

  EXPECT_STREQ(lconv_.result.mon_decimal_point, ",");
  EXPECT_STREQ(lconv_.result.mon_thousands_sep, ".");
  ASSERT_TRUE(lconv_.result.mon_grouping != nullptr);
  EXPECT_EQ(static_cast<int>(lconv_.result.mon_grouping[0]), 3);

  // TODO: b/467701409 - Properly address the incorrect currency symbol.
  // Currently, our ICU implementaiton will return "RSD" instead of "din".
  // EXPECT_STREQ(lconv_.result.currency_symbol, "din");
  EXPECT_STREQ(lconv_.result.int_curr_symbol, "RSD ");

  EXPECT_STREQ(lconv_.result.positive_sign, "");
  EXPECT_STREQ(lconv_.result.negative_sign, "-");

  EXPECT_EQ(lconv_.result.frac_digits, 0);
  EXPECT_EQ(lconv_.result.int_frac_digits, 0);

  EXPECT_EQ(lconv_.result.p_cs_precedes, 0);
  EXPECT_EQ(lconv_.result.p_sep_by_space, 1);
  EXPECT_EQ(lconv_.result.p_sign_posn, 1);

  EXPECT_EQ(lconv_.result.n_cs_precedes, 0);
  EXPECT_EQ(lconv_.result.n_sep_by_space, 1);
  EXPECT_EQ(lconv_.result.n_sign_posn, 1);

  EXPECT_NE(lconv_.result.int_p_cs_precedes, CHAR_MAX);
  EXPECT_NE(lconv_.result.int_p_sep_by_space, CHAR_MAX);

  EXPECT_NE(lconv_.result.int_n_cs_precedes, CHAR_MAX);
  EXPECT_NE(lconv_.result.int_n_sep_by_space, CHAR_MAX);

  EXPECT_NE(lconv_.result.int_p_sign_posn, CHAR_MAX);
  EXPECT_NE(lconv_.result.int_n_sign_posn, CHAR_MAX);
}

TEST_F(LconvSupportTest, UpdateMonetaryLconvHandlesJapaneseYen) {
  UpdateMonetaryLconv("ja_JP", &lconv_);

  EXPECT_EQ(lconv_.result.frac_digits, 0);
  EXPECT_EQ(lconv_.result.int_frac_digits, 0);

  EXPECT_STREQ(lconv_.result.int_curr_symbol, "JPY ");

  EXPECT_STREQ(lconv_.result.currency_symbol, "￥");
}

TEST_F(LconvSupportTest, UpdateMonetaryLconvHandlesIndianGrouping) {
  UpdateMonetaryLconv("hi_IN", &lconv_);

  EXPECT_STREQ(lconv_.result.mon_decimal_point, ".");
  EXPECT_STREQ(lconv_.result.mon_thousands_sep, ",");

  ASSERT_TRUE(lconv_.result.mon_grouping != nullptr);
  EXPECT_EQ(static_cast<int>(lconv_.result.mon_grouping[0]), 3);
  EXPECT_EQ(static_cast<int>(lconv_.result.mon_grouping[1]), 2);
  EXPECT_EQ(static_cast<int>(lconv_.result.mon_grouping[2]), 0);
}

TEST_F(LconvSupportTest, UpdateMonetaryLconvHandlesCzechTrailingSymbol) {
  UpdateMonetaryLconv("cs_CZ", &lconv_);

  EXPECT_STREQ(lconv_.result.mon_decimal_point, ",");
  EXPECT_STREQ(lconv_.result.currency_symbol, "Kč");

  EXPECT_EQ(lconv_.result.p_cs_precedes, 0);
  EXPECT_EQ(lconv_.result.p_sep_by_space, 1);
}

TEST_F(LconvSupportTest, UpdateMonetaryLconvHandlesSpaceBetweenSignAndSym) {
  // nl_NL (Dutch) often formats negative numbers as: "€ -100,00".

  UpdateMonetaryLconv("nl_NL", &lconv_);
  EXPECT_EQ(lconv_.result.n_sep_by_space, 2);
}

TEST_F(LconvSupportTest, UpdateMonetaryLconvHandlesEuroLayout) {
  UpdateMonetaryLconv("fr_FR", &lconv_);

  EXPECT_STREQ(lconv_.result.mon_decimal_point, ",");
  EXPECT_STREQ(lconv_.result.currency_symbol, "€");
  EXPECT_STREQ(lconv_.result.int_curr_symbol, "EUR ");

  EXPECT_EQ(lconv_.result.p_cs_precedes, 0);
  EXPECT_EQ(lconv_.result.n_cs_precedes, 0);
  EXPECT_EQ(lconv_.result.p_sep_by_space, 1);
}

}  // namespace
}  // namespace cobalt
