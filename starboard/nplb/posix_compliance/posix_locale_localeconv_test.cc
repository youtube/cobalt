// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License-for-dev at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <limits.h>
#include <locale.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "starboard/nplb/posix_compliance/posix_locale_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
struct LocaleconvTestData {
  const char* locale_name;

  // Members from struct lconv
  const char* decimal_point;
  const char* thousands_sep;
  const char* grouping;
  const char* int_curr_symbol;
  const char* currency_symbol;
  const char* mon_decimal_point;
  const char* mon_thousands_sep;
  const char* mon_grouping;
  const char* positive_sign;
  const char* negative_sign;
  char int_frac_digits;
  char frac_digits;
  char p_cs_precedes;
  char p_sep_by_space;
  char n_cs_precedes;
  char n_sep_by_space;
  char p_sign_posn;
  char n_sign_posn;
  char int_p_cs_precedes;
  char int_p_sep_by_space;
  char int_n_cs_precedes;
  char int_n_sep_by_space;
  char int_p_sign_posn;
  char int_n_sign_posn;

  friend void PrintTo(const LocaleconvTestData& data, ::std::ostream* os);
};

void PrintTo(const LocaleconvTestData& data, ::std::ostream* os) {
  *os << "{ locale_name: " << (data.locale_name ? data.locale_name : "null")
      << " ... }";
}

struct LocaleconvTestDataNameGenerator {
  std::string operator()(
      const ::testing::TestParamInfo<LocaleconvTestData>& info) const {
    std::string name = info.param.locale_name;
    std::replace(name.begin(), name.end(), '@', '_');
    return name;
  }
};

#include "starboard/nplb/posix_compliance/posix_locale_localeconv_test_data.cc.inc"

void CheckLconv(const lconv* lc, const LocaleconvTestData& data) {
  EXPECT_STREQ(lc->decimal_point, data.decimal_point);
  EXPECT_STREQ(lc->thousands_sep, data.thousands_sep);
  EXPECT_STREQ(lc->grouping, data.grouping);
  EXPECT_STREQ(lc->int_curr_symbol, data.int_curr_symbol);
  EXPECT_STREQ(lc->currency_symbol, data.currency_symbol);
  EXPECT_STREQ(lc->mon_decimal_point, data.mon_decimal_point);
  EXPECT_STREQ(lc->mon_thousands_sep, data.mon_thousands_sep);
  EXPECT_STREQ(lc->mon_grouping, data.mon_grouping);
  EXPECT_STREQ(lc->positive_sign, data.positive_sign);
  EXPECT_STREQ(lc->negative_sign, data.negative_sign);
  EXPECT_EQ(lc->int_frac_digits, data.int_frac_digits);
  EXPECT_EQ(lc->frac_digits, data.frac_digits);
  EXPECT_EQ(lc->p_cs_precedes, data.p_cs_precedes);
  EXPECT_EQ(lc->p_sep_by_space, data.p_sep_by_space);
  EXPECT_EQ(lc->n_cs_precedes, data.n_cs_precedes);
  EXPECT_EQ(lc->n_sep_by_space, data.n_sep_by_space);
  EXPECT_EQ(lc->p_sign_posn, data.p_sign_posn);
  EXPECT_EQ(lc->n_sign_posn, data.n_sign_posn);
  EXPECT_EQ(lc->int_p_cs_precedes, data.int_p_cs_precedes);
  EXPECT_EQ(lc->int_p_sep_by_space, data.int_p_sep_by_space);
  EXPECT_EQ(lc->int_n_cs_precedes, data.int_n_cs_precedes);
  EXPECT_EQ(lc->int_n_sep_by_space, data.int_n_sep_by_space);
  EXPECT_EQ(lc->int_p_sign_posn, data.int_p_sign_posn);
  EXPECT_EQ(lc->int_n_sign_posn, data.int_n_sign_posn);
}

