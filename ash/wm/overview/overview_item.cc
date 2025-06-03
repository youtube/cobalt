// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item.h"

#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_label_widget.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/templates/saved_desk_animations.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_drop_target.h"
#include "ash/wm/overview/overview_focus_cycler.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_event_handler.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/scoped_overview_hide_windows.h"
#include "ash/wm/raster_scale/raster_scale_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_mini_view_header_view.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;

// Opacity for fading out during closing a window.
constexpr float kClosingItemOpacity = 0.8f;

// Before closing a window animate both the window and the caption to shrink by
// this fraction of size.
constexpr float kPreCloseScale = 0.02f;

// The amount of translation an item animates by when it is closed by using
// swipe to close.
constexpr int kSwipeToCloseCloseTranslationDp = 96;

// When an item is being dragged, the bounds are outset horizontally by this
// fraction of the width, and vertically by this fraction of the height. The
// outset in each dimension is on both sides, for a total of twice this much
// change in the size of the item along that dimension.
constexpr float kDragWindowScale = 0.05f;

// A self-deleting animation observer that runs the given callback when its
// associated animation completes. Optionally takes a callback that is run when
// the animation starts.
class AnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserver(base::OnceClosure on_animation_finished)
      : AnimationObserver(base::NullCallback(),
                          std::move(on_animation_finished)) {}

  AnimationObserver(base::OnceClosure on_animation_started,
                    base::OnceClosure on_animation_finished)
      : on_animation_started_(std::move(on_animation_started)),
        on_animation_finished_(std::move(on_animation_finished)) {
    DCHECK(!on_animation_finished_.is_null());
  }

  AnimationObserver(const AnimationObserver&) = delete;
  AnimationObserver& operator=(const AnimationObserver&) = delete;

  ~AnimationObserver() override = default;

  // ui::ImplicitAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override {
    if (!on_animation_started_.is_null())
      std::move(on_animation_started_).Run();
  }
  void OnImplicitAnimationsCompleted() override {
    std::move(on_animation_finished_).Run();
    delete this;
  }

 private:
  base::OnceClosure on_animation_started_;
  base::OnceClosure on_animation_finished_;
};

// Applies |new_bounds_in_screen| to |widget|, animating and observing the
// transform if necessary.
void SetWidgetBoundsAndMaybeAnimateTransform(
    views::Widget* widget,
    const gfx::Rect& new_bounds_in_screen,
    OverviewAnimationType animation_type,
    ui::ImplicitAnimationObserver* observer) {
  aura::Window* window = widget->GetNativeWindow();
  gfx::RectF previous_bounds = gfx::RectF(window->GetBoundsInScreen());
  window->SetBoundsInScreen(
      new_bounds_in_screen,
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
  if (animation_type == OVERVIEW_ANIMATION_NONE ||
      animation_type == OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER ||
      previous_bounds.IsEmpty()) {
    window->SetTransform(gfx::Transform());

    // Make sure that |observer|, which could be a self-deleting object, will
    // not be leaked.
    DCHECK(!observer);
    return;
  }

  // For animations, compute the transform needed to place the widget at its
  // new bounds back to the old bounds, and then apply the idenity
  // transform. This so the bounds visually line up the concurrent transform
  // animations. Also transform animations may be more performant.
  const gfx::RectF current_bounds = gfx::RectF(window->GetBoundsInScreen());
  window->SetTransform(
      gfx::TransformBetweenRects(current_bounds, previous_bounds));
  ScopedOverviewAnimationSettings settings(animation_type, window);
  if (observer)
    settings.AddObserver(observer);
  window->SetTransform(gfx::Transform());
}

bool IsContinuousScrollInProgress() {
  return features::IsContinuousOverviewScrollAnimationEnabled() &&
         OverviewController::Get()->is_continuous_scroll_in_progress();
}

}  // namespace

OverviewItem::OverviewItem(aura::Window* window,
                           OverviewSession* overview_session,
                           OverviewGrid* overview_grid,
                           WindowDestructionDelegate* delegate,
                           bool eligible_for_shadow_config)
    : OverviewItemBase(overview_session,
                       overview_grid,
                       window->GetRootWindow()),
      root_window_(window->GetRootWindow()),
      transform_window_(this, window),
      window_destruction_delegate_(delegate),
      eligible_for_shadow_config_(eligible_for_shadow_config),
      animation_disabler_(window) {
  CHECK(window_destruction_delegate_);
  CreateItemWidget();
  window->AddObserver(this);
  WindowState::Get(window)->AddObserver(this);
}

OverviewItem::~OverviewItem() {
  aura::Window* window = GetWindow();
  WindowState::Get(window)->RemoveObserver(this);
  window->RemoveObserver(this);
}

void OverviewItem::UpdateItemContentViewForMinimizedWindow() {
  overview_item_view_->RefreshPreviewView();
}

void OverviewItem::UpdateRoundedCorners() {
  // TODO(sammiequon): Clean up this function.

  // Do not show the rounded corners and the shadow if overview is shutting
  // down or we're currently in entering overview animation. Also don't update
  // or animate the window's frame header clip under these conditions. If the
  // feature ContinuousOverviewScrollAnimation is enabled, always show rounded
  // corners for minimized windows, and show rounded corners for non-minimized
  // windows after the continuous scroll has ended.
  OverviewController* overview_controller = OverviewController::Get();
  bool show_rounded_corners_for_start_animation = false;
  if (features::IsContinuousOverviewScrollAnimationEnabled() &&
      !Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    show_rounded_corners_for_start_animation =
        transform_window_.IsMinimizedOrTucked() ||
        !IsContinuousScrollInProgress();
  } else {
    show_rounded_corners_for_start_animation =
        !overview_controller->IsInStartAnimation();
  }

  const bool is_shutting_down =
      !overview_controller || !overview_controller->InOverviewSession();
  const bool should_show_rounded_corners =
      !is_shutting_down && show_rounded_corners_for_start_animation;
  if (should_show_rounded_corners) {
    overview_item_view_->RefreshItemVisuals();
    if (!transform_window_.IsMinimizedOrTucked()) {
      transform_window_.UpdateRoundedCorners(should_show_rounded_corners);
    }
  }
}

OverviewAnimationType OverviewItem::GetExitOverviewAnimationType() const {
  if (overview_session_->enter_exit_overview_type() ==
      OverviewEnterExitType::kImmediateExit) {
    return OVERVIEW_ANIMATION_NONE;
  }

  return should_animate_when_exiting_
             ? OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_EXIT
             : OVERVIEW_ANIMATION_NONE;
}

