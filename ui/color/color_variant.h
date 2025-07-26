// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_VARIANT_H_
#define UI_COLOR_COLOR_VARIANT_H_

#include <optional>

#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"

namespace ui {
class ColorProvider;

// ColorVariant represents a color that can be stored either as an SkColor
// or a ColorId.  This allows for flexibility in how colors are handled,
// enabling the use of theme-aware ColorIds where appropriate, while still
// supporting direct SkColor values e.g. runtime-specified overrides, such
// as webpage-provided colors.
class COMPONENT_EXPORT(COLOR) ColorVariant {
 public:
  ColorVariant();

  // NOLINTBEGIN(google-explicit-constructor)
  ColorVariant(SkColor color);
  ColorVariant(ColorId color_id);
  // NOLINTEND(google-explicit-constructor)

  ColorVariant(const ColorVariant& other) = default;
  ColorVariant& operator=(const ColorVariant& other) = default;

  ColorVariant(ColorVariant&& other) = default;
  ColorVariant& operator=(ColorVariant&& other) = default;

  ~ColorVariant();

  ColorVariant& operator=(const SkColor& other) {
    color_variant_ = other;
    return *this;
  }
  ColorVariant& operator=(const ColorId& other) {
    color_variant_ = other;
    return *this;
  }

  bool operator==(const ColorVariant& other) const = default;
  bool operator==(const SkColor& other) const { return GetSkColor() == other; }
  bool operator==(const ColorId& other) const { return GetColorId() == other; }

  std::optional<ColorId> GetColorId() const;
  std::optional<SkColor> GetSkColor() const;

  // Resolves the ColorVariant to an SkColor.  If the ColorVariant holds a
  // ColorId, it will be resolved to an SkColor using the provided
  // ColorProvider.
  SkColor ConvertToSkColor(const ui::ColorProvider* color_provider) const;

 private:
  absl::variant<ColorId, SkColor> color_variant_ = gfx::kPlaceholderColor;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_VARIANT_H_