class LocaleconvTest : public ::testing::TestWithParam<LocaleconvTestData> {
 protected:
  void SetUp() override {
    const LocaleconvTestData& data = GetParam();
    // Save the current locale.
    char* old_locale_cstr = setlocale(LC_ALL, nullptr);
    ASSERT_NE(old_locale_cstr, nullptr);
    old_locale_ = old_locale_cstr;

    // Set the locale for the test.
    if (!setlocale(LC_ALL, data.locale_name)) {
      GTEST_SKIP() << "Locale " << data.locale_name << " not supported.";
    }
  }

  void TearDown() override {
    // Restore the original locale.
    setlocale(LC_ALL, old_locale_.c_str());
  }

 private:
  std::string old_locale_;
};

TEST_P(LocaleconvTest, AllItems) {
  const LocaleconvTestData& data = GetParam();
  const lconv* lc = localeconv();
  ASSERT_NE(lc, nullptr);
  CheckLconv(lc, data);
}

INSTANTIATE_TEST_SUITE_P(Posix,
                         LocaleconvTest,
                         ::testing::ValuesIn(kLocaleconvTestData),
                         LocaleconvTestDataNameGenerator());

class UselocaleLocaleconvTest
    : public ::testing::TestWithParam<LocaleconvTestData> {};

TEST_P(UselocaleLocaleconvTest, AllItems) {
  const LocaleconvTestData& data = GetParam();

  char* old_global_locale_cstr = setlocale(LC_ALL, nullptr);
  ASSERT_NE(old_global_locale_cstr, nullptr);
  std::string old_global_locale = old_global_locale_cstr;

  // Explicitly set the global locale to "C" to ensure that the thread-local
  // locale set by uselocale() takes precedence.
  ASSERT_NE(setlocale(LC_ALL, "C"), nullptr);

  locale_t new_loc = newlocale(LC_ALL_MASK, data.locale_name, (locale_t)0);
  if (!new_loc) {
    setlocale(LC_ALL, old_global_locale.c_str());
    GTEST_SKIP() << "Locale " << data.locale_name << " not supported.";
  }

  locale_t old_loc = uselocale(new_loc);
  ASSERT_NE(old_loc, (locale_t)0);

  const lconv* lc = localeconv();
  ASSERT_NE(lc, nullptr);
  CheckLconv(lc, data);

  uselocale(old_loc);
  freelocale(new_loc);
  setlocale(LC_ALL, old_global_locale.c_str());
}

INSTANTIATE_TEST_SUITE_P(Posix,
                         UselocaleLocaleconvTest,
                         ::testing::ValuesIn(kLocaleconvTestData),
                         LocaleconvTestDataNameGenerator());

// --- Dumper Test ---

const char* kLocaleFormats[] = {
    "C",     "POSIX", "af_ZA", "sq_AL",       "am_ET", "ar_SA",  "hy_AM",
    "as_IN", "az_AZ", "bn_BD", "eu_ES",       "be_BY", "bs_BA",  "bg_BG",
    "ca_ES", "zh_CN", "zh_HK", "zh_TW",       "hr_HR", "cs_CZ",  "da_DK",
    "nl_NL", "en_US", "en_IN", "en_GB",       "et_EE", "fil_PH", "fi_FI",
    "fr_FR", "fr_CA", "gl_ES", "ka_GE",       "de_DE", "el_GR",  "gu_IN",
    "he_IL", "hi_IN", "hu_HU", "is_IS",       "id_ID", "it_IT",  "ja_JP",
    "kn_IN", "kk_KZ", "km_KH", "ko_KR",       "ky_KG", "lo_LA",  "lv_LV",
    "lt_LT", "mk_MK", "ms_MY", "ml_IN",       "mr_IN", "mn_MN",  "my_MM",
    "ne_NP", "or_IN", "fa_IR", "pl_PL",       "pt_BR", "pt_PT",  "pa_IN",
    "ro_RO", "ru_RU", "sr_RS", "sr_RS@latin", "si_LK", "sk_SK",  "sl_SI",
    "es_MX", "es_ES", "es_US", "sw_KE",       "sv_SE", "ta_IN",  "te_IN",
    "th_TH", "tr_TR", "uk_UA", "ur_PK",       "uz_UZ", "vi_VN",  "zu_ZA"};