OverviewAnimationType OverviewItem::GetExitTransformAnimationType() const {
  if (is_moving_to_another_desk_ ||
      overview_session_->enter_exit_overview_type() ==
          OverviewEnterExitType::kImmediateExit) {
    return OVERVIEW_ANIMATION_NONE;
  }

  return should_animate_when_exiting_ ? OVERVIEW_ANIMATION_RESTORE_WINDOW
                                      : OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO;
}

aura::Window* OverviewItem::GetWindow() {
  return transform_window_.window();
}

std::vector<aura::Window*> OverviewItem::GetWindows() {
  return {transform_window_.window()};
}

bool OverviewItem::HasVisibleOnAllDesksWindow() {
  return desks_util::IsWindowVisibleOnAllWorkspaces(GetWindow());
}

bool OverviewItem::Contains(const aura::Window* target) const {
  return transform_window_.Contains(target);
}

OverviewItem* OverviewItem::GetLeafItemForWindow(aura::Window* window) {
  return window == GetWindow() ? this : nullptr;
}

void OverviewItem::SetBounds(const gfx::RectF& target_bounds,
                             OverviewAnimationType animation_type) {
  // Pause raster scale updates during SetBounds. For example, if we perform an
  // item spawned animation, we set the initial transform but immediately start
  // an animation, so we don't want to trigger a raster scale update for the
  // initial transform.
  ScopedPauseRasterScaleUpdates scoped_pause;

  if (in_bounds_update_ || !OverviewController::Get()->InOverviewSession()) {
    return;
  }

  // Do not animate if the resulting bounds does not change or current animation
  // is still in progress. The original window may change bounds so we still
  // need to call `SetItemBounds()` to update the window transform.
  OverviewAnimationType new_animation_type = animation_type;
  if (GetWindow()->layer()->GetAnimator()->is_animating() ||
      target_bounds == target_bounds_) {
    new_animation_type = OVERVIEW_ANIMATION_NONE;
  }

  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  // If `target_bounds_` is empty, this is the first update. Let
  // `UpdateHeaderLayout()` know, as we do not want `item_widget_` to be
  // animated with the window.
  const bool is_first_update = target_bounds_.IsEmpty();
  target_bounds_ = target_bounds;

  // Run at the exit of this function to update rounded corners, shadow and the
  // cannot snap widget.
  base::ScopedClosureRunner at_exit_runner(base::BindOnce(
      [](base::WeakPtr<OverviewItem> item,
         OverviewAnimationType animation_type) {
        if (!item.get()) {
          return;
        }

        // Shadow is normally set after an animation is finished. In the case of
        // no animations, manually set the shadow. Shadow relies on both the
        // window transform and `item_widget_`'s new bounds so set it after
        // `SetItemBounds()` and `UpdateHeaderLayout()`. Do not apply the shadow
        // for drop target.
        if (animation_type == OVERVIEW_ANIMATION_NONE) {
          item->UpdateRoundedCornersAndShadow();
        }

        if (RoundedLabelWidget* widget = item->cannot_snap_widget_.get()) {
          SetWidgetBoundsAndMaybeAnimateTransform(
              widget,
              widget->GetBoundsCenteredIn(
                  ToStableSizeRoundedRect(item->GetTargetBoundsWithInsets())),
              animation_type, nullptr);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), new_animation_type));

  // For non minimized or tucked windows, we simply apply the transform and
  // update the header.
  if (!transform_window_.IsMinimizedOrTucked()) {
    UpdateHeaderLayout(is_first_update ? OVERVIEW_ANIMATION_NONE
                                       : new_animation_type);
    SetItemBounds(target_bounds, new_animation_type, is_first_update);
    return;
  }

  // If the window is minimized we can avoid applying transforms on the original
  // window.
  item_widget_->GetLayer()->GetAnimator()->StopAnimating();

  const gfx::Rect minimized_bounds = ToStableSizeRoundedRect(target_bounds);
  OverviewAnimationType minimized_animation_type =
      is_first_update ? OVERVIEW_ANIMATION_NONE : new_animation_type;
  SetWidgetBoundsAndMaybeAnimateTransform(
      item_widget_.get(), minimized_bounds, minimized_animation_type,
      minimized_animation_type ==
              OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW
          ? new AnimationObserver{base::BindOnce(
                                      &OverviewItem::
                                          OnItemBoundsAnimationStarted,
                                      weak_ptr_factory_.GetWeakPtr()),
                                  base::BindOnce(
                                      &OverviewItem::OnItemBoundsAnimationEnded,
                                      weak_ptr_factory_.GetWeakPtr())}
          : nullptr);

  // If the window was minimized while in overview, the preview may not exist.
  // `OverviewItemView::SetShowPreview()` is a no-op if the preview already
  // exists, so it is free to ensure it here.
  overview_item_view_->SetShowPreview(true);
  ui::Layer* preview_layer = overview_item_view_->preview_view()->layer();

  // Minimized windows have a `WindowPreviewView` which mirrors content from the
  // window. `target_bounds` may not have a matching aspect ratio to the
  // actual window (eg. in splitview overview). In this case, the contents
  // will be squashed to fit the given bounds. To get around this, stretch out
  // the contents so that it matches `unclipped_size_`, then clip the layer to
  // match `target_bounds`. This is what is done on non-minimized windows.
  if (unclipped_size_) {
    gfx::SizeF target_size(*unclipped_size_);
    gfx::SizeF preview_size = GetTargetBoundsWithInsets().size();
    target_size.Enlarge(0, -kHeaderHeightDp);

    const float x_scale = target_size.width() / preview_size.width();
    const float y_scale = target_size.height() / preview_size.height();
    const auto transform = gfx::Transform::MakeScale(x_scale, y_scale);
    preview_layer->SetTransform(transform);

    // Transform affects clip rect so scale the clip rect so that the final
    // size is equal to the untransformed layer.
    gfx::Size clip_size(preview_layer->size());
    clip_size =
        gfx::ScaleToRoundedSize(clip_size, 1.f / x_scale, 1.f / y_scale);
    preview_layer->SetClipRect(gfx::Rect(clip_size));
  } else {
    preview_layer->SetClipRect(gfx::Rect());
    preview_layer->SetTransform(gfx::Transform());
  }

  if (!is_first_update) {
    return;
  }

  // On the first update show `item_widget_`. It's created on creation of
  // `this`, and needs to be shown as soon as its bounds have been determined
  // as it contains a mirror view of the window in its contents. The header
  // will be faded in later to match non minimized windows.
  if (!should_animate_when_entering_) {
    item_widget_->GetLayer()->SetOpacity(1.f);
    return;
  }

  if (new_animation_type == OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW) {
    PerformItemSpawnedAnimation(item_widget_->GetNativeWindow(),
                                gfx::Transform{});
    return;
  }

  // If entering from home launcher, use the home specific (fade) animation.
  OverviewAnimationType fade_animation = animation_type;
  if (fade_animation != OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER) {
    fade_animation = OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN;
  }

  FadeInWidgetToOverview(item_widget_.get(), fade_animation,
                         /*observe=*/true);

  // Update the item header visibility immediately if entering from home
  // launcher.
  if (new_animation_type == OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER) {
    overview_item_view_->SetHeaderVisibility(
        OverviewItemView::HeaderVisibility::kVisible, /*animate=*/true);
  }

  // Update the item header visibility immediately without an animation.
  if (new_animation_type == OVERVIEW_ANIMATION_NONE) {
    overview_item_view_->SetHeaderVisibility(
        OverviewItemView::HeaderVisibility::kVisible, /*animate=*/false);
  }
}

gfx::Transform OverviewItem::ComputeTargetTransform(
    const gfx::RectF& target_bounds) {
  gfx::RectF screen_rect = gfx::RectF(GetWindowsUnionScreenBounds());

  // Avoid division by zero by ensuring screen bounds is not empty.
  gfx::SizeF screen_size(screen_rect.size());
  screen_size.SetToMax(gfx::SizeF(1.f, 1.f));
  screen_rect.set_size(screen_size);

  gfx::RectF transformed_bounds = target_bounds;

  // Update `transformed_bounds` to match the unclipped size of the window, so
  // we transform the window to the correct size.
  if (unclipped_size_) {
    transformed_bounds.set_size(gfx::SizeF(*unclipped_size_));
  }

  const int top_view_inset = transform_window_.GetTopInset();
  gfx::RectF overview_item_bounds =
      transform_window_.ShrinkRectToFitPreservingAspectRatio(
          screen_rect, transformed_bounds, top_view_inset, kHeaderHeightDp);

  if (transform_window_.type() == OverviewGridWindowFillMode::kNormal ||
      transform_window_.type() == OverviewGridWindowFillMode::kLetterBoxed) {
    overview_item_bounds.set_x(transformed_bounds.x());
    overview_item_bounds.set_width(transformed_bounds.width());
  }

  // Adjust the `overview_item_bounds` y position and height if the window has
  // normal or pillar dimensions type to make sure there's no gap between the
  // header and the window and no empty space at the end of the overview item
  // container.
  if (transform_window_.type() == OverviewGridWindowFillMode::kNormal ||
      transform_window_.type() == OverviewGridWindowFillMode::kPillarBoxed) {
    if (!overview_item_view_->header_view()->GetBoundsInScreen().IsEmpty()) {
      // The window top bar's target height with the transform.
      const float window_top_inset_target_height =
          target_bounds.height() / screen_rect.height() * top_view_inset;
      overview_item_bounds.set_y(
          overview_item_view_->header_view()->GetBoundsInScreen().bottom() -
          window_top_inset_target_height);
      overview_item_bounds.set_height(target_bounds.height() - kHeaderHeightDp +
                                      window_top_inset_target_height);
    }
  }

  return gfx::TransformBetweenRects(screen_rect, overview_item_bounds);
}

void OverviewItem::RestoreWindow(bool reset_transform, bool animate) {
  TRACE_EVENT0("ui", "OverviewItem::RestoreWindow");

  // TODO(oshima): SplitViewController has its own logic to adjust the
  // target state in `SplitViewController::OnOverviewModeEnding`.
  // Unify the mechanism to control it and remove ifs.
  if (Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      !SplitViewController::Get(root_window_)->InSplitViewMode() &&
      reset_transform) {
    MaximizeIfSnapped(GetWindow());
  }

  GetWindow()->ClearProperty(kForceVisibleInMiniViewKey);
  for (aura::Window* transient_child : GetTransientTreeIterator(GetWindow())) {
    transient_child->ClearProperty(kForceVisibleInMiniViewKey);
  }

  overview_item_view_->OnOverviewItemWindowRestoring();
  transform_window_.RestoreWindow(reset_transform, animate);

  if (!transform_window_.IsMinimizedOrTucked()) {
    return;
  }

  const auto enter_exit_type = overview_session_->enter_exit_overview_type();
  if (is_moving_to_another_desk_ ||
      enter_exit_type == OverviewEnterExitType::kImmediateExit) {
    overview_session_->focus_cycler()->OnViewDestroyingOrDisabling(
        overview_item_view_);
    ImmediatelyCloseWidgetOnExit(std::move(item_widget_));
    overview_item_view_ = nullptr;
    return;
  }

  OverviewAnimationType animation_type =
      GetExitOverviewAnimationTypeForMinimizedWindow(enter_exit_type);
  FadeOutWidgetFromOverview(std::move(item_widget_), animation_type);
}

gfx::RectF OverviewItem::GetWindowsUnionScreenBounds() const {
  return GetUnionScreenBoundsForWindow(transform_window_.window());
}

gfx::RectF OverviewItem::GetTargetBoundsWithInsets() const {
  gfx::RectF target_bounds = target_bounds_;
  target_bounds.Inset(gfx::InsetsF::TLBR(kHeaderHeightDp, 0, 0, 0));
  return target_bounds;
}

gfx::RectF OverviewItem::GetTransformedBounds() const {
  return transform_window_.GetTransformedBounds();
}

float OverviewItem::GetItemScale(int height) {
  return ScopedOverviewTransformWindow::GetItemScale(
      GetWindowsUnionScreenBounds().height(), height,
      transform_window_.GetTopInset(), kHeaderHeightDp);
}

void OverviewItem::ScaleUpSelectedItem(OverviewAnimationType animation_type) {
  gfx::RectF scaled_bounds = target_bounds();
  scaled_bounds.Inset(
      gfx::InsetsF::VH(-scaled_bounds.height() * kDragWindowScale,
                       -scaled_bounds.width() * kDragWindowScale));
  if (unclipped_size_) {
    // If a clipped item is scaled up, we need to recalculate the unclipped
    // size.
    const int height = scaled_bounds.height();
    const int width =
        overview_grid_->CalculateWidthAndMaybeSetUnclippedBounds(this, height);
    DCHECK(unclipped_size_);
    const gfx::SizeF new_size(width, height);
    scaled_bounds.set_size(new_size);
    scaled_bounds.ClampToCenteredSize(new_size);
  }
  SetBounds(scaled_bounds, animation_type);
}

void OverviewItem::EnsureVisible() {
  transform_window_.EnsureVisible();
}

OverviewFocusableView* OverviewItem::GetFocusableView() const {
  return overview_item_view_;
}

views::View* OverviewItem::GetBackDropView() const {
  return overview_item_view_->backdrop_view();
}

void OverviewItem::UpdateRoundedCornersAndShadow() {
  UpdateRoundedCorners();

  // The shadow should not be created if `this` is hosted by an
  // `OverviewGroupItem` together with another `OverviewItem` (the group-level
  // shadow will be installed instead).
  if (!eligible_for_shadow_config_) {
    return;
  }

  // The shadow should be hidden if
  // 1) Rounded corners are available;
  // 2) `this` is being animated.
  const bool is_animating = transform_window_.GetOverviewWindow()
                                ->layer()
                                ->GetAnimator()
                                ->is_animating() ||
                            IsContinuousScrollInProgress();
  const bool shadow_visible = !GetRoundedCorners().IsEmpty() &&
                              !is_animating;

  // The shadow should always match the size of the item minus the border
  // instead of the transformed window or preview view, since for the window
  // which has `kPillarBoxed` or `kLetterBoxed` dimension types, it doesn't
  // occupy the whole remaining area of the overview item widget minus the
  // header view in which case, the shadow will look weird if it matches the
  // size of the transformed window or preview view.
  RefreshShadowVisuals(shadow_visible);
}

void OverviewItem::SetOpacity(float opacity) {
  item_widget_->SetOpacity(opacity);
  transform_window_.SetOpacity(opacity);
  if (cannot_snap_widget_) {
    cannot_snap_widget_->SetOpacity(opacity);
  }
}

float OverviewItem::GetOpacity() const {
  return item_widget_->GetNativeWindow()->layer()->GetTargetOpacity();
}

void OverviewItem::PrepareForOverview() {
  transform_window_.PrepareForOverview();
  prepared_for_overview_ = true;
}

void OverviewItem::OnStartingAnimationComplete() {
  DCHECK(item_widget_);

  if (transform_window_.IsMinimizedOrTucked()) {
    // Fade the title in if minimized or tucked. The rest of `item_widget_`
    // should already be shown.
    overview_item_view_->SetHeaderVisibility(
        OverviewItemView::HeaderVisibility::kVisible, /*animate=*/true);
  } else if (!IsContinuousScrollInProgress() &&
             overview_session_->enter_exit_overview_type() ==
                 OverviewEnterExitType::
                     kContinuousAnimationEnterOnScrollUpdate) {
    // If a continuous scroll has ended, make the header visible again.
    item_widget_->GetLayer()->SetOpacity(1.f);
  } else {
    FadeInWidgetToOverview(item_widget_.get(),
                           OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
                           /*observe=*/false);
  }

  const bool show_backdrop =
      GetWindowDimensionsType() != OverviewGridWindowFillMode::kNormal;
  overview_item_view_->SetBackdropVisibility(show_backdrop);
  UpdateCannotSnapWarningVisibility(/*animate=*/true);
}

void OverviewItem::HideForSavedDeskLibrary(bool animate) {
  // To hide the window, we will set its layer opacity to 0. This would normally
  // also hide the window from the mini view, which we don't want. By setting a
  // property on the window, we can force it to stay visible.
  GetWindow()->SetProperty(kForceVisibleInMiniViewKey, true);

  // Temporarily hide this window in overview, so that dark/light theme change
  // does not reset the layer visible. If `animate` is false, the callback will
  // not run in `PerformFadeOutLayer`. Thus, here we make sure the window is
  // also hidden in that case.
  DCHECK(item_widget_);
  hide_window_in_overview_callback_.Reset(base::BindOnce(
      &OverviewItem::HideWindowInOverview, weak_ptr_factory_.GetWeakPtr()));
  PerformFadeOutLayer(item_widget_->GetLayer(), animate,
                      hide_window_in_overview_callback_.callback());
  if (!animate) {
    // Cancel the callback if we are going to run it directly.
    hide_window_in_overview_callback_.Cancel();
    HideWindowInOverview();
  }

  for (aura::Window* transient_child : GetTransientTreeIterator(GetWindow())) {
    transient_child->SetProperty(kForceVisibleInMiniViewKey, true);
    PerformFadeOutLayer(transient_child->layer(), animate, base::DoNothing());
  }

  item_widget_event_blocker_ =
      std::make_unique<aura::ScopedWindowEventTargetingBlocker>(
          item_widget_->GetNativeWindow());

  HideCannotSnapWarning(animate);
}

void OverviewItem::RevertHideForSavedDeskLibrary(bool animate) {
  // This might run before `HideForSavedDeskLibrary()`, thus cancel the
  // callback to prevent such case.
  hide_window_in_overview_callback_.Cancel();

  // Restore and show the window back to overview.
  ShowWindowInOverview();

  // `item_widget_` may be null during shutdown if the window is minimized.
  if (item_widget_) {
    PerformFadeInLayer(item_widget_->GetLayer(), animate);
  }

  for (aura::Window* transient_child :
       GetTransientTreeIterator(transform_window_.window())) {
    PerformFadeInLayer(transient_child->layer(), animate);
  }

  item_widget_event_blocker_.reset();

  UpdateCannotSnapWarningVisibility(animate);
}

void OverviewItem::CloseWindows() {
  RefreshShadowVisuals(/*shadow_visible=*/false);

  gfx::RectF inset_bounds(target_bounds_);
  inset_bounds.Inset(gfx::InsetsF::VH(target_bounds_.height() * kPreCloseScale,
                                      target_bounds_.width() * kPreCloseScale));
  // Scale down both the window and label.
  SetBounds(inset_bounds, OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM);

  // First animate opacity to an intermediate value concurrently with the
  // scaling animation.
  AnimateOpacity(kClosingItemOpacity, OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM);

  // Fade out the window and the label, effectively hiding them.
  AnimateOpacity(/*opacity=*/0.0, OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM);

  // `transform_window_` will delete `this` by deleting the widget associated
  // with `this`.
  transform_window_.Close();
}

void OverviewItem::Restack() {
  aura::Window* window = GetWindow();
  aura::Window* parent_window = window->parent();
  aura::Window* stacking_target = nullptr;

  // Stack `window` below the split view window if split view is active.
  SplitViewController* split_view_controller =
      SplitViewController::Get(root_window_);
  if (split_view_controller->InSplitViewMode()) {
    aura::Window* snapped_window =
        split_view_controller->GetDefaultSnappedWindow();
    if (snapped_window->parent() == parent_window) {
      stacking_target = snapped_window;
    }
  }
  // Stack `window` below the last window in `overview_grid_` that comes before
  // `window` and has the same parent.
  for (const std::unique_ptr<OverviewItemBase>& overview_item :
       overview_grid_->window_list()) {
    if (overview_item.get() == this ||
        overview_item.get() == overview_grid_->drop_target()) {
      break;
    }

    if (overview_item->GetWindow()->parent() == parent_window) {
      stacking_target = overview_item->item_widget()->GetNativeWindow();
    }
  }

  if (stacking_target) {
    DCHECK_EQ(parent_window, stacking_target->parent());
    parent_window->StackChildBelow(window, stacking_target);
  }
  DCHECK_EQ(parent_window, item_widget_->GetNativeWindow()->parent());
  parent_window->StackChildBelow(item_widget_->GetNativeWindow(), window);
  if (cannot_snap_widget_) {
    DCHECK_EQ(parent_window, cannot_snap_widget_->GetNativeWindow()->parent());
    parent_window->StackChildAbove(cannot_snap_widget_->GetNativeWindow(),
                                   window);
  }
}

void OverviewItem::StartDrag() {
  // Stack the window and the widget window at the top. This is to ensure that
  // they appear above other app windows, as well as above the desks bar. Note
  // that the stacking operations are done in this order to make sure that the
  // window appears above the widget window.
  if (aura::Window* widget_window = item_widget_->GetNativeWindow()) {
    widget_window->parent()->StackChildAtTop(widget_window);
  }

  aura::Window* window = GetWindow();
  window->parent()->StackChildAtTop(window);
}

void OverviewItem::OnOverviewItemDragStarted(OverviewItemBase* item) {
  is_being_dragged_ = (item == this);

  if (chromeos::features::IsJellyrollEnabled()) {
    overview_item_view_->SetCloseButtonVisible(false);
  } else {
    overview_item_view_->SetHeaderVisibility(
        is_being_dragged_
            ? OverviewItemView::HeaderVisibility::kInvisible
            : OverviewItemView::HeaderVisibility::kCloseButtonInvisibleOnly,
        /*animate=*/true);
  }
}

void OverviewItem::OnOverviewItemDragEnded(bool snap) {
  if (snap) {
    if (!is_being_dragged_) {
      overview_item_view_->HideCloseInstantlyAndThenShowItSlowly();
    }
  } else {
    if (chromeos::features::IsJellyrollEnabled()) {
      overview_item_view_->SetCloseButtonVisible(true);
    } else {
      overview_item_view_->SetHeaderVisibility(
          OverviewItemView::HeaderVisibility::kVisible, /*animate=*/true);
    }
  }
  is_being_dragged_ = false;
}

void OverviewItem::OnOverviewItemContinuousScroll(
    const gfx::Transform& target_transform,
    float scroll_ratio) {
  auto* window = GetWindow();

  // TODO(sammiequon): This should use
  // `ScopedOverviewTransformWindow::IsMinimizedOrTucked()` since tucked
  // windows behave like minimized windows in overview, even if continuous
  // scroll and tucked windows will not be supported together.
  // Minimized windows slowly fade towards their target opacity 1.f. All other
  // windows transform towards their target transform. The operation may be
  // no-ops if the windows are at their final opacity and transform, which can
  // happen if the windows were completely occluded before entering overview.
  if (WindowState::Get(window)->IsMinimized()) {
    item_widget()->GetLayer()->SetOpacity(std::clamp(0.01f, scroll_ratio, 1.f));
  } else {
    gfx::Transform transform = gfx::Tween::TransformValueBetween(
        scroll_ratio, gfx::Transform(), target_transform);
    SetTransform(window, transform);
  }
}

void OverviewItem::SetVisibleDuringItemDragging(bool visible, bool animate) {
  SetWindowsVisibleDuringItemDragging(GetWindowsForHomeGesture(), visible,
                                      animate);
}

void OverviewItem::UpdateCannotSnapWarningVisibility(bool animate) {
  // Windows which can snap will never show this warning.
  bool visible = true;
  if (SplitViewController::Get(root_window_)
          ->ComputeSnapRatio(GetWindow())
          .has_value()) {
    visible = false;
  } else {
    const SplitViewController::State state =
        SplitViewController::Get(root_window_)->state();
    visible = state == SplitViewController::State::kPrimarySnapped ||
              state == SplitViewController::State::kSecondarySnapped;
  }

  if (!visible && !cannot_snap_widget_) {
    return;
  }

  if (!cannot_snap_widget_) {
    RoundedLabelWidget::InitParams params;
    params.horizontal_padding = kSplitviewLabelHorizontalInsetDp;
    params.vertical_padding = kSplitviewLabelVerticalInsetDp;
    params.rounding_dp = kSplitviewLabelRoundRectRadiusDp;
    params.preferred_height = kSplitviewLabelPreferredHeightDp;
    params.message = IDS_ASH_SPLIT_VIEW_CANNOT_SNAP;
    params.parent = GetWindow()->parent();
    cannot_snap_widget_ = std::make_unique<RoundedLabelWidget>();
    cannot_snap_widget_->Init(std::move(params));
    GetWindow()->parent()->StackChildAbove(
        cannot_snap_widget_->GetNativeWindow(), GetWindow());
  }
  if (animate) {
    DoSplitviewOpacityAnimation(
        cannot_snap_widget_->GetLayer(),
        visible ? SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN
                : SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT);
  } else {
    cannot_snap_widget_->GetLayer()->SetOpacity(visible ? 1.f : 0.f);
  }
  const gfx::Rect bounds = ToStableSizeRoundedRect(GetTargetBoundsWithInsets());
  cannot_snap_widget_->SetBoundsCenteredIn(bounds, /*animate=*/false);
}

void OverviewItem::HideCannotSnapWarning(bool animate) {
  if (!cannot_snap_widget_) {
    return;
  }
  if (animate) {
    DoSplitviewOpacityAnimation(cannot_snap_widget_->GetLayer(),
                                SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT);
  } else {
    cannot_snap_widget_->GetLayer()->SetOpacity(0.f);
  }
}

void OverviewItem::OnMovingItemToAnotherDesk() {
  is_moving_to_another_desk_ = true;
  // Restore the dragged item window, so that its transform is reset to
  // identity.
  RestoreWindow(/*reset_transform=*/true, /*animate=*/true);
}

void OverviewItem::UpdateMirrorsForDragging(bool is_touch_dragging) {
  DCHECK_GT(Shell::GetAllRootWindows().size(), 1u);
  const bool minimized_or_tucked = transform_window_.IsMinimizedOrTucked();

  // With Jellyroll, header is visible while dragging.
  if (minimized_or_tucked || chromeos::features::IsJellyrollEnabled()) {
    if (!item_mirror_for_dragging_) {
      item_mirror_for_dragging_ = std::make_unique<DragWindowController>(
          item_widget_->GetNativeWindow(), is_touch_dragging);
    }
    item_mirror_for_dragging_->Update();
  }

  // Minimized or tucked windows don't need to mirror the source as its already
  // in `item_widget_`.
  if (minimized_or_tucked) {
    return;
  }

  if (!window_mirror_for_dragging_) {
    window_mirror_for_dragging_ =
        std::make_unique<DragWindowController>(GetWindow(), is_touch_dragging);
  }
  window_mirror_for_dragging_->Update();
}

void OverviewItem::DestroyMirrorsForDragging() {
  item_mirror_for_dragging_.reset();
  window_mirror_for_dragging_.reset();
}

void OverviewItem::Shutdown() {
  TRACE_EVENT0("ui", "OverviewItem::Shutdown");
  // If `hide_windows` still manages the visibility of this overview item
  // window, remove it from the list without showing.
  ScopedOverviewHideWindows* hide_windows =
      overview_session_->hide_windows_for_saved_desks_grid();
  if (item_widget_ && hide_windows &&
      hide_windows->HasWindow(item_widget_->GetNativeWindow())) {
    hide_windows->RemoveWindow(item_widget_->GetNativeWindow(),
                               /*show_window=*/false);
  }

  DestroyMirrorsForDragging();
  item_widget_.reset();
  overview_item_view_ = nullptr;
}

void OverviewItem::AnimateAndCloseItem(bool up) {
  base::RecordAction(base::UserMetricsAction("WindowSelector_SwipeToClose"));

  animating_to_close_ = true;
  overview_session_->PositionWindows(/*animate=*/true);
  overview_item_view_->OnOverviewItemWindowRestoring();

  int translation_y = kSwipeToCloseCloseTranslationDp * (up ? -1 : 1);
  gfx::Transform transform;
  transform.Translate(gfx::Vector2d(0, translation_y));

  auto animate_window = [this](aura::Window* window,
                               const gfx::Transform& transform, bool observe) {
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM, window);
    gfx::Transform original_transform = window->transform();
    original_transform.PostConcat(transform);
    window->SetTransform(original_transform);
    if (observe) {
      settings.AddObserver(new AnimationObserver{
          base::BindOnce(&OverviewItem::OnWindowCloseAnimationCompleted,
                         weak_ptr_factory_.GetWeakPtr())});
    }
  };

  AnimateOpacity(0.0, OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM);
  if (cannot_snap_widget_) {
    animate_window(cannot_snap_widget_->GetNativeWindow(), transform, false);
  }
  if (!transform_window_.IsMinimizedOrTucked()) {
    animate_window(GetWindow(), transform, false);
  }
  animate_window(item_widget_->GetNativeWindow(), transform, true);
}

