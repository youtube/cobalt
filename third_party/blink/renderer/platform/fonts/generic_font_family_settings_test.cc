// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/generic_font_family_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(GenericFontFamilySettingsTest, FirstAvailableFontFamily) {
  GenericFontFamilySettings settings;
  EXPECT_TRUE(settings.Standard().empty());

  // Returns the first available font if starts with ",".
  settings.UpdateStandard(AtomicString(",not exist, Arial"));
  EXPECT_EQ("Arial", settings.Standard());

  // Otherwise returns any strings as they were set.
  AtomicString non_lists[] = {
      AtomicString("Arial"),
      AtomicString("not exist"),
      AtomicString("not exist, Arial"),
  };
  for (const AtomicString& name : non_lists) {
    settings.UpdateStandard(name);
    EXPECT_EQ(name, settings.Standard());
  }
}

TEST(GenericFontFamilySettingsTest, TestAllNames) {
  GenericFontFamilySettings settings;

  EXPECT_TRUE(settings.Standard().empty());
  EXPECT_TRUE(settings.Fixed().empty());
  EXPECT_TRUE(settings.Serif().empty());
  EXPECT_TRUE(settings.SansSerif().empty());
  EXPECT_TRUE(settings.Cursive().empty());
  EXPECT_TRUE(settings.Fantasy().empty());
  EXPECT_TRUE(settings.Math().empty());

  EXPECT_TRUE(settings.Standard(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.Fixed(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.Serif(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.SansSerif(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.Cursive(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.Fantasy(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.Math(USCRIPT_ARABIC).empty());

  settings.UpdateStandard(AtomicString("CustomStandard"));
  settings.UpdateFixed(AtomicString("CustomFixed"));
  settings.UpdateSerif(AtomicString("CustomSerif"));
  settings.UpdateSansSerif(AtomicString("CustomSansSerif"));
  settings.UpdateCursive(AtomicString("CustomCursive"));
  settings.UpdateFantasy(AtomicString("CustomFantasy"));
  settings.UpdateMath(AtomicString("CustomMath"));

  settings.UpdateStandard(AtomicString("CustomArabicStandard"), USCRIPT_ARABIC);
  settings.UpdateFixed(AtomicString("CustomArabicFixed"), USCRIPT_ARABIC);
  settings.UpdateSerif(AtomicString("CustomArabicSerif"), USCRIPT_ARABIC);
  settings.UpdateSansSerif(AtomicString("CustomArabicSansSerif"),
                           USCRIPT_ARABIC);
  settings.UpdateCursive(AtomicString("CustomArabicCursive"), USCRIPT_ARABIC);
  settings.UpdateFantasy(AtomicString("CustomArabicFantasy"), USCRIPT_ARABIC);
  settings.UpdateMath(AtomicString("CustomArabicMath"), USCRIPT_ARABIC);

  EXPECT_EQ("CustomStandard", settings.Standard());
  EXPECT_EQ("CustomFixed", settings.Fixed());
  EXPECT_EQ("CustomSerif", settings.Serif());
  EXPECT_EQ("CustomSansSerif", settings.SansSerif());
  EXPECT_EQ("CustomCursive", settings.Cursive());
  EXPECT_EQ("CustomFantasy", settings.Fantasy());
  EXPECT_EQ("CustomMath", settings.Math());

  EXPECT_EQ("CustomArabicStandard", settings.Standard(USCRIPT_ARABIC));
  EXPECT_EQ("CustomArabicFixed", settings.Fixed(USCRIPT_ARABIC));
  EXPECT_EQ("CustomArabicSerif", settings.Serif(USCRIPT_ARABIC));
  EXPECT_EQ("CustomArabicSansSerif", settings.SansSerif(USCRIPT_ARABIC));
  EXPECT_EQ("CustomArabicCursive", settings.Cursive(USCRIPT_ARABIC));
  EXPECT_EQ("CustomArabicFantasy", settings.Fantasy(USCRIPT_ARABIC));
  EXPECT_EQ("CustomArabicMath", settings.Math(USCRIPT_ARABIC));

  settings.Reset();

  EXPECT_TRUE(settings.Standard().empty());
  EXPECT_TRUE(settings.Fixed().empty());
  EXPECT_TRUE(settings.Serif().empty());
  EXPECT_TRUE(settings.SansSerif().empty());
  EXPECT_TRUE(settings.Cursive().empty());
  EXPECT_TRUE(settings.Fantasy().empty());
  EXPECT_TRUE(settings.Math().empty());

  EXPECT_TRUE(settings.Standard(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.Fixed(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.Serif(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.SansSerif(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.Cursive(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.Fantasy(USCRIPT_ARABIC).empty());
  EXPECT_TRUE(settings.Math(USCRIPT_ARABIC).empty());
}

}  // namespace blink
