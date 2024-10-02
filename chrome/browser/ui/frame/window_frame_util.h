// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FRAME_WINDOW_FRAME_UTIL_H_
#define CHROME_BROWSER_UI_FRAME_WINDOW_FRAME_UTIL_H_

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Size;
}

class Browser;

// Static-only class containing values and helper functions for frame classes
// that need to be accessible outside of /browser/ui/views.
class WindowFrameUtil {
 public:
  static constexpr int kWindows10GlassCaptionButtonWidth = 45;
  static constexpr int kWindows10GlassCaptionButtonHeightRestored = 29;
  static constexpr int kWindows10GlassCaptionButtonVisualSpacing = 1;

  WindowFrameUtil(const WindowFrameUtil&) = delete;
  WindowFrameUtil& operator=(const WindowFrameUtil&) = delete;

  // Returns the alpha that the Windows10CaptionButton should use to blend the
  // color provided by the theme in determining the button's 'base color'.
  static SkAlpha CalculateWindows10GlassCaptionButtonBackgroundAlpha(
      SkAlpha theme_alpha);

  // Returns the size of the area occupied by the caption buttons in the glass
  // browser frame view.
  static gfx::Size GetWindows10GlassCaptionButtonAreaSize();

  // Returns true if the windows 10 caption button is enabled.
  static bool IsWin10TabSearchCaptionButtonEnabled(const Browser* browser);

 private:
  WindowFrameUtil() {}
};

#endif  // CHROME_BROWSER_UI_FRAME_WINDOW_FRAME_UTIL_H_