void OverviewItem::StopWidgetAnimation() {
  CHECK(item_widget_.get());
  item_widget_->GetNativeWindow()->layer()->GetAnimator()->StopAnimating();
}

OverviewGridWindowFillMode OverviewItem::GetWindowDimensionsType() const {
  return transform_window_.type();
}

void OverviewItem::UpdateWindowDimensionsType() {
  transform_window_.UpdateWindowDimensionsType();
  const bool show_backdrop =
      GetWindowDimensionsType() != OverviewGridWindowFillMode::kNormal;
  overview_item_view_->SetBackdropVisibility(show_backdrop);
}

gfx::Point OverviewItem::GetMagnifierFocusPointInScreen() const {
  return overview_item_view_->GetMagnifierFocusPointInScreen();
}

const gfx::RoundedCornersF OverviewItem::GetRoundedCorners() const {
  if (transform_window_.IsMinimizedOrTucked()) {
    return overview_item_view_->GetRoundedCorners();
  }

  aura::Window* window = transform_window_.window();
  const gfx::RoundedCornersF& header_rounded_corners =
      overview_item_view_->header_view()->GetHeaderRoundedCorners(window);
  const auto* layer = window->layer();
  const gfx::RoundedCornersF& transform_window_rounded_corners =
      layer->rounded_corner_radii();
  const float scale = layer->transform().To2dScale().x();
  return gfx::RoundedCornersF(
      header_rounded_corners.upper_left(), header_rounded_corners.upper_right(),
      transform_window_rounded_corners.lower_right() * scale,
      transform_window_rounded_corners.lower_left() * scale);
}

