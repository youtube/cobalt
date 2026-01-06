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

#include "cobalt/common/libc/locale/locale_support.h"

#include <array>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/ucnv.h"

namespace cobalt {
namespace {

// Common Locales used for testing.
constexpr char kEnUsLocale[] = "en_US";
constexpr char kEnUsUTF8Locale[] = "en_US.UTF-8";
constexpr char kFrFrLocale[] = "fr_FR";
constexpr char kSrRsLatinLocale[] = "sr_RS@latin";

TEST(GetCanonicalLocaleTest, HandlesStandardPosixLiterals) {
  EXPECT_EQ(GetCanonicalLocale(kCLocale), kCLocale);
  EXPECT_EQ(GetCanonicalLocale(kCLocale), kCLocale);
}

TEST(GetCanonicalLocaleTest, NormalizesBcp47ToPosix) {
  std::string res = GetCanonicalLocale("en-US");
  if (!res.empty()) {
    EXPECT_EQ(res, kEnUsLocale);
  }
}

TEST(GetCanonicalLocaleTest, PreservesEncodingSuffix) {
  std::string res = GetCanonicalLocale(kEnUsUTF8Locale);
  if (!res.empty()) {
    EXPECT_EQ(res, kEnUsUTF8Locale);
  }
}

TEST(GetCanonicalLocaleTest, ConvertsScriptToModifier) {
  std::string res = GetCanonicalLocale("sr_Latn_RS");
  if (!res.empty()) {
    EXPECT_EQ(res, kSrRsLatinLocale);
  }
}

TEST(GetCanonicalLocaleTest, HandlesEmptyOrNullInput) {
  EXPECT_EQ(GetCanonicalLocale(nullptr), "");
  EXPECT_EQ(GetCanonicalLocale(""), "");
}

TEST(GetCanonicalLocaleTest, RejectsUnsupportedLocales) {
  EXPECT_EQ(GetCanonicalLocale("xx_YY"), "");
}

class SyncIcuDefaultTest : public ::testing::Test {
 protected:
  void SetUp() override { original_default_ = icu::Locale::getDefault(); }

  void TearDown() override {
    UErrorCode status = U_ZERO_ERROR;
    icu::Locale::setDefault(original_default_, status);
  }

  icu::Locale original_default_;
};

TEST_F(SyncIcuDefaultTest, UpdatesGlobalDefaultFromStandardLocale) {
  SyncIcuDefault(kFrFrLocale);

  icu::Locale current = icu::Locale::getDefault();
  EXPECT_STREQ(current.getName(), kFrFrLocale);
}

TEST_F(SyncIcuDefaultTest, CanonicalizesPosixModifiersToIcu) {
  std::string canonical_locale = GetCanonicalLocale(kSrRsLatinLocale);
  SyncIcuDefault(canonical_locale);
  icu::Locale current = icu::Locale::getDefault();

  // The ICU documentation states that when canonicalizing POSIX variants, it
  // will normalize the variant "@latin" to "_LATIN." Source:
  // https://unicode-org.github.io/icu/userguide/locale/#canonicalization
  EXPECT_STREQ(current.getName(), "sr_RS_LATIN");
}

TEST_F(SyncIcuDefaultTest, HandlesRootLocale) {
  SyncIcuDefault("C");

  icu::Locale current = icu::Locale::getDefault();
  EXPECT_FALSE(current.isBogus());
}

class ParseCompositeLocaleTest : public ::testing::Test {
 protected:
  LocaleImpl current_state_;  // Defaults to "C"
  std::array<std::string, LC_ALL> out_categories_;
};

TEST_F(ParseCompositeLocaleTest, ParsesValidString) {
  const char* input = "LC_CTYPE=C;LC_TIME=en_US";

  EXPECT_TRUE(ParseCompositeLocale(input, current_state_, out_categories_));

  EXPECT_EQ(out_categories_[LC_CTYPE], kCLocale);
  EXPECT_EQ(out_categories_[LC_TIME], kEnUsLocale);
}

TEST_F(ParseCompositeLocaleTest, RejectsSimpleString) {
  EXPECT_FALSE(
      ParseCompositeLocale(kEnUsLocale, current_state_, out_categories_));
}

TEST_F(ParseCompositeLocaleTest, RejectsInvalidCategoryNames) {
  EXPECT_FALSE(
      ParseCompositeLocale("LC_GARBAGE=C", current_state_, out_categories_));
}

TEST(RefreshCompositeStringTest, GeneratesSimpleStringForUniformState) {
  LocaleImpl loc;
  for (int i = 0; i < LC_ALL; ++i) {
    loc.categories[i] = kEnUsLocale;
  }
  RefreshCompositeString(&loc);
  EXPECT_EQ(loc.composite_lc_all, kEnUsLocale);
}

TEST(RefreshCompositeStringTest, GeneratesCompositeStringForMixedState) {
  LocaleImpl loc;
  loc.categories[LC_NUMERIC] = kFrFrLocale;
  RefreshCompositeString(&loc);

  EXPECT_NE(loc.composite_lc_all, kCLocale);
  EXPECT_NE(loc.composite_lc_all.find("LC_CTYPE=C"), std::string::npos);
  EXPECT_NE(loc.composite_lc_all.find("LC_NUMERIC=fr_FR"), std::string::npos);
  EXPECT_NE(loc.composite_lc_all.find(";"), std::string::npos);
}

TEST(UpdateLocaleSettingsTest, UpdatesSpecificCategoryMask) {
  LocaleImpl loc;
  UpdateLocaleSettings(LC_NUMERIC_MASK, kFrFrLocale, &loc);

  EXPECT_EQ(loc.categories[LC_NUMERIC], kFrFrLocale);
  EXPECT_EQ(loc.categories[LC_CTYPE], kCLocale);
}

TEST(UpdateLocaleSettingsTest, UpdatesAllCategoriesWithAllMask) {
  LocaleImpl loc;
  UpdateLocaleSettings(LC_ALL_MASK, kEnUsLocale, &loc);

  for (int i = 0; i < LC_ALL; ++i) {
    EXPECT_EQ(loc.categories[i], kEnUsLocale);
  }
}

}  // namespace
}  // namespace cobalt
