// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_TAB_STRIP_COLOR_MIXER_H_
#define CHROME_BROWSER_UI_COLOR_TAB_STRIP_COLOR_MIXER_H_

#include "ui/color/color_provider_manager.h"

namespace ui {
class ColorProvider;
}

// Adds a color mixer that contains recipes for tab strip colors to |provider|
// with |key|.
void AddTabStripColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderManager::Key& key);

#endif  // CHROME_BROWSER_UI_COLOR_TAB_STRIP_COLOR_MIXER_H_
