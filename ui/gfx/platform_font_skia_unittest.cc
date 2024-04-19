// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_skia.h"

#include <string>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_names_testing.h"
#include "ui/gfx/font_render_params.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/system_fonts_win.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/fake_linux_ui.h"
#endif

namespace gfx {

#if BUILDFLAG(IS_LINUX)
// Implementation of LinuxUi used to control the default font description.
class TestFontDelegate : public ui::FakeLinuxUi {
 public:
  TestFontDelegate() = default;

  TestFontDelegate(const TestFontDelegate&) = delete;
  TestFontDelegate& operator=(const TestFontDelegate&) = delete;

  ~TestFontDelegate() override = default;

  void set_family(const std::string& family) { family_ = family; }
  void set_size_pixels(int size_pixels) { size_pixels_ = size_pixels; }
  void set_style(int style) { style_ = style; }
  void set_weight(gfx::Font::Weight weight) { weight_ = weight; }
  void set_params(const FontRenderParams& params) { params_ = params; }

  FontRenderParams GetDefaultFontRenderParams() const override {
    NOTIMPLEMENTED();
    return FontRenderParams();
  }

  void GetDefaultFontDescription(std::string* family_out,
                                 int* size_pixels_out,
                                 int* style_out,
                                 int* weight_out,
                                 FontRenderParams* params_out) const override {
    *family_out = family_;
    *size_pixels_out = size_pixels_;
    *style_out = style_;
    *weight_out = static_cast<int>(weight_);
    *params_out = params_;
  }

 private:
  // Default values to be returned.
  std::string family_;
  int size_pixels_ = 0;
  int style_ = Font::NORMAL;
  gfx::Font::Weight weight_ = Font::Weight::NORMAL;
  FontRenderParams params_;
};

class PlatformFontSkiaTest : public testing::Test {
 public:
  PlatformFontSkiaTest() = default;

  PlatformFontSkiaTest(const PlatformFontSkiaTest&) = delete;
  PlatformFontSkiaTest& operator=(const PlatformFontSkiaTest&) = delete;

  ~PlatformFontSkiaTest() override = default;

  void SetUp() override {
    DCHECK_EQ(ui::LinuxUi::instance(), nullptr);
    old_linux_ui_ = ui::LinuxUi::SetInstance(&test_font_delegate_);
    PlatformFontSkia::ReloadDefaultFont();
  }

  void TearDown() override {
    DCHECK_EQ(&test_font_delegate_, ui::LinuxUi::instance());
    ui::LinuxUi::SetInstance(old_linux_ui_);
    PlatformFontSkia::ReloadDefaultFont();
  }

 protected:
  TestFontDelegate test_font_delegate_;
  raw_ptr<ui::LinuxUi> old_linux_ui_ = nullptr;
};

// Test that PlatformFontSkia's default constructor initializes the instance
// with the correct parameters.
TEST_F(PlatformFontSkiaTest, DefaultFont) {
  test_font_delegate_.set_family(kTestFontName);
  test_font_delegate_.set_size_pixels(13);
  test_font_delegate_.set_style(Font::NORMAL);
  FontRenderParams params;
  params.antialiasing = false;
  params.hinting = FontRenderParams::HINTING_FULL;
  test_font_delegate_.set_params(params);
  scoped_refptr<gfx::PlatformFontSkia> font(new gfx::PlatformFontSkia());
  EXPECT_EQ(kTestFontName, font->GetFontName());
  EXPECT_EQ(13, font->GetFontSize());
  EXPECT_EQ(gfx::Font::NORMAL, font->GetStyle());

  EXPECT_EQ(params.antialiasing, font->GetFontRenderParams().antialiasing);
  EXPECT_EQ(params.hinting, font->GetFontRenderParams().hinting);

  // Drop the old default font and check that new settings are loaded.
  test_font_delegate_.set_family(kSymbolFontName);
  test_font_delegate_.set_size_pixels(15);
  test_font_delegate_.set_style(gfx::Font::ITALIC);
  test_font_delegate_.set_weight(gfx::Font::Weight::BOLD);
  PlatformFontSkia::ReloadDefaultFont();
  scoped_refptr<gfx::PlatformFontSkia> font2(new gfx::PlatformFontSkia());
  EXPECT_EQ(kSymbolFontName, font2->GetFontName());
  EXPECT_EQ(15, font2->GetFontSize());
  EXPECT_NE(font2->GetStyle() & Font::ITALIC, 0);
  EXPECT_EQ(gfx::Font::Weight::BOLD, font2->GetWeight());
}
#endif  // BUILDFLAG(IS_LINUX)

TEST(PlatformFontSkiaRenderParamsTest, DefaultFontRenderParams) {
  scoped_refptr<PlatformFontSkia> default_font(new PlatformFontSkia());
  scoped_refptr<PlatformFontSkia> named_font(new PlatformFontSkia(
      default_font->GetFontName(), default_font->GetFontSize()));

  // Ensures that both constructors are producing fonts with the same render
  // params.
  EXPECT_EQ(default_font->GetFontRenderParams(),
            named_font->GetFontRenderParams());
}

#if BUILDFLAG(IS_WIN)
TEST(PlatformFontSkiaOnWindowsTest, SystemFont) {
  // Ensures that the font styles are kept while creating the default font.
  gfx::Font system_font = win::GetDefaultSystemFont();
  gfx::Font default_font;

  EXPECT_EQ(system_font.GetFontName(), default_font.GetFontName());
  EXPECT_EQ(system_font.GetFontSize(), default_font.GetFontSize());
  EXPECT_EQ(system_font.GetStyle(), default_font.GetStyle());
  EXPECT_EQ(system_font.GetWeight(), default_font.GetWeight());
  EXPECT_EQ(system_font.GetHeight(), default_font.GetHeight());
  EXPECT_EQ(system_font.GetBaseline(), default_font.GetBaseline());
  EXPECT_EQ(system_font.GetBaseline(), default_font.GetBaseline());
  EXPECT_EQ(system_font.GetFontRenderParams(),
            default_font.GetFontRenderParams());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace gfx