void OverviewItem::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  DCHECK_EQ(GetWindow(), window);

  if (!prepared_for_overview_)
    return;

  if (key != aura::client::kTopViewInset)
    return;

  if (window->GetProperty(aura::client::kTopViewInset) !=
      static_cast<int>(old)) {
    overview_grid_->PositionWindows(/*animate=*/false);
  }
}

void OverviewItem::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  CHECK_EQ(GetWindow(), window);

  // During preparation, window bounds can change. Ignore bounds change
  // notifications in this case; we'll reposition soon.
  if (!prepared_for_overview_)
    return;

  // Do not update the overview bounds if we're shutting down.
  if (!OverviewController::Get()->InOverviewSession()) {
    return;
  }

  // Do not update the overview item if the window is to be snapped into split
  // view. It will be removed from overview soon and will update overview grid
  // at that moment.
  if (SplitViewController::Get(window)->IsWindowInTransitionalState(window))
    return;

  if (reason == ui::PropertyChangeReason::NOT_FROM_ANIMATION)
    overview_item_view_->RefreshPreviewView();

  // Immediately finish any active bounds animation.
  window->layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS);
  UpdateWindowDimensionsType();
  overview_grid_->PositionWindows(/*animate=*/false);
}

void OverviewItem::OnWindowDestroying(aura::Window* window) {
  // TODO(b/298518626): Create a Delegate class to handle window destroying as
  // the current case may no longer apply to group item. We should inform its
  // direct parent to remove the item.
  CHECK_EQ(GetWindow(), window);

  if (is_being_dragged_) {
    CHECK_EQ(this, overview_session_->window_drag_controller()->item());
    overview_session_->window_drag_controller()->ResetGesture();
  }

  CHECK(window_destruction_delegate_);
  window_destruction_delegate_->OnOverviewItemWindowDestroying(
      this, /*reposition=*/!animating_to_close_);

  // Trigger a11y alert about the window represented by `this` is being
  // destroyed.
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_OVERVIEW_WINDOW_CLOSING_A11Y_ALERT, window->GetTitle()));
}

