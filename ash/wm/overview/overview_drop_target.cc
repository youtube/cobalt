// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_drop_target.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/style/ash_color_id.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/window_properties.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

constexpr int kDropTargetBorderThickness = 2;

// `OverviewDropTargetView` represents a transparent view with border in
// overview. It includes a background view. Dragged window in tablet mode can be
// dragged into it and then dropped into overview.
class OverviewDropTargetView : public views::View {
 public:
  METADATA_HEADER(OverviewDropTargetView);

  OverviewDropTargetView() {
    SetUseDefaultFillLayout(true);

    const int corner_radius =
        chromeos::features::IsJellyrollEnabled()
            ? kOverviewItemCornerRadius
            : views::LayoutProvider::Get()->GetCornerRadiusMetric(
                  views::Emphasis::kLow);

    background_view_ = AddChildView(std::make_unique<views::View>());
    background_view_->SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshShieldAndBase20, corner_radius, /*for_border_thickness=*/0));

    SetBorder(views::CreateThemedRoundedRectBorder(
        kDropTargetBorderThickness, corner_radius,
        cros_tokens::kCrosSysSystemBaseElevated));
  }
  OverviewDropTargetView(const OverviewDropTargetView&) = delete;
  OverviewDropTargetView& operator=(const OverviewDropTargetView&) = delete;
  ~OverviewDropTargetView() override = default;

  // Updates the visibility of `background_view_` since it is only shown when
  // drop target is selected in overview.
  void UpdateBackgroundVisibility(bool visible) {
    background_view_->SetVisible(visible);
  }

  raw_ptr<views::View> background_view_ = nullptr;
};

BEGIN_METADATA(OverviewDropTargetView, views::View)
END_METADATA

OverviewDropTarget::OverviewDropTarget(OverviewGrid* overview_grid)
    : OverviewItemBase(overview_grid->overview_session(),
                       overview_grid,
                       overview_grid->root_window()) {
  CreateItemWidget();
}

OverviewDropTarget::~OverviewDropTarget() = default;

void OverviewDropTarget::UpdateBackgroundVisibility(
    const gfx::Point& location_in_screen) {
  OverviewDropTargetView* drop_target_view =
      static_cast<OverviewDropTargetView*>(item_widget_->GetContentsView());
  drop_target_view->UpdateBackgroundVisibility(
      item_widget_->GetWindowBoundsInScreen().Contains(location_in_screen));
}

aura::Window* OverviewDropTarget::GetWindow() {
  return nullptr;
}

std::vector<aura::Window*> OverviewDropTarget::GetWindows() {
  return {};
}

bool OverviewDropTarget::HasVisibleOnAllDesksWindow() {
  return false;
}

bool OverviewDropTarget::Contains(const aura::Window* target) const {
  return false;
}

OverviewItem* OverviewDropTarget::GetLeafItemForWindow(aura::Window* window) {
  return nullptr;
}

void OverviewDropTarget::RestoreWindow(bool reset_transform, bool animate) {}

void OverviewDropTarget::SetBounds(const gfx::RectF& target_bounds,
                                   OverviewAnimationType animation_type) {
  target_bounds_ = target_bounds;
  item_widget_->SetBounds(gfx::ToRoundedRect(target_bounds));
}

gfx::Transform OverviewDropTarget::ComputeTargetTransform(
    const gfx::RectF& target_bounds) {
  return gfx::Transform();
}

gfx::RectF OverviewDropTarget::GetWindowsUnionScreenBounds() const {
  return target_bounds_;
}

gfx::RectF OverviewDropTarget::GetTargetBoundsWithInsets() const {
  return GetWindowsUnionScreenBounds();
}

gfx::RectF OverviewDropTarget::GetTransformedBounds() const {
  return GetWindowsUnionScreenBounds();
}

float OverviewDropTarget::GetItemScale(int height) {
  return 1.f;
}

void OverviewDropTarget::ScaleUpSelectedItem(
    OverviewAnimationType animation_type) {}

void OverviewDropTarget::EnsureVisible() {}

OverviewFocusableView* OverviewDropTarget::GetFocusableView() const {
  return nullptr;
}

views::View* OverviewDropTarget::GetBackDropView() const {
  return nullptr;
}

void OverviewDropTarget::UpdateRoundedCornersAndShadow() {}

void OverviewDropTarget::SetOpacity(float opacity) {}

float OverviewDropTarget::GetOpacity() const {
  return 1.f;
}

void OverviewDropTarget::PrepareForOverview() {}

void OverviewDropTarget::OnStartingAnimationComplete() {}

void OverviewDropTarget::HideForSavedDeskLibrary(bool animate) {}

void OverviewDropTarget::RevertHideForSavedDeskLibrary(bool animate) {}

void OverviewDropTarget::CloseWindows() {}

void OverviewDropTarget::Restack() {}

void OverviewDropTarget::StartDrag() {}

void OverviewDropTarget::OnOverviewItemDragStarted(OverviewItemBase* item) {}

void OverviewDropTarget::OnOverviewItemDragEnded(bool snap) {}

void OverviewDropTarget::OnOverviewItemContinuousScroll(
    const gfx::Transform& target_transform,
    float scroll_ratio) {}

void OverviewDropTarget::SetVisibleDuringItemDragging(bool visible,
                                                      bool animate) {
  SetWindowsVisibleDuringItemDragging({item_widget_->GetNativeWindow()},
                                      visible, animate);
}

void OverviewDropTarget::UpdateCannotSnapWarningVisibility(bool animate) {}

void OverviewDropTarget::HideCannotSnapWarning(bool animate) {}

void OverviewDropTarget::OnMovingItemToAnotherDesk() {}

void OverviewDropTarget::UpdateMirrorsForDragging(bool is_touch_dragging) {}

void OverviewDropTarget::DestroyMirrorsForDragging() {}

void OverviewDropTarget::Shutdown() {}

void OverviewDropTarget::AnimateAndCloseItem(bool up) {}

void OverviewDropTarget::StopWidgetAnimation() {}

OverviewGridWindowFillMode OverviewDropTarget::GetWindowDimensionsType() const {
  return OverviewGridWindowFillMode::kNormal;
}

void OverviewDropTarget::UpdateWindowDimensionsType() {}

gfx::Point OverviewDropTarget::GetMagnifierFocusPointInScreen() const {
  return gfx::Point();
}

const gfx::RoundedCornersF OverviewDropTarget::GetRoundedCorners() const {
  return gfx::RoundedCornersF();
}

void OverviewDropTarget::CreateItemWidget() {
  item_widget_ = std::make_unique<views::Widget>(CreateOverviewItemWidgetParams(
      desks_util::GetActiveDeskContainerForRoot(root_window_),
      "OverviewOverviewDropTarget", /*accept_events=*/false));
  item_widget_->set_focus_on_creation(false);
  item_widget_->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  item_widget_->SetContentsView(std::make_unique<OverviewDropTargetView>());

  aura::Window* drop_target_window = item_widget_->GetNativeWindow();
  drop_target_window->parent()->StackChildAtBottom(drop_target_window);
  item_widget_->Show();
}

}  // namespace ash
