// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_view.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/wm/desks/desk_bar_controller.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/work_area_insets.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

// -----------------------------------------------------------------------------
// DeskBarView:

DeskBarView::DeskBarView(aura::Window* root)
    : DeskBarViewBase(root, DeskBarViewBase::Type::kDeskButton) {}

gfx::Size DeskBarView::CalculatePreferredSize() const {
  // For desk button bar, it comes with dynamic width. Thus, we calculate
  // the preferred width (summation of all child elements and paddings) and
  // use the full available width as the upper limit.
  // TODO(b/301663756): consolidate size calculation for the desk bar and its
  // scroll contents.
  int width = 0;
  for (auto* child : scroll_view_contents_->children()) {
    if (!child->GetVisible()) {
      continue;
    }
    if (width) {
      width += kDeskBarMiniViewsSpacing;
    }
    width += child->GetPreferredSize().width();
  }
  width += kDeskBarScrollViewMinimumHorizontalPaddingDeskButton * 2 +
           kDeskBarDeskPreviewViewFocusRingThicknessAndPadding * 2;
  width = std::min(width, GetAvailableBounds().width());

  return {width, GetPreferredBarHeight(root_, type_, state_)};
}

gfx::Rect DeskBarView::GetAvailableBounds() const {
  // Return all available bounds from the full usable workarea for desk bar
  // widget based on shelf alignment. Calculations for different shelf alignment
  // are as following.
  //
  // Symbols:
  //   - H: Home button
  //   - D: Desk button
  //   - S: Shelf
  //   - B: Bar widget
  //
  // Charts:
  //   1. `kBottom`
  //     ┌────────────────────────────────┐
  //     │                                │
  //     │                                │
  //     │                                │
  //     │                                │
  //     │                                │
  //     ├────────────────────────────────│
  //     │                B               │
  //     ├───┬─────┬──────────────────────┤
  //     │ H │  D  │           S          │
  //     └───┴─────┴──────────────────────┘
  //   2. `kLeft`
  //     ┌───┬────────────────────────────┐
  //     │ H │                            │
  //     ├───┤ ┌──────────────────────────┤
  //     │ D │ │             B            │
  //     ├───┤ │                          │
  //     │   │ └──────────────────────────┤
  //     │   │                            │
  //     │ S │                            │
  //     │   │                            │
  //     │   │                            │
  //     └───┴────────────────────────────┘
  //   3. `kRight`
  //     ┌────────────────────────────┬───┐
  //     │                            │ H │
  //     ├──────────────────────────┐ ├───┤
  //     │             B            │ │ D │
  //     │                          │ ├───┤
  //     ├──────────────────────────┘ │   │
  //     │                            │   │
  //     │                            │ S │
  //     │                            │   │
  //     │                            │   │
  //     └────────────────────────────┴───┘
  const gfx::Rect work_area =
      WorkAreaInsets::ForWindow(root_)->user_work_area_bounds();
  gfx::Size bar_size(work_area.width(),
                     DeskBarViewBase::GetPreferredBarHeight(
                         root_, DeskBarViewBase::Type::kDeskButton,
                         DeskBarViewBase::State::kExpanded));

  const Shelf* shelf = Shelf::ForWindow(root_);
  const gfx::Rect shelf_bounds = shelf->GetShelfBoundsInScreen();
  const int desk_button_y =
      shelf->desk_button_widget()->GetWindowBoundsInScreen().y();

  gfx::Point bar_origin;
  switch (shelf->alignment()) {
    case ShelfAlignment::kBottom:
      bar_origin.set_x(shelf_bounds.x() +
                       (work_area.width() - bar_size.width()) / 2);
      bar_origin.set_y(shelf_bounds.y() - kDeskBarShelfAndBarSpacing -
                       bar_size.height());
      break;
    case ShelfAlignment::kLeft:
      bar_size.set_width(bar_size.width() - kDeskBarShelfAndBarSpacing);
      bar_origin.set_x(shelf_bounds.right() + kDeskBarShelfAndBarSpacing);
      bar_origin.set_y(desk_button_y);
      break;
    case ShelfAlignment::kRight:
      bar_size.set_width(bar_size.width() - kDeskBarShelfAndBarSpacing);
      bar_origin.set_x(shelf_bounds.x() - kDeskBarShelfAndBarSpacing -
                       bar_size.width());
      bar_origin.set_y(desk_button_y);
      break;
    case ShelfAlignment::kBottomLocked:
      // TODO(yongshun): investigate why we can not add `NOTREACHED()` here.
      // If the bar is still visible while we shut down the system, there is a
      // chance that this could happen.
      bar_size = {0, 0};
      bar_origin = {0, 0};
      break;
  }

  return {bar_origin, bar_size};
}

void DeskBarView::UpdateBarBounds() {
  // Refresh bounds as preferred. This is needed for dynamic width for the
  // bar.
  const ShelfAlignment shelf_alignment = Shelf::ForWindow(root_)->alignment();
  gfx::Size preferred_size = CalculatePreferredSize();
  gfx::Rect new_bounds = GetAvailableBounds();
  switch (shelf_alignment) {
    case ShelfAlignment::kBottom:
      new_bounds.ClampToCenteredSize(preferred_size);
      break;
    case ShelfAlignment::kLeft:
      new_bounds.set_size(preferred_size);
      break;
    case ShelfAlignment::kRight:
      new_bounds.set_origin(
          {new_bounds.right() - preferred_size.width(), new_bounds.y()});
      new_bounds.set_size(preferred_size);
      break;
    case ShelfAlignment::kBottomLocked:
      break;
  }
  GetWidget()->SetBounds(new_bounds);
}

BEGIN_METADATA(DeskBarView, DeskBarViewBase)
END_METADATA

}  // namespace ash
