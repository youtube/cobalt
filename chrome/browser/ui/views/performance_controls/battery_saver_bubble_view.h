// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_VIEW_H_

#include <optional>

#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_border.h"

class BatterySaverBubbleObserver;

namespace views {
class BubbleDialogModelHost;
}  // namespace views

// This class provides the view for the bubble dialog that is shown to the user
// when the battery saver toolbar button is clicked.
class BatterySaverBubbleView {
 public:
  // Creates the battery saver bubble dialog anchored to the specified view.
  static views::BubbleDialogModelHost* CreateBubble(
      views::BubbleAnchor anchor,
      views::BubbleBorder::Arrow anchor_position,
      BatterySaverBubbleObserver* observer,
      std::optional<gfx::Rect> anchor_rect = std::nullopt);

  // Hides the battery saver bubble dialog.
  static void CloseBubble(views::BubbleDialogModelHost*);

  static const char kViewClassName[];
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_VIEW_H_