void OverviewItem::OnPreWindowStateTypeChange(WindowState* window_state,
                                              WindowStateType old_type) {
  // If entering overview and PIP happen at the same time, the PIP window is
  // incorrectly listed in the overview list, which is not allowed.
  if (window_state->IsPip())
    overview_session_->RemoveItem(this);
}

void OverviewItem::OnPostWindowStateTypeChange(WindowState* window_state,
                                               WindowStateType old_type) {
  // During preparation, window state can change, e.g. updating shelf
  // visibility may show the temporarily hidden (minimized) panels.
  if (!prepared_for_overview_)
    return;

  // Minimizing an originally active window will activate and unminimize the
  // window upon exiting, and the item window will be "moved" to fade out in
  // 'RestoreWindow'.
  if (!item_widget_)
    return;

  WindowStateType new_type = window_state->GetStateType();
  if (chromeos::IsMinimizedWindowStateType(old_type) ==
      chromeos::IsMinimizedWindowStateType(new_type)) {
    return;
  }

  const bool minimized_or_tucked = transform_window_.IsMinimizedOrTucked();
  overview_item_view_->SetShowPreview(minimized_or_tucked);
  if (!minimized_or_tucked) {
    EnsureVisible();
  }

  // Ensures the item widget is visible. |item_widget_| opacity is set to 0.f
  // and shown at either |SetBounds| or |OnStartingAnimationComplete| based on
  // the minimized state. It's possible the minimized state changes in between
  // for ARC apps, so just force show it here.
  item_widget_->GetLayer()->SetOpacity(1.f);

  overview_grid_->PositionWindows(/*animate=*/false);
}

