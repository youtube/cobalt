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

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkWoff2FontCache_cobalt.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkStream.h"
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
  // Verifies that querying font names not installed on the system returns an
  // empty style set from matchFamily and nullptr from matchFamilyStyle so Blink
  // can continue down the CSS font-family fallback list.
  const std::vector<const char*> uninstalled_families = {
      "NonExistentFontFamilyA",
      "NonExistentFontFamilyB",
      "NonExistentFontFamilyC",
      "NonExistentFontFamilyD",
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
  // e.g. `font-family: "NonExistentFontA", "NonExistentFontB", monospace;`
  const std::vector<const char*> css_font_family_list = {
      "NonExistentFontFamilyA", "NonExistentFontFamilyB", "monospace"};

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

TEST_F(SkFontMgrCobaltTest, LimitedFontPackageLastResortFallbackSimulation) {
  // Simulates a platform using cobalt_font_package = "limited" or "minimal"
  // where only "sans-serif" (Roboto) is present in fonts.xml and optional
  // generic families like "monospace" and "serif" are omitted.
  base::ScopedTempDir temp_config_dir;
  ASSERT_TRUE(temp_config_dir.CreateUniqueTempDir());

  static constexpr char kLimitedFontsXml[] =
      R"xml(<?xml version="1.0" encoding="utf-8"?>
<familyset version="1">
  <family name="sans-serif">
    <font weight="400" style="normal">Roboto-Regular.woff2</font>
    <font weight="700" style="normal">Roboto-Bold.woff2</font>
  </family>
  <alias name="roboto" to="sans-serif" />
</familyset>
)xml";

  base::FilePath fonts_xml_path =
      temp_config_dir.GetPath().Append(FILE_PATH_LITERAL("fonts.xml"));
  ASSERT_TRUE(base::WriteFile(fonts_xml_path, kLimitedFontsXml));

  base::FilePath font_files_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM_FONTS, &font_files_dir));

  skia_private::TArray<SkString, true> default_families;
  default_families.push_back(SkString("sans-serif"));

  auto limited_font_mgr = sk_make_sp<SkFontMgr_Cobalt>(
      temp_config_dir.GetPath().value().c_str(), font_files_dir.value().c_str(),
      "", "", default_families);

  // On a limited font package, "monospace", "serif", "Sans", and "Arial" are
  // not in fonts.xml and return nullptr.
  EXPECT_EQ(limited_font_mgr->matchFamilyStyle("monospace", SkFontStyle()),
            nullptr);
  EXPECT_EQ(limited_font_mgr->matchFamilyStyle("serif", SkFontStyle()),
            nullptr);
  EXPECT_EQ(limited_font_mgr->matchFamilyStyle("Sans", SkFontStyle()), nullptr);
  EXPECT_EQ(limited_font_mgr->matchFamilyStyle("Arial", SkFontStyle()),
            nullptr);

  // When FontCache::GetLastResortFallbackFont falls back to g_empty_atom
  // (passing nullptr to matchFamilyStyle), SkFontMgr_Cobalt resolves to
  // default_families_[0] ("sans-serif").
  sk_sp<SkTypeface> fallback_typeface =
      limited_font_mgr->matchFamilyStyle(nullptr, SkFontStyle());
  ASSERT_TRUE(fallback_typeface != nullptr);
  SkString resolved_family;
  fallback_typeface->getFamilyName(&resolved_family);
  EXPECT_STREQ(resolved_family.c_str(), "sans-serif");
}

// Tests the CobaltMmapFontCache feature with font managers whose only font is
// Roboto-Regular.woff2. Each font manager simulates an app session. The feature
// is enabled unless a test disables it.
class SkFontMgrCobaltMmapFontCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_CACHE, temp_dir_.GetPath());

    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SYSTEM_FONTS, &font_files_dir_));
    woff2_path_ =
        font_files_dir_.Append(FILE_PATH_LITERAL("Roboto-Regular.woff2"));
    if (!base::PathExists(woff2_path_)) {
      GTEST_SKIP() << "Roboto-Regular.woff2 is not installed.";
    }

    static constexpr char kFontsXml[] =
        R"xml(<?xml version="1.0" encoding="utf-8"?>
