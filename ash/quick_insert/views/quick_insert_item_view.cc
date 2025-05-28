// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_item_view.h"

#include <memory>
#include <utility>

#include "ash/quick_insert/views/quick_insert_focus_indicator.h"
#include "ash/quick_insert/views/quick_insert_submenu_controller.h"
#include "ash/style/style_util.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

// How much to clip the focused QuickInsertItemView by. Inset by at least the
// default focus ring inset so the clipping is actually visible, then clip by
// the actual desired amount. It would be better to absolute value the halo
// inset here, but to keep this constexpr, also multiply by -1 to keep it
// positive.
constexpr float kPseudoFocusClipInset =
    views::FocusRing::kDefaultHaloInset * -1.0f + 2.0f;

constexpr auto kQuickInsertItemFocusIndicatorMargins = gfx::Insets::VH(6, 0);

}  // namespace

QuickInsertItemView::QuickInsertItemView(
    SelectItemCallback select_item_callback,
    FocusIndicatorStyle focus_indicator_style)
    : views::Button(select_item_callback),
      select_item_callback_(select_item_callback),
      focus_indicator_style_(focus_indicator_style) {
  switch (focus_indicator_style_) {
    case FocusIndicatorStyle::kFocusRingWithInsetGap:
      [[fallthrough]];
    case FocusIndicatorStyle::kFocusRing:
      StyleUtil::SetUpFocusRingForView(this);
      views::FocusRing::Get(this)->SetHasFocusPredicate(
          base::BindRepeating([](const View* view) {
            const auto* v = views::AsViewClass<QuickInsertItemView>(view);
            CHECK(v);
            return (v->HasFocus() ||
                    v->GetItemState() ==
                        QuickInsertItemView::ItemState::kPseudoFocused);
          }));
      break;
    case FocusIndicatorStyle::kFocusBar:
      // Disable default focus ring to use a custom focus indicator.
      SetInstallFocusRingOnFocus(false);
      break;
  }
}

QuickInsertItemView::~QuickInsertItemView() = default;

void QuickInsertItemView::StateChanged(ButtonState old_state) {
  UpdateBackground();
}

void QuickInsertItemView::PaintButtonContents(gfx::Canvas* canvas) {
  views::Button::PaintButtonContents(canvas);

  if (focus_indicator_style_ == FocusIndicatorStyle::kFocusBar &&
      item_state_ == ItemState::kPseudoFocused) {
    PaintQuickInsertFocusIndicator(
        canvas, gfx::Point(0, kQuickInsertItemFocusIndicatorMargins.top()),
        height() - kQuickInsertItemFocusIndicatorMargins.height(),
        GetColorProvider()->GetColor(cros_tokens::kCrosSysFocusRing));
  }
}

void QuickInsertItemView::SelectItem() {
  select_item_callback_.Run();
}

void QuickInsertItemView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateClipPathForFocusRingWithInsetGap();
}

void QuickInsertItemView::OnMouseEntered(const ui::MouseEvent& event) {
  if (submenu_controller_ != nullptr) {
    submenu_controller_->Close();
  }
}

void QuickInsertItemView::SetCornerRadius(int corner_radius) {
  if (corner_radius_ == corner_radius) {
    return;
  }

  corner_radius_ = corner_radius;
  StyleUtil::InstallRoundedCornerHighlightPathGenerator(
      this, gfx::RoundedCornersF(corner_radius_));
  UpdateBackground();
}

QuickInsertSubmenuController* QuickInsertItemView::GetSubmenuController() {
  return submenu_controller_;
}

void QuickInsertItemView::SetSubmenuController(
    QuickInsertSubmenuController* submenu_controller) {
  submenu_controller_ = submenu_controller;
}

QuickInsertItemView::ItemState QuickInsertItemView::GetItemState() const {
  return item_state_;
}

void QuickInsertItemView::SetItemState(ItemState item_state) {
  if (item_state_ == item_state) {
    return;
  }

  item_state_ = item_state;
  UpdateBackground();

  switch (focus_indicator_style_) {
    case FocusIndicatorStyle::kFocusRingWithInsetGap:
      UpdateClipPathForFocusRingWithInsetGap();
      [[fallthrough]];
    case FocusIndicatorStyle::kFocusRing:
      views::FocusRing::Get(this)->SchedulePaint();
      break;
    case FocusIndicatorStyle::kFocusBar:
      SchedulePaint();
      break;
  }
}

void QuickInsertItemView::UpdateClipPathForFocusRingWithInsetGap() {
  if (focus_indicator_style_ != FocusIndicatorStyle::kFocusRingWithInsetGap) {
    return;
  }

  SkPath clip_path;
  if (item_state_ == ItemState::kPseudoFocused) {
    gfx::RectF inset_bounds(GetLocalBounds());
    const SkScalar radius =
        SkIntToScalar(corner_radius_ - kPseudoFocusClipInset);
    inset_bounds.Inset(kPseudoFocusClipInset);
    clip_path.addRoundRect(gfx::RectFToSkRect(inset_bounds), radius, radius);
  }
  SetClipPath(clip_path);
}

void QuickInsertItemView::UpdateBackground() {
  if (GetState() == views::Button::ButtonState::STATE_HOVERED ||
      item_state_ == QuickInsertItemView::ItemState::kPseudoFocused) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysHoverOnSubtle, corner_radius_));
  } else {
    SetBackground(nullptr);
  }
}

BEGIN_METADATA(QuickInsertItemView)
END_METADATA

}  // namespace ash
