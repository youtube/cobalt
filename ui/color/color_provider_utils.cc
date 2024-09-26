// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_utils.h"

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace ui {
namespace {

using RendererColorId = color::mojom::RendererColorId;

// Below defines the mapping between RendererColorIds and ColorIds.
struct RendererColorIdTable {
  RendererColorId renderer_color_id;
  ColorId color_id;
};
constexpr RendererColorIdTable kRendererColorIdMap[] = {
    {RendererColorId::kColorMenuBackground, kColorMenuBackground},
    {RendererColorId::kColorMenuItemBackgroundSelected,
     kColorMenuItemBackgroundSelected},
    {RendererColorId::kColorMenuSeparator, kColorMenuSeparator},
    {RendererColorId::kColorOverlayScrollbarFill, kColorOverlayScrollbarFill},
    {RendererColorId::kColorOverlayScrollbarFillDark,
     kColorOverlayScrollbarFillDark},
    {RendererColorId::kColorOverlayScrollbarFillLight,
     kColorOverlayScrollbarFillLight},
    {RendererColorId::kColorOverlayScrollbarFillHovered,
     kColorOverlayScrollbarFillHovered},
    {RendererColorId::kColorOverlayScrollbarFillHoveredDark,
     kColorOverlayScrollbarFillHoveredDark},
    {RendererColorId::kColorOverlayScrollbarFillHoveredLight,
     kColorOverlayScrollbarFillHoveredLight},
    {RendererColorId::kColorOverlayScrollbarStroke,
     kColorOverlayScrollbarStroke},
    {RendererColorId::kColorOverlayScrollbarStrokeDark,
     kColorOverlayScrollbarStrokeDark},
    {RendererColorId::kColorOverlayScrollbarStrokeLight,
     kColorOverlayScrollbarStrokeLight},
    {RendererColorId::kColorOverlayScrollbarStrokeHovered,
     kColorOverlayScrollbarStrokeHovered},
    {RendererColorId::kColorOverlayScrollbarStrokeHoveredDark,
     kColorOverlayScrollbarStrokeHoveredDark},
    {RendererColorId::kColorOverlayScrollbarStrokeHoveredLight,
     kColorOverlayScrollbarStrokeHoveredLight},
    {RendererColorId::kColorScrollbarArrowBackgroundHovered,
     kColorScrollbarArrowBackgroundHovered},
    {RendererColorId::kColorScrollbarArrowBackgroundPressed,
     kColorScrollbarArrowBackgroundPressed},
    {RendererColorId::kColorScrollbarArrowForeground,
     kColorScrollbarArrowForeground},
    {RendererColorId::kColorScrollbarArrowForegroundPressed,
     kColorScrollbarArrowForegroundPressed},
    {RendererColorId::kColorScrollbarCorner, kColorScrollbarCorner},
    {RendererColorId::kColorScrollbarThumb, kColorScrollbarThumb},
    {RendererColorId::kColorScrollbarThumbHovered, kColorScrollbarThumbHovered},
    {RendererColorId::kColorScrollbarThumbInactive,
     kColorScrollbarThumbInactive},
    {RendererColorId::kColorScrollbarThumbPressed, kColorScrollbarThumbPressed},
    {RendererColorId::kColorScrollbarTrack, kColorScrollbarTrack},
};

ColorProviderUtilsCallbacks* g_color_provider_utils_callbacks = nullptr;

}  // namespace

ColorProviderUtilsCallbacks::~ColorProviderUtilsCallbacks() = default;

base::StringPiece ColorModeName(ColorProviderManager::ColorMode color_mode) {
  switch (color_mode) {
    case ColorProviderManager::ColorMode::kLight:
      return "kLight";
    case ColorProviderManager::ColorMode::kDark:
      return "kDark";
    default:
      return "<invalid>";
  }
}

base::StringPiece ContrastModeName(
    ColorProviderManager::ContrastMode contrast_mode) {
  switch (contrast_mode) {
    case ColorProviderManager::ContrastMode::kNormal:
      return "kNormal";
    case ColorProviderManager::ContrastMode::kHigh:
      return "kHigh";
    default:
      return "<invalid>";
  }
}

base::StringPiece SystemThemeName(ui::SystemTheme system_theme) {
  switch (system_theme) {
    case ui::SystemTheme::kDefault:
      return "kDefault";
#if BUILDFLAG(IS_LINUX)
    case ui::SystemTheme::kGtk:
      return "kGtk";
    case ui::SystemTheme::kQt:
      return "kQt";
#endif
    default:
      return "<invalid>";
  }
}

#include "ui/color/color_id_map_macros.inc"

std::string ColorIdName(ColorId color_id) {
  static constexpr const auto color_id_map =
      base::MakeFixedFlatMap<ColorId, const char*>({COLOR_IDS});
  auto* i = color_id_map.find(color_id);
  if (i != color_id_map.cend())
    return {i->second};
  base::StringPiece color_name;
  if (g_color_provider_utils_callbacks &&
      g_color_provider_utils_callbacks->ColorIdName(color_id, &color_name))
    return std::string(color_name.data(), color_name.length());
  return base::StringPrintf("ColorId(%d)", color_id);
}

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/color/color_id_map_macros.inc"  // NOLINT(build/include)

std::string SkColorName(SkColor color) {
  static const auto color_name_map =
      base::MakeFixedFlatMap<SkColor, const char*>({
          {gfx::kGoogleBlue050, "kGoogleBlue050"},
          {gfx::kGoogleBlue100, "kGoogleBlue100"},
          {gfx::kGoogleBlue200, "kGoogleBlue200"},
          {gfx::kGoogleBlue300, "kGoogleBlue300"},
          {gfx::kGoogleBlue400, "kGoogleBlue400"},
          {gfx::kGoogleBlue500, "kGoogleBlue500"},
          {gfx::kGoogleBlue600, "kGoogleBlue600"},
          {gfx::kGoogleBlue700, "kGoogleBlue700"},
          {gfx::kGoogleBlue800, "kGoogleBlue800"},
          {gfx::kGoogleBlue900, "kGoogleBlue900"},
          {gfx::kGoogleRed050, "kGoogleRed050"},
          {gfx::kGoogleRed100, "kGoogleRed100"},
          {gfx::kGoogleRed200, "kGoogleRed200"},
          {gfx::kGoogleRed300, "kGoogleRed300"},
          {gfx::kGoogleRed400, "kGoogleRed400"},
          {gfx::kGoogleRed500, "kGoogleRed500"},
          {gfx::kGoogleRed600, "kGoogleRed600"},
          {gfx::kGoogleRed700, "kGoogleRed700"},
          {gfx::kGoogleRed800, "kGoogleRed800"},
          {gfx::kGoogleRed900, "kGoogleRed900"},
          {gfx::kGoogleGreen050, "kGoogleGreen050"},
          {gfx::kGoogleGreen100, "kGoogleGreen100"},
          {gfx::kGoogleGreen200, "kGoogleGreen200"},
          {gfx::kGoogleGreen300, "kGoogleGreen300"},
          {gfx::kGoogleGreen400, "kGoogleGreen400"},
          {gfx::kGoogleGreen500, "kGoogleGreen500"},
          {gfx::kGoogleGreen600, "kGoogleGreen600"},
          {gfx::kGoogleGreen700, "kGoogleGreen700"},
          {gfx::kGoogleGreen800, "kGoogleGreen800"},
          {gfx::kGoogleGreen900, "kGoogleGreen900"},
          {gfx::kGoogleYellow050, "kGoogleYellow050"},
          {gfx::kGoogleYellow100, "kGoogleYellow100"},
          {gfx::kGoogleYellow200, "kGoogleYellow200"},
          {gfx::kGoogleYellow300, "kGoogleYellow300"},
          {gfx::kGoogleYellow400, "kGoogleYellow400"},
          {gfx::kGoogleYellow500, "kGoogleYellow500"},
          {gfx::kGoogleYellow600, "kGoogleYellow600"},
          {gfx::kGoogleYellow700, "kGoogleYellow700"},
          {gfx::kGoogleYellow800, "kGoogleYellow800"},
          {gfx::kGoogleYellow900, "kGoogleYellow900"},
          {gfx::kGoogleGrey050, "kGoogleGrey050"},
          {gfx::kGoogleGrey100, "kGoogleGrey100"},
          {gfx::kGoogleGrey200, "kGoogleGrey200"},
          {gfx::kGoogleGrey300, "kGoogleGrey300"},
          {gfx::kGoogleGrey400, "kGoogleGrey400"},
          {gfx::kGoogleGrey500, "kGoogleGrey500"},
          {gfx::kGoogleGrey600, "kGoogleGrey600"},
          {gfx::kGoogleGrey700, "kGoogleGrey700"},
          {gfx::kGoogleGrey800, "kGoogleGrey800"},
          {gfx::kGoogleGrey900, "kGoogleGrey900"},
          {gfx::kGoogleOrange050, "kGoogleOrange050"},
          {gfx::kGoogleOrange100, "kGoogleOrange100"},
          {gfx::kGoogleOrange200, "kGoogleOrange200"},
          {gfx::kGoogleOrange300, "kGoogleOrange300"},
          {gfx::kGoogleOrange400, "kGoogleOrange400"},
          {gfx::kGoogleOrange500, "kGoogleOrange500"},
          {gfx::kGoogleOrange600, "kGoogleOrange600"},
          {gfx::kGoogleOrange700, "kGoogleOrange700"},
          {gfx::kGoogleOrange800, "kGoogleOrange800"},
          {gfx::kGoogleOrange900, "kGoogleOrange900"},
          {gfx::kGooglePink050, "kGooglePink050"},
          {gfx::kGooglePink100, "kGooglePink100"},
          {gfx::kGooglePink200, "kGooglePink200"},
          {gfx::kGooglePink300, "kGooglePink300"},
          {gfx::kGooglePink400, "kGooglePink400"},
          {gfx::kGooglePink500, "kGooglePink500"},
          {gfx::kGooglePink600, "kGooglePink600"},
          {gfx::kGooglePink700, "kGooglePink700"},
          {gfx::kGooglePink800, "kGooglePink800"},
          {gfx::kGooglePink900, "kGooglePink900"},
          {gfx::kGooglePurple050, "kGooglePurple050"},
          {gfx::kGooglePurple100, "kGooglePurple100"},
          {gfx::kGooglePurple200, "kGooglePurple200"},
          {gfx::kGooglePurple300, "kGooglePurple300"},
          {gfx::kGooglePurple400, "kGooglePurple400"},
          {gfx::kGooglePurple500, "kGooglePurple500"},
          {gfx::kGooglePurple600, "kGooglePurple600"},
          {gfx::kGooglePurple700, "kGooglePurple700"},
          {gfx::kGooglePurple800, "kGooglePurple800"},
          {gfx::kGooglePurple900, "kGooglePurple900"},
          {gfx::kGoogleCyan050, "kGoogleCyan050"},
          {gfx::kGoogleCyan100, "kGoogleCyan100"},
          {gfx::kGoogleCyan200, "kGoogleCyan200"},
          {gfx::kGoogleCyan300, "kGoogleCyan300"},
          {gfx::kGoogleCyan400, "kGoogleCyan400"},
          {gfx::kGoogleCyan500, "kGoogleCyan500"},
          {gfx::kGoogleCyan600, "kGoogleCyan600"},
          {gfx::kGoogleCyan700, "kGoogleCyan700"},
          {gfx::kGoogleCyan800, "kGoogleCyan800"},
          {gfx::kGoogleCyan900, "kGoogleCyan900"},
          {SK_ColorTRANSPARENT, "SK_ColorTRANSPARENT"},
          {SK_ColorBLACK, "SK_ColorBLACK"},
          {SK_ColorDKGRAY, "SK_ColorDKGRAY"},
          {SK_ColorGRAY, "SK_ColorGRAY"},
          {SK_ColorLTGRAY, "SK_ColorLTGRAY"},
          {SK_ColorWHITE, "SK_ColorWHITE"},
          {SK_ColorRED, "kPlaceholderColor"},
          {SK_ColorGREEN, "SK_ColorGREEN"},
          {SK_ColorBLUE, "SK_ColorBLUE"},
          {SK_ColorYELLOW, "SK_ColorYELLOW"},
          {SK_ColorCYAN, "SK_ColorCYAN"},
          {SK_ColorMAGENTA, "SK_ColorMAGENTA"},
      });
  auto color_with_alpha = color;
  SkAlpha color_alpha = SkColorGetA(color_with_alpha);
  color = SkColorSetA(color, color_alpha != 0 ? SK_AlphaOPAQUE : color_alpha);
  auto* i = color_name_map.find(color);
  if (i != color_name_map.cend()) {
    if (SkColorGetA(color_with_alpha) == SkColorGetA(color))
      return i->second;
    return base::StringPrintf("rgba(%s, %f)", i->second, 1.0 / color_alpha);
  }
  return color_utils::SkColorToRgbaString(color);
}

std::string ConvertColorProviderColorIdToCSSColorId(std::string color_id_name) {
  color_id_name.replace(color_id_name.begin(), color_id_name.begin() + 1, "-");
  std::string css_color_id_name;
  for (char i : color_id_name) {
    if (base::IsAsciiUpper(i))
      css_color_id_name += std::string("-");
    css_color_id_name += base::ToLowerASCII(i);
  }
  return css_color_id_name;
}

std::string ConvertSkColorToCSSColor(SkColor color) {
  return base::StringPrintf("#%.2x%.2x%.2x%.2x", SkColorGetR(color),
                            SkColorGetG(color), SkColorGetB(color),
                            SkColorGetA(color));
}

RendererColorMap CreateRendererColorMap(const ColorProvider& color_provider) {
  RendererColorMap map;
  for (const auto& table : kRendererColorIdMap) {
    map.emplace(table.renderer_color_id,
                color_provider.GetColor(table.color_id));
  }
  return map;
}

ColorProvider CreateColorProviderFromRendererColorMap(
    const RendererColorMap& renderer_color_map) {
  ColorProvider color_provider;
  ui::ColorMixer& mixer = color_provider.AddMixer();

  for (const auto& table : kRendererColorIdMap)
    mixer[table.color_id] = {renderer_color_map.at(table.renderer_color_id)};
  color_provider.GenerateColorMap();

  return color_provider;
}

bool IsRendererColorMappingEquivalent(
    const ColorProvider& color_provider,
    const RendererColorMap& renderer_color_map) {
  for (const auto& table : kRendererColorIdMap) {
    // The `renderer_color_map_` should map the full set of renderer color ids.
    DCHECK(base::Contains(renderer_color_map, table.renderer_color_id));
    if (color_provider.GetColor(table.color_id) !=
        renderer_color_map.at(table.renderer_color_id)) {
      return false;
    }
  }
  return true;
}

void SetColorProviderUtilsCallbacks(ColorProviderUtilsCallbacks* callbacks) {
  g_color_provider_utils_callbacks = callbacks;
}

}  // namespace ui