<familyset version="1">
  <family name="sans-serif">
    <font weight="400" style="normal">Roboto-Regular.woff2</font>
  </family>
</familyset>
)xml";
    config_dir_ = temp_dir_.GetPath().AppendASCII("config");
    ASSERT_TRUE(base::CreateDirectory(config_dir_));
    ASSERT_TRUE(
        base::WriteFile(config_dir_.AppendASCII("fonts.xml"), kFontsXml));
  }

  void TearDown() override {
    // Let scheduled cache work finish while DIR_CACHE is still overridden.
    task_environment_.RunUntilIdle();
    cache_override_.reset();
  }

  sk_sp<SkFontMgr_Cobalt> CreateFontMgr() const {
    skia_private::TArray<SkString, true> default_families;
    default_families.push_back(SkString("sans-serif"));
    return sk_make_sp<SkFontMgr_Cobalt>(config_dir_.value().c_str(),
                                        font_files_dir_.value().c_str(), "", "",
                                        default_families);
  }

  // Returns the cache file of Roboto-Regular.woff2, or an empty path if it
  // does not exist.
  base::FilePath GetCacheFile() const {
    return base::FilePath(sk_woff2_cache_cobalt::GetCachedSfntPath(
                              SkString(woff2_path_.value().c_str()))
                              .c_str());
  }

  base::test::ScopedFeatureList feature_list_{
      sk_woff2_cache_cobalt::kCobaltMmapFontCache};
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_override_;
  base::FilePath font_files_dir_;
  base::FilePath woff2_path_;
  base::FilePath config_dir_;
};

// Font managers and typefaces can be created where blocking is disallowed
// (e.g. on the browser UI thread, as in I18nBrowserTest). With the
// CobaltMmapFontCache feature enabled, that must work both with a cold cache,
// where the typeface is backed by the font decompressed into memory and the
// cache file is written by a background task, and with a warm cache, where the
// typeface is backed by the mmap'd cache file.
TEST_F(SkFontMgrCobaltMmapFontCacheTest,
       CreatesTypefacesWhereBlockingDisallowed) {
  // Returns a stream of the font data backing |typeface|.
  auto open_font_data = [](const sk_sp<SkTypeface>& typeface) {
    int ttc_index = 0;
    return typeface->openStream(&ttc_index);
  };

  // Cold cache: the typeface is backed by the decompressed font, in memory,
  // and the cache file is written in the background. Font managers are
  // created where blocking is disallowed too, because the constructor already
  // creates the default typeface.
  sk_sp<SkFontMgr_Cobalt> cold_font_mgr;
  sk_sp<SkTypeface> cold_typeface;
  {
    base::ScopedDisallowBlocking disallow_blocking;
    cold_font_mgr = CreateFontMgr();
    cold_typeface =
        cold_font_mgr->matchFamilyStyle("sans-serif", SkFontStyle());
  }
  ASSERT_TRUE(cold_typeface);
  std::unique_ptr<SkStreamAsset> cold_data = open_font_data(cold_typeface);
  ASSERT_TRUE(cold_data);
  ASSERT_TRUE(cold_data->getMemoryBase());
  task_environment_.RunUntilIdle();

  // The cache file holds the same decompressed bytes.
  const base::FilePath cache_file = GetCacheFile();
  ASSERT_FALSE(cache_file.empty());
  std::string cache_bytes;
  ASSERT_TRUE(base::ReadFileToString(cache_file, &cache_bytes));
  EXPECT_EQ(
      cache_bytes,
      std::string_view(static_cast<const char*>(cold_data->getMemoryBase()),
                       cold_data->getLength()));

  // Warm cache (next session): the typeface is backed by the cache file.
  sk_sp<SkFontMgr_Cobalt> warm_font_mgr;
  sk_sp<SkTypeface> warm_typeface;
  {
    base::ScopedDisallowBlocking disallow_blocking;
    warm_font_mgr = CreateFontMgr();
    warm_typeface =
        warm_font_mgr->matchFamilyStyle("sans-serif", SkFontStyle());
  }
  ASSERT_TRUE(warm_typeface);
  std::unique_ptr<SkStreamAsset> warm_data = open_font_data(warm_typeface);
  ASSERT_TRUE(warm_data);
  // SkStream::MakeFromFile() maps each file once and shares the mapping.
  std::unique_ptr<SkStreamAsset> cache_file_data =
      SkStream::MakeFromFile(cache_file.value().c_str());
  ASSERT_TRUE(cache_file_data);
  EXPECT_EQ(warm_data->getMemoryBase(), cache_file_data->getMemoryBase());
  EXPECT_EQ(warm_data->getLength(), cache_bytes.size());
}

