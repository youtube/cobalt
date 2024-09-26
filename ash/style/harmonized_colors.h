// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_HARMONIZED_COLORS_H_
#define ASH_STYLE_HARMONIZED_COLORS_H_

#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_manager.h"

namespace ash {

// Adds the harmonized reference colors to `mixer` based on the seed color in
// `key`. If a seed color is not specified, an arbitrary set of harmonized
// colors are used.
void AddHarmonizedColors(ui::ColorMixer& mixer,
                         const ui::ColorProviderManager::Key& key);

}  // namespace ash

#endif  // ASH_STYLE_HARMONIZED_COLORS_H_
