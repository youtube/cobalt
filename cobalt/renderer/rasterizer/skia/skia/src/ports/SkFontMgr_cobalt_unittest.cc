// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontMgr_cobalt.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkString.h"
#include "include/core/SkTypeface.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SkFontMgrCobaltTest : public ::testing::Test {
 protected:
  void SetUp() override {
    font_mgr_ = skia::DefaultFontMgr();
    ASSERT_TRUE(font_mgr_ != nullptr);
  }

  sk_sp<SkFontMgr> font_mgr_;
};

TEST_F(SkFontMgrCobaltTest, DefaultTypefaceIsValid) {
  sk_sp<SkTypeface> typeface = skia::DefaultTypeface();
  ASSERT_TRUE(typeface != nullptr);

  SkString family_name;
  typeface->getFamilyName(&family_name);
  EXPECT_FALSE(family_name.isEmpty());

  SkFont font(typeface, 16.0f);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  EXPECT_LT(metrics.fAscent, 0.0f);
  EXPECT_GT(metrics.fDescent, 0.0f);
}

TEST_F(SkFontMgrCobaltTest, GenericFontFamiliesMatchSuccessfully) {
  struct TestCase {
    const char* requested_family;
    const char* expected_family_name;
  };

  const std::vector<TestCase> test_cases = {
      {"sans-serif", "sans-serif"},
      {"serif", "serif"},
      {"monospace", "monospace"},
      {"casual", "casual"},
      {"cursive", "cursive"},
      {"sans-serif-smallcaps", "sans-serif-smallcaps"},
      {"serif-monospace", "serif-monospace"},
      {"sans-serif-monospace", "monospace"},
      {"roboto", "sans-serif"},
      {"fantasy", "serif"},
  };

  for (const auto& test_case : test_cases) {
    sk_sp<SkTypeface> typeface(
        font_mgr_->matchFamilyStyle(test_case.requested_family, SkFontStyle()));
    ASSERT_TRUE(typeface != nullptr)
        << "Failed to match family: " << test_case.requested_family;

    SkString actual_family;
    typeface->getFamilyName(&actual_family);
    EXPECT_STREQ(actual_family.c_str(), test_case.expected_family_name)
        << "Family for '" << test_case.requested_family << "' resolved to '"
        << actual_family.c_str() << "', expected '"
        << test_case.expected_family_name << "'";

    // Verify font metrics (ascent, descent, line spacing)
    SkFont font(typeface, 24.0f);
    SkFontMetrics metrics;
    SkScalar line_spacing = font.getMetrics(&metrics);
    EXPECT_GT(line_spacing, 0.0f);
    EXPECT_LT(metrics.fAscent, 0.0f);
    EXPECT_GT(metrics.fDescent, 0.0f);

    // Verify glyph generation and measurement
    SkGlyphID glyph_id = font.unicharToGlyph('A');
    EXPECT_GT(glyph_id, 0u)
        << "No glyph for 'A' in " << test_case.requested_family;

    SkScalar width;
    font.getWidths(&glyph_id, 1, &width);
    EXPECT_GT(width, 0.0f) << "Zero advance width for 'A' in "
                           << test_case.requested_family;
  }
}

TEST_F(SkFontMgrCobaltTest, FontStylesWeightAndItalic) {
  // Test Bold, Italic, Normal for sans-serif (Roboto)
  sk_sp<SkTypeface> regular(
      font_mgr_->matchFamilyStyle("sans-serif", SkFontStyle::Normal()));
  ASSERT_TRUE(regular != nullptr);
  EXPECT_FALSE(regular->isBold());
  EXPECT_FALSE(regular->isItalic());

  sk_sp<SkTypeface> bold(
      font_mgr_->matchFamilyStyle("sans-serif", SkFontStyle::Bold()));
  ASSERT_TRUE(bold != nullptr);
  EXPECT_TRUE(bold->isBold());

  sk_sp<SkTypeface> italic(
      font_mgr_->matchFamilyStyle("sans-serif", SkFontStyle::Italic()));
  ASSERT_TRUE(italic != nullptr);
  EXPECT_TRUE(italic->isItalic());

  // Test Bold for cursive (Dancing Script)
  sk_sp<SkTypeface> cursive_bold(
      font_mgr_->matchFamilyStyle("cursive", SkFontStyle::Bold()));
  ASSERT_TRUE(cursive_bold != nullptr);
  EXPECT_TRUE(cursive_bold->isBold());
}

