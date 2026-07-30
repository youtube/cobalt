// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/caption_style.h"

#include <string>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

constexpr wchar_t kCaptionRegPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\ClosedCaptioning";

// GUID of the built-in Windows "Default" closed caption theme.
constexpr wchar_t kDefaultThemeGuid[] =
    L"{642F4BD2-475F-4802-9B13-95261896CB1C}";

// GUID of a non-default built-in Windows caption theme (Yellow on Blue).
constexpr wchar_t kCustomThemeGuid[] =
    L"{DF834234-A0EF-4E2A-BB87-C40E5D1CFC8C}";

// Seeds the per-property registry values that the Windows.Media.
// ClosedCaptioning runtime API reads. Each ClosedCaption* enum uses 0 to mean
// "Default", so any non-zero DWORD here yields a non-Default enum value
// (e.g. White, Black, Tahoma, 100%). These specific numbers are not meaningful
// — the test only needs the API to report non-Default for each property.
void SeedCaptionPropertyRegistryValues() {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER, kCaptionRegPath, KEY_WRITE),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CaptionColor", static_cast<DWORD>(1)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CaptionOpacity", static_cast<DWORD>(1)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CaptionSize", static_cast<DWORD>(2)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CaptionFontStyle", static_cast<DWORD>(4)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CaptionEdgeEffect", static_cast<DWORD>(1)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"BackgroundColor", static_cast<DWORD>(2)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"BackgroundOpacity", static_cast<DWORD>(2)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"RegionColor", static_cast<DWORD>(2)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"RegionOpacity", static_cast<DWORD>(4)),
            ERROR_SUCCESS);
}

void SetCurrentSelectedTheme(const wchar_t* guid) {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER, kCaptionRegPath, KEY_WRITE),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CurrentSelectedTheme", guid), ERROR_SUCCESS);
}

class CaptionStyleWinTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Redirect HKCU to a scratch hive so the test never touches the real user
    // registry and is parallel-safe. The Windows.Media.ClosedCaptioning WinRT
    // API runs in-process and reads HKCU directly, so this redirect also
    // controls what the API observes.
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
  base::win::ScopedCOMInitializer com_initializer_;
};

}  // namespace

// When the Windows Default caption theme is selected, FromSystemSettings()
// should return a CaptionStyle whose populated fields do NOT contain
// !important. The OS-reported defaults serve as UA styles that HTML author
// ::cue rules can override.
TEST_F(CaptionStyleWinTest, TestWinCaptionStyleDefault) {
  ASSERT_NO_FATAL_FAILURE(SeedCaptionPropertyRegistryValues());
  ASSERT_NO_FATAL_FAILURE(SetCurrentSelectedTheme(kDefaultThemeGuid));

  std::optional<ui::CaptionStyle> caption_style =
      ui::CaptionStyle::FromSystemSettings();
  ASSERT_TRUE(caption_style.has_value());

  // No field may carry !important under the Default theme — author ::cue
  // styles must remain free to override the UA defaults.
  EXPECT_EQ(caption_style->background_color.find("!important"),
            std::string::npos)
      << "Value: " << caption_style->background_color;
  EXPECT_EQ(caption_style->font_family.find("!important"), std::string::npos)
      << "Value: " << caption_style->font_family;
  EXPECT_EQ(caption_style->font_variant.find("!important"), std::string::npos)
      << "Value: " << caption_style->font_variant;
  EXPECT_EQ(caption_style->text_color.find("!important"), std::string::npos)
      << "Value: " << caption_style->text_color;
  EXPECT_EQ(caption_style->text_shadow.find("!important"), std::string::npos)
      << "Value: " << caption_style->text_shadow;
  EXPECT_EQ(caption_style->text_size.find("!important"), std::string::npos)
      << "Value: " << caption_style->text_size;
  EXPECT_EQ(caption_style->window_color.find("!important"), std::string::npos)
      << "Value: " << caption_style->window_color;
}

// When a non-default caption theme is selected, FromSystemSettings() should
// return a CaptionStyle whose populated fields contain !important so the
// user's accessibility preference overrides author ::cue styles.
TEST_F(CaptionStyleWinTest, TestWinCaptionStyleNonDefault) {
  ASSERT_NO_FATAL_FAILURE(SeedCaptionPropertyRegistryValues());
  ASSERT_NO_FATAL_FAILURE(SetCurrentSelectedTheme(kCustomThemeGuid));

  std::optional<ui::CaptionStyle> caption_style =
      ui::CaptionStyle::FromSystemSettings();
  ASSERT_TRUE(caption_style.has_value());

  EXPECT_NE(caption_style->background_color.find("!important"),
            std::string::npos)
      << "Value: " << caption_style->background_color;
  EXPECT_NE(caption_style->font_family.find("!important"), std::string::npos)
      << "Value: " << caption_style->font_family;
  EXPECT_NE(caption_style->text_color.find("!important"), std::string::npos)
      << "Value: " << caption_style->text_color;
  EXPECT_NE(caption_style->text_shadow.find("!important"), std::string::npos)
      << "Value: " << caption_style->text_shadow;
  EXPECT_NE(caption_style->text_size.find("!important"), std::string::npos)
      << "Value: " << caption_style->text_size;
  EXPECT_NE(caption_style->window_color.find("!important"), std::string::npos)
      << "Value: " << caption_style->window_color;
}

}  // namespace ui