void OverviewItem::CreateItemWidget() {
  TRACE_EVENT0("ui", "OverviewItem::CreateItemWidget");

  item_widget_ = std::make_unique<views::Widget>();
  item_widget_->set_focus_on_creation(false);
  item_widget_->Init(CreateOverviewItemWidgetParams(
      transform_window_.window()->parent(), "OverviewItemWidget",
      /*accept_events=*/true));
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  widget_window->parent()->StackChildBelow(widget_window, GetWindow());
  // Overview uses custom animations so remove the default ones.
  wm::SetWindowVisibilityAnimationTransition(widget_window, wm::ANIMATE_NONE);

  if (eligible_for_shadow_config_) {
    ConfigureTheShadow();
  }

  overview_item_view_ =
      item_widget_->SetContentsView(std::make_unique<OverviewItemView>(
          this,
          base::BindRepeating(&OverviewItem::CloseButtonPressed,
                              base::Unretained(this)),
          GetWindow(), transform_window_.IsMinimizedOrTucked()));
  item_widget_->Show();
  item_widget_->SetOpacity(
      overview_session_ && overview_session_->ShouldEnterWithoutAnimations()
          ? 1.f
          : 0.f);
  item_widget_->GetLayer()->SetMasksToBounds(/*masks_to_bounds=*/false);
}

