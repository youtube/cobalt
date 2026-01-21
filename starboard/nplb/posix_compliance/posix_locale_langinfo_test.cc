// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <langinfo.h>
#include <locale.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include "starboard/nplb/posix_compliance/posix_locale_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

// A special marker to indicate that we should only check for a non-empty
// string, without asserting its specific content.
constexpr char kNonEmptyStringMarker = '\0';
constexpr const char* kNonEmptyString = &kNonEmptyStringMarker;

struct LanginfoTestData {
  const char* locale_name;

  // LC_NUMERIC
  const char* radixchar = nullptr;
  const char* thousands_sep = nullptr;

  // LC_CTYPE
  const char* codeset = nullptr;

  // LC_TIME
  const char* d_t_fmt = nullptr;
  const char* d_fmt = nullptr;
  const char* t_fmt = nullptr;
  const char* t_fmt_ampm = nullptr;
  const char* am_str = nullptr;
  const char* pm_str = nullptr;
  const char* day_1 = nullptr;
  const char* day_2 = nullptr;
  const char* day_3 = nullptr;
  const char* day_4 = nullptr;
  const char* day_5 = nullptr;
  const char* day_6 = nullptr;
  const char* day_7 = nullptr;
  const char* abday_1 = nullptr;
  const char* abday_2 = nullptr;
  const char* abday_3 = nullptr;
  const char* abday_4 = nullptr;
  const char* abday_5 = nullptr;
  const char* abday_6 = nullptr;
  const char* abday_7 = nullptr;
  const char* mon_1 = nullptr;
  const char* mon_2 = nullptr;
  const char* mon_3 = nullptr;
  const char* mon_4 = nullptr;
  const char* mon_5 = nullptr;
  const char* mon_6 = nullptr;
  const char* mon_7 = nullptr;
  const char* mon_8 = nullptr;
  const char* mon_9 = nullptr;
  const char* mon_10 = nullptr;
  const char* mon_11 = nullptr;
  const char* mon_12 = nullptr;
  const char* abmon_1 = nullptr;
  const char* abmon_2 = nullptr;
  const char* abmon_3 = nullptr;
  const char* abmon_4 = nullptr;
  const char* abmon_5 = nullptr;
  const char* abmon_6 = nullptr;
  const char* abmon_7 = nullptr;
  const char* abmon_8 = nullptr;
  const char* abmon_9 = nullptr;
  const char* abmon_10 = nullptr;
  const char* abmon_11 = nullptr;
  const char* abmon_12 = nullptr;

  // LC_MESSAGES
  const char* yesexpr = nullptr;
  const char* noexpr = nullptr;

  friend void PrintTo(const LanginfoTestData& data, ::std::ostream* os);
};

void PrintTo(const LanginfoTestData& data, ::std::ostream* os) {
  *os << "{ locale_name: " << (data.locale_name ? data.locale_name : "null")
      << " ... }";
}

struct LanginfoTestDataNameGenerator {
  std::string operator()(
      const ::testing::TestParamInfo<LanginfoTestData>& info) const {
    std::string name = info.param.locale_name;
    std::replace(name.begin(), name.end(), '@', '_');
    return name;
  }
};

#include "starboard/nplb/posix_compliance/posix_locale_langinfo_test_data.cc.inc"

void CheckItem(nl_item item, const char* expected) {
  if (expected == nullptr) {
    return;
  }
  const char* actual = nl_langinfo(item);
  if (expected == kNonEmptyString) {
    ASSERT_NE(actual, nullptr);
    EXPECT_STRNE(actual, "");
  } else if (item == CODESET && strcmp(expected, "US-ASCII") == 0) {
    // The "C" locale can have multiple valid codesets.
    EXPECT_TRUE(strcmp(actual, "US-ASCII") == 0 ||
                strcmp(actual, "ANSI_X3.4-1968") == 0 ||
                strcmp(actual, "UTF-8") == 0);
  } else {
    EXPECT_STREQ(actual, expected);
  }
}

void CheckItemL(nl_item item, const char* expected, locale_t locale) {
  if (expected == nullptr) {
    return;
  }
  const char* actual = nl_langinfo_l(item, locale);
  if (expected == kNonEmptyString) {
    ASSERT_NE(actual, nullptr);
    EXPECT_STRNE(actual, "");
  } else if (item == CODESET && strcmp(expected, "US-ASCII") == 0) {
    // The "C" locale can have multiple valid codesets.
    EXPECT_TRUE(strcmp(actual, "US-ASCII") == 0 ||
                strcmp(actual, "ANSI_X3.4-1968") == 0 ||
                strcmp(actual, "UTF-8") == 0);
  } else {
    EXPECT_STREQ(actual, expected);
  }
}