TEST_F(SkFontMgrCobaltTest, CharacterFallbackForMultilingualScripts) {
  struct FallbackTestCase {
    const char* script_name;
    SkUnichar character;
    const char* bcp47_locale;
  };

  const std::vector<FallbackTestCase> fallback_cases = {
      {"Arabic", 0x0628 /* BEH */, "ar"},  {"Hebrew", 0x05D0 /* ALEF */, "he"},
      {"Thai", 0x0E01 /* KO KAI */, "th"}, {"Devanagari", 0x0905 /* A */, "hi"},
      {"Ethiopic", 0x1200 /* HA */, "am"}, {"Georgian", 0x10D0 /* AN */, "ka"},
      {"Tamil", 0x0B95 /* KA */, "ta"},
  };

  for (const auto& tc : fallback_cases) {
    const char* bcp47[] = {tc.bcp47_locale};
    sk_sp<SkTypeface> typeface(font_mgr_->matchFamilyStyleCharacter(
        "sans-serif", SkFontStyle(), bcp47, 1, tc.character));
    ASSERT_TRUE(typeface != nullptr)
        << "Fallback failed for script " << tc.script_name;

    SkFont font(typeface, 20.0f);
    SkGlyphID glyph = font.unicharToGlyph(tc.character);
    EXPECT_GT(glyph, 0u) << "No glyph resolved for script " << tc.script_name;

    SkFontMetrics metrics;
    font.getMetrics(&metrics);
    EXPECT_LT(metrics.fAscent, 0.0f);
    EXPECT_GT(metrics.fDescent, 0.0f);
  }
}

TEST_F(SkFontMgrCobaltTest, LatinCharactersDoNotFallbackToNonLatinFonts) {
  // Test that Latin character 'e' with Arabic locale requested still returns
  // sans-serif
  const char* bcp47[] = {"ar"};
  sk_sp<SkTypeface> typeface(font_mgr_->matchFamilyStyleCharacter(
      "sans-serif", SkFontStyle(), bcp47, 1, 'e'));
  ASSERT_TRUE(typeface != nullptr);

  SkString family_name;
  typeface->getFamilyName(&family_name);
  EXPECT_STREQ(family_name.c_str(), "sans-serif")
      << "Latin character resolved to unexpected font: " << family_name.c_str();
}

TEST_F(SkFontMgrCobaltTest, UninstalledFontFamiliesReturnNull) {
  // Verifies that querying font names not installed on the system (e.g. Courier
  // New, Times New Roman, Arial, unknown fonts) returns an empty style set from
  // matchFamily and nullptr from matchFamilyStyle so Blink can continue down
  // the CSS font-family fallback list.
  const std::vector<const char*> uninstalled_families = {
      "Courier New", "Courier",   "Times New Roman", "Times",
      "Arial",       "Helvetica", "Georgia",         "NonExistentFontName",
  };

  for (const char* family : uninstalled_families) {
    sk_sp<SkFontStyleSet> style_set(font_mgr_->matchFamily(family));
    ASSERT_TRUE(style_set != nullptr);
    EXPECT_EQ(style_set->count(), 0)
        << "matchFamily should return empty style set for uninstalled family: "
        << family;

    sk_sp<SkTypeface> typeface(
        font_mgr_->matchFamilyStyle(family, SkFontStyle()));
    EXPECT_TRUE(typeface == nullptr)
        << "matchFamilyStyle should return null for uninstalled family: "
        << family;
  }
}

TEST_F(SkFontMgrCobaltTest, CssFontFamilyFallbackSimulation) {
  // Simulates Blink's FontFallbackList iterating over a CSS font-family list:
  // e.g. `font-family: "Courier New", Courier, monospace;`
  const std::vector<const char*> css_font_family_list = {
      "Courier New", "Courier", "monospace"};

  sk_sp<SkTypeface> resolved_typeface = nullptr;
  for (const char* family : css_font_family_list) {
    resolved_typeface = font_mgr_->matchFamilyStyle(family, SkFontStyle());
    if (resolved_typeface != nullptr) {
      break;
    }
  }

  ASSERT_TRUE(resolved_typeface != nullptr);
  SkString resolved_family;
  resolved_typeface->getFamilyName(&resolved_family);
  EXPECT_STREQ(resolved_family.c_str(), "monospace");
}

TEST_F(SkFontMgrCobaltTest, NullFamilyNameReturnsDefaultFamily) {
  sk_sp<SkTypeface> typeface_style =
      font_mgr_->matchFamilyStyle(nullptr, SkFontStyle());
  ASSERT_TRUE(typeface_style != nullptr);
  SkString name_style;
  typeface_style->getFamilyName(&name_style);
  EXPECT_STREQ(name_style.c_str(), "sans-serif");

  sk_sp<SkTypeface> legacy_typeface =
      font_mgr_->legacyMakeTypeface(nullptr, SkFontStyle());
  ASSERT_TRUE(legacy_typeface != nullptr);
  SkString name_legacy;
  legacy_typeface->getFamilyName(&name_legacy);
  EXPECT_STREQ(name_legacy.c_str(), "sans-serif");
}

}  // namespace