std::string EscapeString(const char* str) {
  if (!str) {
    return "";
  }
  std::string result;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(str); *p;
       ++p) {
    if (*p >= 32 && *p <= 126 && *p != '"' && *p != '\\') {
      result += *p;
    } else {
      char hex[5];
      snprintf(hex, sizeof(hex), "\\x%02x", *p);
      result += hex;
    }
  }
  return result;
}

void DumpString(FILE* file, const char* name, const char* value) {
  if (value) {
    fprintf(file, "      .%s = \"%s\",\n", name, EscapeString(value).c_str());
  }
}

void DumpChar(FILE* file, const char* name, char value) {
  if (value != CHAR_MAX) {
    fprintf(file, "      .%s = %d,\n", name, value);
  } else {
    fprintf(file, "      .%s = CHAR_MAX,\n", name);
  }
}

TEST(PosixLocaleLocaleconvDumperTest, DISABLED_DumpAllLocaleDataForConstexpr) {
  FILE* file = fopen(
      "starboard/nplb/posix_compliance/"
      "posix_locale_localeconv_test_data.cc.inc",
      "w");
  ASSERT_NE(file, nullptr);

  fprintf(file, "constexpr LocaleconvTestData kLocaleconvTestData[] = {\n");
  for (const char* locale_name : kLocaleFormats) {
    if (!setlocale(LC_ALL, locale_name)) {
      continue;
    }
    const lconv* lc = localeconv();
    fprintf(file, "    {\n");
    fprintf(file, "      .locale_name = \"%s\",\n", locale_name);
    DumpString(file, "decimal_point", lc->decimal_point);
    DumpString(file, "thousands_sep", lc->thousands_sep);
    DumpString(file, "grouping", lc->grouping);
    DumpString(file, "int_curr_symbol", lc->int_curr_symbol);
    DumpString(file, "currency_symbol", lc->currency_symbol);
    DumpString(file, "mon_decimal_point", lc->mon_decimal_point);
    DumpString(file, "mon_thousands_sep", lc->mon_thousands_sep);
    DumpString(file, "mon_grouping", lc->mon_grouping);
    DumpString(file, "positive_sign", lc->positive_sign);
    DumpString(file, "negative_sign", lc->negative_sign);
    DumpChar(file, "int_frac_digits", lc->int_frac_digits);
    DumpChar(file, "frac_digits", lc->frac_digits);
    DumpChar(file, "p_cs_precedes", lc->p_cs_precedes);
    DumpChar(file, "p_sep_by_space", lc->p_sep_by_space);
    DumpChar(file, "n_cs_precedes", lc->n_cs_precedes);
    DumpChar(file, "n_sep_by_space", lc->n_sep_by_space);
    DumpChar(file, "p_sign_posn", lc->p_sign_posn);
    DumpChar(file, "n_sign_posn", lc->n_sign_posn);
    DumpChar(file, "int_p_cs_precedes", lc->int_p_cs_precedes);
    DumpChar(file, "int_p_sep_by_space", lc->int_p_sep_by_space);
    DumpChar(file, "int_n_cs_precedes", lc->int_n_cs_precedes);
    DumpChar(file, "int_n_sep_by_space", lc->int_n_sep_by_space);
    DumpChar(file, "int_p_sign_posn", lc->int_p_sign_posn);
    DumpChar(file, "int_n_sign_posn", lc->int_n_sign_posn);
    fprintf(file, "    },\n");
  }
  fprintf(file, "};\n");
  fclose(file);
}

}  // namespace