// Creating the font manager, i.e. starting an app session, deletes the stale
// entries of the font cache and keeps the cache files of the current fonts.
TEST_F(SkFontMgrCobaltMmapFontCacheTest, DeletesStaleCacheFilesAtStartup) {
  // First session: creates the cache file.
  ASSERT_TRUE(CreateFontMgr());
  task_environment_.RunUntilIdle();
  const base::FilePath cache_file = GetCacheFile();
  ASSERT_FALSE(cache_file.empty());

  // Leftovers of earlier sessions: the cache file of an earlier version of the
  // font, the cache file of a font that was removed, and the temporary file of
  // an interrupted cache file creation.
  const base::FilePath cache_dir = cache_file.DirName();
  const base::FilePath old_version =
      cache_dir.AppendASCII("Roboto-Regular.1234.5678.ttf");
  const base::FilePath removed_font =
      cache_dir.AppendASCII("RemovedFont-Regular.1234.5678.ttf");
  ASSERT_TRUE(base::WriteFile(old_version, "stale sfnt bytes"));
  ASSERT_TRUE(base::WriteFile(removed_font, "stale sfnt bytes"));
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(cache_dir, &temp_file));

  // Next session. The cleanup is scheduled where blocking is disallowed too.
  sk_sp<SkFontMgr_Cobalt> font_mgr;
  {
    base::ScopedDisallowBlocking disallow_blocking;
    font_mgr = CreateFontMgr();
  }
  ASSERT_TRUE(font_mgr);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::PathExists(cache_file));
  EXPECT_FALSE(base::PathExists(old_version));
  EXPECT_FALSE(base::PathExists(removed_font));
  EXPECT_FALSE(base::PathExists(temp_file));
}

// With the CobaltMmapFontCache feature disabled, the default, the font manager
// loads fonts as without the feature and leaves the font cache alone: it does
// not use the cache files left by a session that had the feature enabled, and
// it neither writes nor deletes any.
TEST_F(SkFontMgrCobaltMmapFontCacheTest, DoesNotUseFontCacheWhenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      sk_woff2_cache_cobalt::kCobaltMmapFontCache);

  // Files left by a session with the feature enabled: the cache file of the
  // font and a stale cache file.
  const SkString woff2_path(woff2_path_.value().c_str());
  sk_woff2_cache_cobalt::ScheduleCacheFileWrite(
      woff2_path, sk_woff2_cache_cobalt::DecompressWoff2(woff2_path));
  task_environment_.RunUntilIdle();
  const base::FilePath cache_file = GetCacheFile();
  ASSERT_FALSE(cache_file.empty());
  const base::FilePath stale_file =
      cache_file.DirName().AppendASCII("RemovedFont-Regular.1234.5678.ttf");
  ASSERT_TRUE(base::WriteFile(stale_file, "stale sfnt bytes"));

  sk_sp<SkFontMgr_Cobalt> font_mgr = CreateFontMgr();
  ASSERT_TRUE(font_mgr);
  sk_sp<SkTypeface> typeface =
      font_mgr->matchFamilyStyle("sans-serif", SkFontStyle());
  ASSERT_TRUE(typeface);
  task_environment_.RunUntilIdle();

  // The typeface reads the WOFF2 file, not the decompressed bytes of its
  // cache file.
  int ttc_index = 0;
  std::unique_ptr<SkStreamAsset> font_data = typeface->openStream(&ttc_index);
  ASSERT_TRUE(font_data);
  EXPECT_FALSE(font_data->getMemoryBase());
  EXPECT_EQ(base::GetFileSize(woff2_path_),
            static_cast<int64_t>(font_data->getLength()));
  // No cleanup ran.
  EXPECT_TRUE(base::PathExists(cache_file));
  EXPECT_TRUE(base::PathExists(stale_file));
}

}  // namespace