void OverviewItem::OnWindowCloseAnimationCompleted() {
  transform_window_.Close();
}

void OverviewItem::OnItemSpawnedAnimationCompleted() {
  UpdateRoundedCornersAndShadow();
  if (should_restack_on_animation_end_) {
    Restack();
    should_restack_on_animation_end_ = false;
  }
  OnStartingAnimationComplete();
}

void OverviewItem::OnItemBoundsAnimationStarted() {
  // Remove the shadow before animating because it may affect animation
  // performance. The shadow will be added back once the animation is completed.
  // Note that we can't use `UpdateRoundedCornersAndShadow()` since we don't
  // want to update the rounded corners.
  RefreshShadowVisuals(/*shadow_visible=*/false);
}

void OverviewItem::OnItemBoundsAnimationEnded() {
  // Do nothing if overview is shutting down. See crbug.com/1025267 for when it
  // might happen.
  if (!OverviewController::Get()->InOverviewSession()) {
    return;
  }

  if (overview_session_->IsShowingSavedDeskLibrary()) {
    HideForSavedDeskLibrary(false);
    return;
  }

  UpdateRoundedCornersAndShadow();
  if (should_restack_on_animation_end_) {
    Restack();
    should_restack_on_animation_end_ = false;
  }
}

void OverviewItem::PerformItemSpawnedAnimation(
    aura::Window* window,
    const gfx::Transform& target_transform) {
  DCHECK(should_use_spawn_animation_);
  should_use_spawn_animation_ = false;

  constexpr float kInitialScaler = 0.1f;
  constexpr float kTargetScaler = 1.0f;

  // Scale-up |window| and fade it in along with the |cannot_snap_widget_|'s
  // window.
  gfx::Transform initial_transform = target_transform;
  initial_transform.Scale(kInitialScaler, kInitialScaler);
  SetTransform(window, initial_transform);
  transform_window_.SetOpacity(kInitialScaler);

  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  for (auto* window_iter :
       window_util::GetVisibleTransientTreeIterator(window)) {
    auto settings = std::make_unique<ScopedOverviewAnimationSettings>(
        OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW, window_iter);
    settings->DeferPaint();
    animation_settings.push_back(std::move(settings));
  }

  if (!animation_settings.empty()) {
    animation_settings.front()->AddObserver(new AnimationObserver{
        base::BindOnce(&OverviewItem::OnItemSpawnedAnimationCompleted,
                       weak_ptr_factory_.GetWeakPtr())});
  }
  SetTransform(window, target_transform);
  transform_window_.SetOpacity(kTargetScaler);

  if (cannot_snap_widget_) {
    aura::Window* cannot_snap_window = cannot_snap_widget_->GetNativeWindow();
    cannot_snap_window->layer()->SetOpacity(kInitialScaler);
    ScopedOverviewAnimationSettings label_animation_settings(
        OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW, cannot_snap_window);
    cannot_snap_window->layer()->SetOpacity(kTargetScaler);
  }
}

void OverviewItem::SetItemBounds(const gfx::RectF& target_bounds,
                                 OverviewAnimationType animation_type,
                                 bool is_first_update) {
  aura::Window* window = GetWindow();
  CHECK_EQ(root_window_, window->GetRootWindow());

  const gfx::Transform transform = ComputeTargetTransform(target_bounds);

  // Determine the amount of clipping we should put on the window. Note that the
  // clipping goes after setting a transform, as layer transform affects layer
  // clip.
  using ClippingType = ScopedOverviewTransformWindow::ClippingType;
  ScopedOverviewTransformWindow::ClippingData clipping_data{
      ClippingType::kCustom, gfx::SizeF()};
  if (unclipped_size_) {
    clipping_data.second = GetTargetBoundsWithInsets().size();
  } else if (is_first_update) {
    clipping_data.first = ClippingType::kEnter;
  }

  if (is_first_update &&
      animation_type == OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW) {
    PerformItemSpawnedAnimation(window, transform);
    transform_window_.SetClipping(clipping_data);
    return;
  }

  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW &&
      !animation_settings.empty() && !GetWindow()->is_destroying()) {
    animation_settings.front()->AddObserver(new AnimationObserver{
        base::BindOnce(&OverviewItem::OnItemBoundsAnimationStarted,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&OverviewItem::OnItemBoundsAnimationEnded,
                       weak_ptr_factory_.GetWeakPtr())});
  }
  SetTransform(window, transform);
  transform_window_.SetClipping(clipping_data);
}

