// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_POSITIONING_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_POSITIONING_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ash {

// How Quick Insert should try to position itself relative to its anchor.
enum class QuickInsertPositionType {
  // Near the anchor bounds.
  kNearAnchor,
  // Centered on the display containing the anchor bounds.
  kCentered,
};

// Gets the anchor bounds to use for positioning Quick Insert. We prefer to
// anchor at `caret_bounds`, but may use `cursor_point` as a fallback.
// `caret_bounds`, `cursor_point`, `focused_window_bounds` and returned anchor
// bounds should be in screen coordinates.
gfx::Rect ASH_EXPORT
GetQuickInsertAnchorBounds(const gfx::Rect& caret_bounds,
                           const gfx::Point& cursor_point,
                           const gfx::Rect& focused_window_bounds);

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_POSITIONING_H_
