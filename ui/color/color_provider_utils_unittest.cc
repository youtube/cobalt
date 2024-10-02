// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_utils.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

using ColorProviderUtilsTest = ::testing::Test;

TEST_F(ColorProviderUtilsTest, ConvertColorProviderColorIdToCSSColorId) {
  EXPECT_EQ(std::string("--color-primary-background"),
            ui::ConvertColorProviderColorIdToCSSColorId(
                std::string(ui::ColorIdName(ui::kColorPrimaryBackground))));
}

TEST_F(ColorProviderUtilsTest, ConvertSkColorToCSSColor) {
  SkColor test_color = SkColorSetRGB(0xF2, 0x99, 0x00);
  // This will fail if we don't make sure to show two hex digits per color.
  EXPECT_EQ(std::string("#f29900ff"), ui::ConvertSkColorToCSSColor(test_color));
  SkColor test_color_alpha = SkColorSetA(test_color, 0x25);
  EXPECT_EQ(std::string("#f2990025"),
            ui::ConvertSkColorToCSSColor(test_color_alpha));
}

TEST_F(ColorProviderUtilsTest, RendererColorMapGeneratesProvidersCorrectly) {
  // Total number of RendererColorIds is 1 greater than the max since ids start
  // at 0.
  constexpr uint32_t kTotaltRendererColorIds =
      static_cast<int32_t>(color::mojom::RendererColorId::kMaxValue) + 1;

  // The total number of RendererColorIds should be a subset of ui ColorIds. The
  // total number of ui ColorIds is the exclusive range of ids between
  // kUiColorsStart and kUiColorsEnd. This should be a positive non-zero value.
  ASSERT_LT(ui::kUiColorsStart, ui::kUiColorsEnd);
  ASSERT_LT(kTotaltRendererColorIds,
            static_cast<uint32_t>(ui::kUiColorsEnd - ui::kUiColorsStart - 1));

  // Generate the entire defined range of ui ColorIds. Do this so that we can
  // assert that only the subset of ColorIds specified by the RendererColorIds
  // enum is generated in the resulting RendererColorMap.
  ui::ColorProvider color_provider;
  ui::ColorMixer& mixer = color_provider.AddMixer();
  for (int i = ui::kUiColorsStart + 1; i < ui::kUiColorsEnd; ++i)
    mixer[i] = {static_cast<SkColor>(i)};
  color_provider.GenerateColorMap();

  // The size of the RendererColorMap should match number of defined
  // RendererColorIds.
  ui::RendererColorMap renderer_color_map =
      ui::CreateRendererColorMap(color_provider);
  EXPECT_EQ(kTotaltRendererColorIds, renderer_color_map.size());

  // The size of the ColorMap of the ColorProvider created from this map should
  // also match the number of defined RendererColorIds.
  auto new_color_provider =
      ui::CreateColorProviderFromRendererColorMap(renderer_color_map);
  EXPECT_EQ(kTotaltRendererColorIds,
            new_color_provider.color_map_for_testing().size());
}

TEST_F(ColorProviderUtilsTest, ColorProviderRendererColorMapEquivalence) {
  // Generate the entire defined range of ui ColorIds, which includes the
  // subset of renderer color ids.
  ui::ColorProvider color_provider;
  ui::ColorMixer& mixer = color_provider.AddMixer();
  for (int i = ui::kUiColorsStart + 1; i < ui::kUiColorsEnd; ++i) {
    mixer[i] = {static_cast<SkColor>(i)};
  }
  color_provider.GenerateColorMap();

  // A renderer color map generated from its source provider should have
  // equivalent mappings.
  ui::RendererColorMap renderer_color_map =
      ui::CreateRendererColorMap(color_provider);
  EXPECT_TRUE(
      IsRendererColorMappingEquivalent(color_provider, renderer_color_map));

  // Providers with different renderer color mappings should not be flagged as
  // equivalent.
  ui::ColorProvider new_color_provider;
  new_color_provider.GenerateColorMap();
  EXPECT_FALSE(
      IsRendererColorMappingEquivalent(new_color_provider, renderer_color_map));
}