void OverviewItem::UpdateHeaderLayout(OverviewAnimationType animation_type) {
  if (chromeos::features::IsJellyrollEnabled()) {
    UpdateHeaderLayoutCrOSNext(animation_type);
    return;
  }

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  ScopedOverviewAnimationSettings animation_settings(animation_type,
                                                     widget_window);
  // Create a start animation observer if this is an enter overview layout
  // animation.
  if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER ||
      animation_type == OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER) {
    auto enter_observer = std::make_unique<EnterAnimationObserver>();
    animation_settings.AddObserver(enter_observer.get());
    OverviewController::Get()->AddEnterAnimationObserver(
        std::move(enter_observer));
  }

  gfx::RectF item_bounds = target_bounds_;
  ::wm::TranslateRectFromScreen(root_window_, &item_bounds);
  const gfx::Point origin = gfx::ToRoundedPoint(item_bounds.origin());
  item_bounds.set_origin(gfx::PointF());
  widget_window->SetBounds(ToStableSizeRoundedRect(item_bounds));

  gfx::Transform label_transform;
  label_transform.Translate(origin.x(), origin.y());
  widget_window->SetTransform(label_transform);
}

void OverviewItem::UpdateHeaderLayoutCrOSNext(
    OverviewAnimationType animation_type) {
  gfx::RectF current_item_bounds(item_widget_->GetWindowBoundsInScreen());
  gfx::RectF target_item_bounds = target_bounds_;

  wm::TranslateRectFromScreen(root_window_, &current_item_bounds);
  wm::TranslateRectFromScreen(root_window_, &target_item_bounds);

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  if (current_item_bounds.IsEmpty()) {
    widget_window->SetBounds(ToStableSizeRoundedRect(target_item_bounds));
    return;
  }

  const gfx::Transform item_bounds_transform =
      gfx::TransformBetweenRects(target_item_bounds, current_item_bounds);
  widget_window->SetBounds(ToStableSizeRoundedRect(target_item_bounds));
  widget_window->SetTransform(item_bounds_transform);

  ScopedOverviewAnimationSettings item_animation_settings(animation_type,
                                                          widget_window);
  // Create a start animation observer if this is an enter overview layout
  // animation.
  if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER ||
      animation_type == OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER) {
    auto enter_observer = std::make_unique<EnterAnimationObserver>();
    item_animation_settings.AddObserver(enter_observer.get());
    OverviewController::Get()->AddEnterAnimationObserver(
        std::move(enter_observer));
  }
  widget_window->SetTransform(gfx::Transform());

  // The header doesn't need to be painted to a layer unless dragged.
  WindowMiniViewHeaderView* header_view = overview_item_view_->header_view();
  if (!header_view->layer()) {
    header_view->SetPaintToLayer();
    header_view->layer()->SetFillsBoundsOpaquely(false);
  }
  ui::Layer* header_layer = overview_item_view_->header_view()->layer();

  // Since header view is a child of the overview item view, the bounds
  // animation is appled to the header as well when it's applied to the overview
  // item. However, when calculating the target bounds for the window, it's
  // always assumed that the header's height is 40, there's a gap between the
  // header and the window during the animation. In order to neutralize the gap,
  // apply the reversed vertical transform to the header separately.
  float vertical_scale = item_bounds_transform.To2dScale().y();
  gfx::Transform vertical_reverse_transform =
      gfx::Transform::MakeScale(1.f, 1.f / vertical_scale);
  header_layer->SetTransform(vertical_reverse_transform);
  ScopedOverviewAnimationSettings header_animation_settings(
      animation_type, header_layer->GetAnimator());
  header_layer->SetTransform(gfx::Transform());
}

OverviewAnimationType
OverviewItem::GetExitOverviewAnimationTypeForMinimizedWindow(
    OverviewEnterExitType type) {
  // We should never get here when overview mode should exit immediately. The
  // minimized window's `item_widget_` should be closed and destroyed
  // immediately.
  DCHECK_NE(type, OverviewEnterExitType::kImmediateExit);

  // If the managed window has been hidden by the saved desk library, then
  // we must avoid animating a minimized window. See http://b/260001863.
  if (ScopedOverviewHideWindows* hide_windows =
          overview_session_->hide_windows_for_saved_desks_grid()) {
    if (hide_windows->HasWindow(item_widget_->GetNativeWindow())) {
      return OVERVIEW_ANIMATION_NONE;
    }
  }

  // OverviewEnterExitType can only be set to `kWindowMinimized` in tablet mode.
  // Fade out the minimized window without animation if switch from tablet mode
  // to clamshell mode.
  if (type == OverviewEnterExitType::kFadeOutExit) {
    return Shell::Get()->tablet_mode_controller()->InTabletMode()
               ? OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER
               : OVERVIEW_ANIMATION_NONE;
  }
  return should_animate_when_exiting_
             ? OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT
             : OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO;
}

void OverviewItem::AnimateOpacity(float opacity,
                                  OverviewAnimationType animation_type) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetOpacity(opacity);

  ScopedOverviewAnimationSettings scoped_animation_settings(
      animation_type, item_widget_->GetNativeWindow());
  item_widget_->SetOpacity(opacity);

  if (cannot_snap_widget_) {
    aura::Window* cannot_snap_widget_window =
        cannot_snap_widget_->GetNativeWindow();
    ScopedOverviewAnimationSettings scoped_animation_settings_2(
        animation_type, cannot_snap_widget_window);
    cannot_snap_widget_window->layer()->SetOpacity(opacity);
  }
}

void OverviewItem::CloseButtonPressed() {
  base::RecordAction(
      base::UserMetricsAction("WindowSelector_OverviewCloseButton"));
  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    base::RecordAction(
        base::UserMetricsAction("Tablet_WindowCloseFromOverviewButton"));
  }
  CloseWindows();
}

aura::Window::Windows OverviewItem::GetWindowsForHomeGesture() {
  aura::Window::Windows windows = {item_widget_->GetNativeWindow()};
  if (!transform_window_.IsMinimizedOrTucked()) {
    for (auto* window : GetTransientTreeIterator(GetWindow()))
      windows.push_back(window);
  }
  if (cannot_snap_widget_)
    windows.push_back(cannot_snap_widget_->GetNativeWindow());
  return windows;
}

void OverviewItem::HideWindowInOverview() {
  ScopedOverviewHideWindows* hide_windows =
      overview_session_->hide_windows_for_saved_desks_grid();
  DCHECK(hide_windows);

  // Hide the overview item window.
  if (item_widget_ && !hide_windows->HasWindow(item_widget_->GetNativeWindow()))
    hide_windows->AddWindow(item_widget_->GetNativeWindow());
}

void OverviewItem::ShowWindowInOverview() {
  ScopedOverviewHideWindows* hide_windows =
      overview_session_->hide_windows_for_saved_desks_grid();
  DCHECK(hide_windows);

  // Show the overview item window.
  if (item_widget_ &&
      hide_windows->HasWindow(item_widget_->GetNativeWindow())) {
    hide_windows->RemoveWindow(item_widget_->GetNativeWindow(),
                               /*show_window=*/true);
  }
}

}  // namespace ash
