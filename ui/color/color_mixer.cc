// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixer.h"

#include <utility>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"

namespace ui {

ColorMixer::ColorMixer(MixerGetter previous_mixer_getter,
                       MixerGetter input_mixer_getter)
    : previous_mixer_getter_(previous_mixer_getter),
      input_mixer_getter_(std::move(input_mixer_getter)) {}

ColorMixer::ColorMixer(ColorMixer&&) noexcept = default;

ColorMixer& ColorMixer::operator=(ColorMixer&&) noexcept = default;

ColorMixer::~ColorMixer() = default;

ColorRecipe& ColorMixer::operator[](ColorId id) {
  return recipes_[id];
}

SkColor ColorMixer::GetInputColor(ColorId id) const {
  const ColorMixer* previous_mixer =
      previous_mixer_getter_ ? previous_mixer_getter_.Run() : nullptr;
  // Don't log transitions to previous mixers unless the logging level is a
  // little higher.
  DVLOG_IF(3, previous_mixer)
      << "GetInputColor: ColorId " << ColorIdName(id) << " not found. "
      << "Checking previous mixer.";
  // If there's no previous mixer, always log color id misses.
  DVLOG_IF(2, !previous_mixer)
      << "GetInputColor: ColorId " << ColorIdName(id) << " not found. "
      << "Returning gfx::kPlaceholderColor.";
  return previous_mixer ? previous_mixer->GetResultColor(id)
                        : gfx::kPlaceholderColor;
}

SkColor ColorMixer::GetResultColor(ColorId id) const {
  const SkColor color = GetInputColor(id);
  const auto i = recipes_.find(id);
  const ColorMixer* const mixer =
      input_mixer_getter_ ? input_mixer_getter_.Run() : nullptr;
  return (i == recipes_.end())
             ? color
             : i->second.GenerateResult(color, *(mixer ? mixer : this));
}

std::set<ColorId> ColorMixer::GetDefinedColorIds() const {
  std::set<ColorId> color_ids;
  base::ranges::transform(recipes_, std::inserter(color_ids, color_ids.end()),
                          &std::pair<const ColorId, ColorRecipe>::first);

  return color_ids;
}

}  // namespace ui
