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

namespace cobalt {
namespace {

TEST(GetCanonicalLocaleTest, HandlesStandardPosixLiterals) {
  EXPECT_EQ(GetCanonicalLocale("C"), "C");
  EXPECT_EQ(GetCanonicalLocale("POSIX"), "POSIX");
}

TEST(GetCanonicalLocaleTest, NormalizesBcp47ToPosix) {
  std::string res = GetCanonicalLocale("en-US");
  if (!res.empty()) {
    EXPECT_EQ(res, "en_US");
  }
}

TEST(GetCanonicalLocaleTest, PreservesEncodingSuffix) {
  std::string res = GetCanonicalLocale("en_US.UTF-8");
  if (!res.empty()) {
    EXPECT_EQ(res, "en_US.UTF-8");
  }
}

TEST(GetCanonicalLocaleTest, ConvertsScriptToModifier) {
  std::string res = GetCanonicalLocale("sr-Latn-RS");
  if (!res.empty()) {
    EXPECT_EQ(res, "sr_RS@latin");
  }
}

TEST(GetCanonicalLocaleTest, HandlesEmptyOrNullInput) {
  EXPECT_EQ(GetCanonicalLocale(nullptr), "");
  EXPECT_EQ(GetCanonicalLocale(""), "");
}

TEST(GetCanonicalLocaleTest, RejectsUnsupportedLocales) {
  EXPECT_EQ(GetCanonicalLocale("xx_YY"), "");
}

class ParseCompositeLocaleTest : public ::testing::Test {
 protected:
  LocaleImpl current_state_;  // Defaults to "C"
  std::array<std::string, kCobaltLcCount> out_categories_;
};

TEST_F(ParseCompositeLocaleTest, ParsesValidString) {
  const char* input = "LC_CTYPE=C;LC_TIME=en_US";

  EXPECT_TRUE(ParseCompositeLocale(input, current_state_, out_categories_));

  EXPECT_EQ(out_categories_[kCobaltLcCtype], "C");
  EXPECT_EQ(out_categories_[kCobaltLcTime], "en_US");
}

TEST_F(ParseCompositeLocaleTest, RejectsSimpleString) {
  EXPECT_FALSE(ParseCompositeLocale("en_US", current_state_, out_categories_));
}

TEST_F(ParseCompositeLocaleTest, RejectsInvalidCategoryNames) {
  EXPECT_FALSE(
      ParseCompositeLocale("LC_GARBAGE=C", current_state_, out_categories_));
}

TEST(RefreshCompositeStringTest, GeneratesSimpleStringForUniformState) {
  LocaleImpl loc;
  for (int i = 0; i < kCobaltLcCount; ++i) {
    loc.categories[i] = "en_US";
  }
  RefreshCompositeString(&loc);
  EXPECT_EQ(loc.composite_lc_all, "en_US");
}

TEST(RefreshCompositeStringTest, GeneratesCompositeStringForMixedState) {
  LocaleImpl loc;
  loc.categories[kCobaltLcNumeric] = "fr_FR";
  RefreshCompositeString(&loc);

  EXPECT_NE(loc.composite_lc_all, "C");
  EXPECT_NE(loc.composite_lc_all.find("LC_CTYPE=C"), std::string::npos);
  EXPECT_NE(loc.composite_lc_all.find("LC_NUMERIC=fr_FR"), std::string::npos);
  EXPECT_NE(loc.composite_lc_all.find(";"), std::string::npos);
}

TEST(UpdateLocaleSettingsTest, UpdatesSpecificCategoryMask) {
  LocaleImpl loc;
  UpdateLocaleSettings(LC_NUMERIC_MASK, "fr_FR", &loc);

  EXPECT_EQ(loc.categories[kCobaltLcNumeric], "fr_FR");
  EXPECT_EQ(loc.categories[kCobaltLcCtype], "C");
}

TEST(UpdateLocaleSettingsTest, UpdatesAllCategoriesWithAllMask) {
  LocaleImpl loc;
  UpdateLocaleSettings(LC_ALL_MASK, "en_US", &loc);

  for (int i = 0; i < kCobaltLcCount; ++i) {
    EXPECT_EQ(loc.categories[i], "en_US");
  }
}

}  // namespace
}  // namespace cobalt