class NlLanginfoTest : public ::testing::TestWithParam<LanginfoTestData> {
 protected:
  void SetUp() override {
    const LanginfoTestData& data = GetParam();
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

// The following nl_items are not tested due to various reasons, which
// are:
//
// - No ICU support:
//   - YESEXPR
//   - NOEXPR
//
// - Not currently implemented:
//   - ERA
//   - ERA_D_FMT
//   - ERA_D_T_FMT
//   - ERA_T_FMT
//   - CRNCYSTR
//   - ALT_DIGITS
//
// Our ICU build only formats the strings in the UTF-8 codeset, so all locales
// should always return UTF-8 for the |CODESET| item.
TEST_P(NlLanginfoTest, AllItems) {
  const LanginfoTestData& data = GetParam();
  CheckItem(RADIXCHAR, data.radixchar);
  CheckItem(THOUSEP, data.thousands_sep);
  CheckItem(CODESET, data.codeset);
  // TODO: b/466160361 - Add remaining support for D_FMT* operations.
  CheckItem(AM_STR, data.am_str);
  CheckItem(PM_STR, data.pm_str);
  for (int i = 0; i < 7; ++i) {
    CheckItem(DAY_1 + i, (&data.day_1)[i]);
    CheckItem(ABDAY_1 + i, (&data.abday_1)[i]);
  }
  for (int i = 0; i < 12; ++i) {
    CheckItem(MON_1 + i, (&data.mon_1)[i]);
    CheckItem(ABMON_1 + i, (&data.abmon_1)[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(Posix,
                         NlLanginfoTest,
                         ::testing::ValuesIn(kLanginfoTestData),
                         LanginfoTestDataNameGenerator());

class NlLanginfoLTest : public ::testing::TestWithParam<LanginfoTestData> {};

// The following nl_items are not tested due to various reasons, which
// are:
//
// - No ICU support:
//   - YESEXPR
//   - NOEXPR
//
// - Not currently implemented:
//   - ERA
//   - ERA_D_FMT
//   - ERA_D_T_FMT
//   - ERA_T_FMT
//   - CRNCYSTR
//   - ALT_DIGITS
//
// Our ICU build only formats the strings in the UTF-8 codeset, so all locales
// should always return UTF-8 for the |CODESET| item.
TEST_P(NlLanginfoLTest, AllItems) {
  const LanginfoTestData& data = GetParam();
  locale_t locale = newlocale(LC_ALL_MASK, data.locale_name, (locale_t)0);
  if (!locale) {
    GTEST_SKIP() << "Locale " << data.locale_name << " not supported.";
  }
  CheckItemL(RADIXCHAR, data.radixchar, locale);
  CheckItemL(THOUSEP, data.thousands_sep, locale);
  CheckItemL(CODESET, data.codeset, locale);
  // TODO: b/466160361 - Add remaining support for D_FMT* operations.
  CheckItemL(AM_STR, data.am_str, locale);
  CheckItemL(PM_STR, data.pm_str, locale);
  for (int i = 0; i < 7; ++i) {
    CheckItemL(DAY_1 + i, (&data.day_1)[i], locale);
    CheckItemL(ABDAY_1 + i, (&data.abday_1)[i], locale);
  }
  for (int i = 0; i < 12; ++i) {
    CheckItemL(MON_1 + i, (&data.mon_1)[i], locale);
    CheckItemL(ABMON_1 + i, (&data.abmon_1)[i], locale);
  }

  freelocale(locale);
}

INSTANTIATE_TEST_SUITE_P(Posix,
                         NlLanginfoLTest,
                         ::testing::ValuesIn(kLanginfoTestData),
                         LanginfoTestDataNameGenerator());

// Note: Calling nl_langinfo_l with a null locale is undefined behavior
// according to the POSIX standard. While some implementations like glibc return
// an empty string, others may crash. Therefore, we do not test this case.

// --- Dumper Test ---

const char* kLocaleFormats[] = {
    "af_ZA", "sq_AL",       "am_ET", "ar_SA",  "hy_AM", "as_IN", "az_AZ",
    "bn_BD", "eu_ES",       "be_BY", "bs_BA",  "bg_BG", "ca_ES", "zh_CN",
    "zh_HK", "zh_TW",       "hr_HR", "cs_CZ",  "da_DK", "nl_NL", "en_US",
    "en_IN", "en_GB",       "et_EE", "fil_PH", "fi_FI", "fr_FR", "fr_CA",
    "gl_ES", "ka_GE",       "de_DE", "el_GR",  "gu_IN", "he_IL", "hi_IN",
    "hu_HU", "is_IS",       "id_ID", "it_IT",  "ja_JP", "kn_IN", "kk_KZ",
    "km_KH", "ko_KR",       "ky_KG", "lo_LA",  "lv_LV", "lt_LT", "mk_MK",
    "ms_MY", "ml_IN",       "mr_IN", "mn_MN",  "my_MM", "ne_NP", "or_IN",
    "fa_IR", "pl_PL",       "pt_BR", "pt_PT",  "pa_IN", "ro_RO", "ru_RU",
    "sr_RS", "sr_RS@latin", "si_LK", "sk_SK",  "sl_SI", "es_MX", "es_ES",
    "es_US", "sw_KE",       "sv_SE", "ta_IN",  "te_IN", "th_TH", "tr_TR",
    "uk_UA", "ur_PK",       "uz_UZ", "vi_VN",  "zu_ZA"};

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
      const unsigned char* next_p = p + 1;
      if (*next_p && isxdigit(*next_p)) {
        result += "\"\"";
      }
    }
  }
  return result;
}

void DumpItem(FILE* file, const char* name, nl_item item) {
  const char* value = nl_langinfo(item);
  if (value) {
    fprintf(file, "     .%s = \"%s\",\n", name, EscapeString(value).c_str());
  }
}

TEST(PosixLocaleLanginfoDumperTest, DISABLED_DumpAllLocaleDataForConstexpr) {
  FILE* file = fopen(
      "starboard/nplb/posix_compliance/posix_locale_langinfo_test_data.inc",
      "w");
  ASSERT_NE(file, nullptr);

  fprintf(file, "constexpr LanginfoTestData kLanginfoTestData[] = {\n");
  for (const char* locale_name : kLocaleFormats) {
    if (!setlocale(LC_ALL, locale_name)) {
      continue;
    }
    fprintf(file, "    {\n");
    fprintf(file, "     .locale_name = \"%s\",\n", locale_name);
    DumpItem(file, "radixchar", RADIXCHAR);
    DumpItem(file, "thousands_sep", THOUSEP);
    DumpItem(file, "codeset", CODESET);
    DumpItem(file, "d_t_fmt", D_T_FMT);
    DumpItem(file, "d_fmt", D_FMT);
    DumpItem(file, "t_fmt", T_FMT);
    DumpItem(file, "t_fmt_ampm", T_FMT_AMPM);
    DumpItem(file, "am_str", AM_STR);
    DumpItem(file, "pm_str", PM_STR);
    DumpItem(file, "day_1", DAY_1);
    DumpItem(file, "day_2", DAY_2);
    DumpItem(file, "day_3", DAY_3);
    DumpItem(file, "day_4", DAY_4);
    DumpItem(file, "day_5", DAY_5);
    DumpItem(file, "day_6", DAY_6);
    DumpItem(file, "day_7", DAY_7);
    DumpItem(file, "abday_1", ABDAY_1);
    DumpItem(file, "abday_2", ABDAY_2);
    DumpItem(file, "abday_3", ABDAY_3);
    DumpItem(file, "abday_4", ABDAY_4);
    DumpItem(file, "abday_5", ABDAY_5);
    DumpItem(file, "abday_6", ABDAY_6);
    DumpItem(file, "abday_7", ABDAY_7);
    DumpItem(file, "mon_1", MON_1);
    DumpItem(file, "mon_2", MON_2);
    DumpItem(file, "mon_3", MON_3);
    DumpItem(file, "mon_4", MON_4);
    DumpItem(file, "mon_5", MON_5);
    DumpItem(file, "mon_6", MON_6);
    DumpItem(file, "mon_7", MON_7);
    DumpItem(file, "mon_8", MON_8);
    DumpItem(file, "mon_9", MON_9);
    DumpItem(file, "mon_10", MON_10);
    DumpItem(file, "mon_11", MON_11);
    DumpItem(file, "mon_12", MON_12);
    DumpItem(file, "abmon_1", ABMON_1);
    DumpItem(file, "abmon_2", ABMON_2);
    DumpItem(file, "abmon_3", ABMON_3);
    DumpItem(file, "abmon_4", ABMON_4);
    DumpItem(file, "abmon_5", ABMON_5);
    DumpItem(file, "abmon_6", ABMON_6);
    DumpItem(file, "abmon_7", ABMON_7);
    DumpItem(file, "abmon_8", ABMON_8);
    DumpItem(file, "abmon_9", ABMON_9);
    DumpItem(file, "abmon_10", ABMON_10);
    DumpItem(file, "abmon_11", ABMON_11);
    DumpItem(file, "abmon_12", ABMON_12);
    DumpItem(file, "yesexpr", YESEXPR);
    DumpItem(file, "noexpr", NOEXPR);
    fprintf(file, "    },\n");
  }
  fprintf(file, "};");
  fclose(file);
}
