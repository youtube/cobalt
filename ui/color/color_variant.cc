// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_variant.h"

#include <optional>

#include "base/check.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"

namespace ui {

ColorVariant::ColorVariant() = default;

ColorVariant::ColorVariant(SkColor color) : color_variant_(color) {}
ColorVariant::ColorVariant(ColorId color_id) : color_variant_(color_id) {}

ColorVariant::~ColorVariant() = default;

std::optional<ColorId> ColorVariant::GetColorId() const {
  return absl::holds_alternative<ColorId>(color_variant_)
             ? std::make_optional(absl::get<ColorId>(color_variant_))
             : std::nullopt;
}

std::optional<SkColor> ColorVariant::GetSkColor() const {
  return absl::holds_alternative<SkColor>(color_variant_)
             ? std::make_optional(absl::get<SkColor>(color_variant_))
             : std::nullopt;
}

SkColor ColorVariant::ConvertToSkColor(
    const ColorProvider* color_provider) const {
  CHECK(color_provider);

  if (auto color = GetSkColor()) {
    return color.value();
  }

  return color_provider->GetColor(GetColorId().value());
}

}  // namespace ui
