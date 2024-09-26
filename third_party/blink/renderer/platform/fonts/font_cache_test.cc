// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include <unicode/unistr.h>
#include <string>
#include <tuple>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

class FontCacheTest : public FontTestBase {};

TEST_F(FontCacheTest, getLastResortFallbackFont) {
  FontCache& font_cache = FontCache::Get();

  // Perform the test for the default font family (kStandardFamily) and the
  // -webkit-body font family (kWebkitBodyFamily) since they behave the same in
  // term of font/glyph selection.
  // TODO(crbug.com/1065468): Remove the test for kWebkitBodyFamily when
  // -webkit-body in unshipped.
  for (FontDescription::GenericFamilyType family_type :
       {FontDescription::kStandardFamily, FontDescription::kWebkitBodyFamily,
        FontDescription::kSansSerifFamily}) {
    FontDescription font_description;
    font_description.SetGenericFamily(family_type);
    scoped_refptr<SimpleFontData> font_data =
        font_cache.GetLastResortFallbackFont(font_description, kRetain);
    EXPECT_TRUE(font_data);
  }
}

TEST_F(FontCacheTest, NoFallbackForPrivateUseArea) {
  FontCache& font_cache = FontCache::Get();

  // Perform the test for the default font family (kStandardFamily) and the
  // -webkit-body font family (kWebkitBodyFamily) since they behave the same in
  // term of font/glyph selection.
  // TODO(crbug.com/1065468): Remove the test for kWebkitBodyFamily when
  // -webkit-body in unshipped.
  for (FontDescription::GenericFamilyType family_type :
       {FontDescription::kStandardFamily, FontDescription::kWebkitBodyFamily}) {
    FontDescription font_description;
    font_description.SetGenericFamily(family_type);
    for (UChar32 character : {0xE000, 0xE401, 0xE402, 0xE403, 0xF8FF, 0xF0000,
                              0xFAAAA, 0x100000, 0x10AAAA}) {
      scoped_refptr<SimpleFontData> font_data =
          font_cache.FallbackFontForCharacter(font_description, character,
                                              nullptr);
      EXPECT_EQ(font_data.get(), nullptr);
    }
  }
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(FontCacheTest, FallbackForEmojis) {
  FontCache& font_cache = FontCache::Get();
  FontCachePurgePreventer purge_preventer;

  // Perform the test for the default font family (kStandardFamily) and the
  // -webkit-body font family (kWebkitBodyFamily) since they behave the same in
  // term of font/glyph selection.
  // TODO(crbug.com/1065468): Remove the test for kWebkitBodyFamily when
  // -webkit-body in unshipped.
  for (FontDescription::GenericFamilyType family_type :
       {FontDescription::kStandardFamily, FontDescription::kWebkitBodyFamily}) {
    FontDescription font_description;
    font_description.SetGenericFamily(family_type);

    static constexpr char kNotoColorEmoji[] = "Noto Color Emoji";

    // We should use structured binding when it becomes available...
    for (auto info : {
             std::pair<UChar32, bool>{U'☺', true},
             {U'👪', true},
             {U'🤣', false},
         }) {
      UChar32 character = info.first;
      // Set to true if the installed contour fonts support this glyph.
      bool available_in_contour_font = info.second;
      std::string character_utf8;
      icu::UnicodeString(character).toUTF8String(character_utf8);

      {
        scoped_refptr<SimpleFontData> font_data =
            font_cache.FallbackFontForCharacter(
                font_description, character, nullptr,
                FontFallbackPriority::kEmojiEmoji);
        EXPECT_EQ(font_data->PlatformData().FontFamilyName(), kNotoColorEmoji)
            << "Character " << character_utf8
            << " doesn't match what we expected for kEmojiEmoji.";
      }
      {
        scoped_refptr<SimpleFontData> font_data =
            font_cache.FallbackFontForCharacter(
                font_description, character, nullptr,
                FontFallbackPriority::kEmojiText);
        if (available_in_contour_font) {
          EXPECT_NE(font_data->PlatformData().FontFamilyName(), kNotoColorEmoji)
              << "Character " << character_utf8
              << " doesn't match what we expected for kEmojiText.";
        } else {
          EXPECT_EQ(font_data->PlatformData().FontFamilyName(), kNotoColorEmoji)
              << "Character " << character_utf8
              << " doesn't match what we expected for kEmojiText.";
        }
      }
    }
  }
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

TEST_F(FontCacheTest, firstAvailableOrFirst) {
  EXPECT_TRUE(FontCache::FirstAvailableOrFirst("").empty());
  EXPECT_TRUE(FontCache::FirstAvailableOrFirst(String()).empty());

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("Arial"));
  EXPECT_EQ("not exist", FontCache::FirstAvailableOrFirst("not exist"));

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("Arial, not exist"));
  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("not exist, Arial"));
  EXPECT_EQ("Arial",
            FontCache::FirstAvailableOrFirst("not exist, Arial, not exist"));

  EXPECT_EQ("not exist",
            FontCache::FirstAvailableOrFirst("not exist, not exist 2"));

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst(", not exist, Arial"));
  EXPECT_EQ("not exist",
            FontCache::FirstAvailableOrFirst(", not exist, not exist"));
}

// https://crbug.com/969402
TEST_F(FontCacheTest, GetLargerThanMaxUnsignedFont) {
  FontCache& font_cache = FontCache::Get();

  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kStandardFamily);
  font_description.SetComputedSize(
      static_cast<float>(std::numeric_limits<unsigned>::max()) + 1.f);
  FontFaceCreationParams creation_params;
  scoped_refptr<blink::SimpleFontData> font_data =
      font_cache.GetFontData(font_description, AtomicString());
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
  // Unfortunately, we can't ensure a font here since on Android and Mac the
  // unittests can't access the font configuration. However, this test passes
  // when it's not crashing in FontCache.
  EXPECT_TRUE(font_data);
#endif
}

#if !BUILDFLAG(IS_MAC)
TEST_F(FontCacheTest, systemFont) {
  FontCache::SystemFontFamily();
  // Test the function does not crash. Return value varies by system and config.
}
#endif

#if BUILDFLAG(IS_ANDROID)
TEST_F(FontCacheTest, Locale) {
  FontCacheKey key1(FontFaceCreationParams(), /* font_size */ 16,
                    /* options */ 0, /* device_scale_factor */ 1.0f,
                    /* variation_settings */ nullptr,
                    /* palette */ nullptr,
                    /* variant_alternates */ nullptr,
                    /* is_unique_match */ false,
                    /* is_generic_family */ false);
  FontCacheKey key2 = key1;
  EXPECT_EQ(key1.GetHash(), key2.GetHash());
  EXPECT_EQ(key1, key2);

  key2.SetLocale("ja");
  EXPECT_NE(key1.GetHash(), key2.GetHash());
  EXPECT_NE(key1, key2);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace blink
