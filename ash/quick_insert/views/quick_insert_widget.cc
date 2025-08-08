// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_widget.h"

#include "ash/bubble/bubble_event_filter.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/quick_insert/metrics/quick_insert_session_metrics.h"
#include "ash/quick_insert/views/quick_insert_positioning.h"
#include "ash/quick_insert/views/quick_insert_style.h"
#include "ash/quick_insert/views/quick_insert_view.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// This is the maximum size of QuickInsertView, including emoji bar.
constexpr int kQuickInsertViewMaxHeight = 356;

// Gets the preferred layout to use given `anchor_bounds` in screen coordinates.
PickerLayoutType GetLayoutType(const gfx::Rect& anchor_bounds,
                               PickerPositionType position_type) {
  return position_type == PickerPositionType::kCentered ||
                 anchor_bounds.bottom() + kQuickInsertViewMaxHeight <=
                     display::Screen::GetScreen()
                         ->GetDisplayMatching(anchor_bounds)
                         .work_area()
                         .bottom()
             ? PickerLayoutType::kMainResultsBelowSearchField
             : PickerLayoutType::kMainResultsAboveSearchField;
}

views::Widget::InitParams CreateInitParams(
    QuickInsertViewDelegate* delegate,
    const gfx::Rect& anchor_bounds,
    PickerPositionType position_type,
    const base::TimeTicks trigger_event_timestamp) {
  auto quick_insert_view = std::make_unique<QuickInsertView>(
      delegate, anchor_bounds, GetLayoutType(anchor_bounds, position_type),
      position_type, trigger_event_timestamp);

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_BUBBLE);
  params.parent = Shell::GetContainer(
      Shell::GetRootWindowForDisplayId(
          display::Screen::GetScreen()->GetDisplayMatching(anchor_bounds).id()),
      kShellWindowId_FloatContainer);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  // TODO(b/309706053): Replace this with the finalized string.
  params.name = "Picker";
  params.delegate = quick_insert_view.release();
  return params;
}

}  // namespace

views::UniqueWidgetPtr QuickInsertWidget::Create(
    QuickInsertViewDelegate* delegate,
    const gfx::Rect& anchor_bounds,
    base::TimeTicks trigger_event_timestamp) {
  return base::WrapUnique(new QuickInsertWidget(delegate, anchor_bounds,
                                                PickerPositionType::kNearAnchor,
                                                trigger_event_timestamp));
}

views::UniqueWidgetPtr QuickInsertWidget::CreateCentered(
    QuickInsertViewDelegate* delegate,
    const gfx::Rect& anchor_bounds,
    base::TimeTicks trigger_event_timestamp) {
  return base::WrapUnique(new QuickInsertWidget(delegate, anchor_bounds,
                                                PickerPositionType::kCentered,
                                                trigger_event_timestamp));
}

void QuickInsertWidget::OnNativeBlur() {
  SetVisibilityAnimationTransition(
      views::Widget::VisibilityTransition::ANIMATE_NONE);
  if (delegate_ != nullptr) {
    delegate_->GetSessionMetrics().SetOutcome(
        PickerSessionMetrics::SessionOutcome::kAbandoned);
  }
  Close();
}

QuickInsertWidget::QuickInsertWidget(QuickInsertViewDelegate* delegate,
                                     const gfx::Rect& anchor_bounds,
                                     PickerPositionType position_type,
                                     base::TimeTicks trigger_event_timestamp)
    : views::Widget(CreateInitParams(delegate,
                                     anchor_bounds,
                                     position_type,
                                     trigger_event_timestamp)),
      bubble_event_filter_(/*widget=*/this),
      delegate_(delegate) {
  SetVisibilityAnimationTransition(
      views::Widget::VisibilityTransition::ANIMATE_HIDE);
}

QuickInsertWidget::~QuickInsertWidget() = default;

}  // namespace ash
