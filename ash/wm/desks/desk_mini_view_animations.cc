// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_mini_view_animations.h"

#include <utility>

#include "ash/shell.h"
#include "ash/wm/desks/cros_next_desk_icon_button.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/legacy_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr gfx::Transform kEndTransform;

constexpr base::TimeDelta kExistingMiniViewsAnimationDuration =
    base::Milliseconds(250);
constexpr base::TimeDelta kExistingMiniViewsAnimationDurationCrOSNext =
    base::Milliseconds(150);

constexpr base::TimeDelta kRemovedMiniViewsFadeOutDuration =
    base::Milliseconds(200);

constexpr base::TimeDelta kZeroStateAnimationDuration = base::Milliseconds(200);

// Animation duration when feature flag `Jellyroll` is enabled.
constexpr base::TimeDelta kZeroStateAnimationDurationCrOSNext =
    base::Milliseconds(150);

// Animation durations for scale up and scale down the desk icon button.
constexpr base::TimeDelta kScaleUpDeskIconButton = base::Milliseconds(150);
constexpr base::TimeDelta kScaleDownDeskIconButton = base::Milliseconds(50);

// Scale for entering/exiting zero state.
constexpr float kEnterOrExitZeroStateScale = 0.6f;

// Animation durations for fade in the label below the desk icon button.
constexpr base::TimeDelta kLabelFadeInDelay = base::Milliseconds(100);
constexpr base::TimeDelta kLabelFadeInDuration = base::Milliseconds(50);

// |settings| will be initialized with a fast-out-slow-in animation with the
// given |duration|.
void InitScopedAnimationSettings(ui::ScopedLayerAnimationSettings* settings,
                                 base::TimeDelta duration) {
  settings->SetTransitionDuration(duration);
  const gfx::Tween::Type tween_type = chromeos::features::IsJellyrollEnabled()
                                          ? gfx::Tween::ACCEL_20_DECEL_100
                                          : gfx::Tween::ACCEL_20_DECEL_60;
  settings->SetTweenType(tween_type);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

// Animates the transform of the layer of the given |view| from the supplied
// |begin_transform| to the identity transform.
void AnimateView(views::View* view, const gfx::Transform& begin_transform) {
  ui::Layer* layer = view->layer();
  layer->SetTransform(begin_transform);

  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings,
                              chromeos::features::IsJellyrollEnabled()
                                  ? kExistingMiniViewsAnimationDurationCrOSNext
                                  : kExistingMiniViewsAnimationDuration);
  layer->SetTransform(kEndTransform);
}

// Note this function assumes that the given `view` is already set with its
// final visibility. If it's not visible, no need to fade it in. Return
// immediately instead.
void FadeInView(views::View* view,
                base::TimeDelta duration,
                base::TimeDelta delay) {
  if (!view->GetVisible()) {
    return;
  }

  auto* layer = view->layer();
  layer->SetOpacity(0.f);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .At(delay)
      .SetDuration(duration)
      .SetOpacity(layer, 1.f, gfx::Tween::ACCEL_20_DECEL_100);
}

// See details at AnimateView.
void AnimateMiniViews(std::vector<DeskMiniView*> mini_views,
                      const gfx::Transform& begin_transform) {
  for (auto* mini_view : mini_views) {
    AnimateView(mini_view, begin_transform);
  }
}

// Gets the scale transform for |view|, it can be scale up or scale down. The
// anchor of the scale animation will be a point whose |x| is the center of the
// desks bar while |y| is the top of the given |view|. GetMirroredX is used here
// to make sure the transform is correct while in RTL layout.
gfx::Transform GetScaleTransformForView(views::View* view, int bar_x_center) {
  return gfx::GetScaleTransform(
      gfx::Point(bar_x_center - view->GetMirroredX(), 0),
      kEnterOrExitZeroStateScale);
}

// Scales down the given |view| to |kEnterOrExitZeroStateScale| and fading out
// it at the same time.
void ScaleDownAndFadeOutView(views::View* view, int bar_x_center) {
  ui::Layer* layer = view->layer();
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings, kZeroStateAnimationDuration);

  layer->SetTransform(GetScaleTransformForView(view, bar_x_center));
  layer->SetOpacity(0.f);
}

// Scales up the given |view| from |kEnterOrExitZeroStateScale| to identity and
// fading in it at the same time.
void ScaleUpAndFadeInView(views::View* view, int bar_x_center) {
  DCHECK(view);
  ui::Layer* layer = view->layer();
  layer->SetTransform(GetScaleTransformForView(view, bar_x_center));
  layer->SetOpacity(0.f);

  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  const base::TimeDelta animation_duration =
      chromeos::features::IsJellyrollEnabled()
          ? kZeroStateAnimationDurationCrOSNext
          : kZeroStateAnimationDuration;
  InitScopedAnimationSettings(&settings, animation_duration);
  layer->SetTransform(kEndTransform);
  layer->SetOpacity(1.f);
}

void PositionWindowsInOverview() {
  auto* controller = Shell::Get()->overview_controller();
  DCHECK(controller->InOverviewSession());
  controller->overview_session()->PositionWindows(true);
}

void UpdateAccessibilityFocusInOverview() {
  auto* controller = Shell::Get()->overview_controller();
  DCHECK(controller->InOverviewSession());
  controller->overview_session()->UpdateAccessibilityFocus();
}

// A self-deleting object that performs a fade out animation on
// |removed_mini_view|'s layer by changing its opacity from 1 to 0 and scales
// down it around the center of |bar_view| while switching back to zero state.
// |removed_mini_view_| and the object itself will be deleted when the
// animation is complete.
// TODO(afakhry): Consider generalizing HidingWindowAnimationObserverBase to be
// reusable for the mini_view removal animation.
class RemovedMiniViewAnimation : public ui::ImplicitAnimationObserver {
 public:
  RemovedMiniViewAnimation(DeskMiniView* removed_mini_view,
                           LegacyDeskBarView* bar_view,
                           const bool to_zero_state)
      : removed_mini_view_(removed_mini_view), bar_view_(bar_view) {
    removed_mini_view_->set_is_animating_to_remove(true);
    ui::Layer* layer = removed_mini_view_->layer();
    ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
    InitScopedAnimationSettings(&settings, kRemovedMiniViewsFadeOutDuration);
    settings.AddObserver(this);

    if (to_zero_state) {
      DCHECK(bar_view);
      layer->SetTransform(GetScaleTransformForView(
          removed_mini_view, bar_view->bounds().CenterPoint().x()));
    } else {
      layer->SetTransform(kEndTransform);
    }
    layer->SetOpacity(0);
  }

  RemovedMiniViewAnimation(const RemovedMiniViewAnimation&) = delete;
  RemovedMiniViewAnimation& operator=(const RemovedMiniViewAnimation&) = delete;

  ~RemovedMiniViewAnimation() override {
    DCHECK(removed_mini_view_->parent());
    removed_mini_view_->parent()->RemoveChildViewT(removed_mini_view_.get());

    if (Shell::Get()->overview_controller()->InOverviewSession()) {
      DCHECK(bar_view_);
      bar_view_->UpdateDeskButtonsVisibility();
      UpdateAccessibilityFocusInOverview();
    }
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  raw_ptr<DeskMiniView, ExperimentalAsh> removed_mini_view_;
  const raw_ptr<LegacyDeskBarView, ExperimentalAsh> bar_view_;
};

// A self-deleting object that performs bounds changes animation for the desks
// bar while it switches between zero state and expanded state.
// `is_bounds_animation_on_going_` will be used to help hold Layout calls during
// the animation. Since Layout is expensive and will be called lots of times
// during the bounds changes animation without doing this. The object itself
// will be deleted when the animation is complete.
class DesksBarBoundsAnimation : public ui::ImplicitAnimationObserver {
 public:
  DesksBarBoundsAnimation(LegacyDeskBarView* bar_view, bool to_zero_state)
      : bar_view_(bar_view) {
    auto* desks_widget = bar_view_->GetWidget();
    const gfx::Rect current_widget_bounds =
        desks_widget->GetWindowBoundsInScreen();
    gfx::Rect target_widget_bounds = current_widget_bounds;
    // When `to_zero_state` is false, desks bar is switching from zero to
    // expanded state.
    if (to_zero_state) {
      target_widget_bounds.set_height(kDeskBarZeroStateHeight);

      if (chromeos::features::IsJellyrollEnabled()) {
        // When `Jellyroll` is enabled, setting desks bar's bounds to its bounds
        // at zero state directly to layout its contents at the correct position
        // first before the animation. When `Jellyroll` is enabled, we use the
        // same buttons (default desk button and library) for both expanded
        // state and zero state, the scale up and fade in animation is applied
        // to the buttona during the desks bar states transition, thus the
        // buttons need to be layout and put at the correct positions before the
        // animation starts.
        desks_widget->SetBounds(target_widget_bounds);
        bar_view_->set_is_bounds_animation_on_going(true);
        desks_widget->SetBounds(current_widget_bounds);
      } else {
        bar_view_->set_is_bounds_animation_on_going(true);
      }
    } else {
      // While switching desks bar from zero state to expanded state, setting
      // its bounds to its bounds at expanded state directly without animation,
      // which will trigger Layout and make sure the contents of
      // desks bar(e.g, desk mini view, new desk button) are at the correct
      // positions before the animation. And set `is_bounds_animation_on_going_`
      // to be true, which will help hold Layout until the animation is done.
      // Then set the bounds of the desks bar back to its bounds at zero state
      // to start the bounds change animation. See more details at
      // `is_bounds_animation_on_going_`.
      target_widget_bounds.set_height(DeskBarViewBase::GetPreferredBarHeight(
          desks_widget->GetNativeWindow()->GetRootWindow(),
          DeskBarViewBase::Type::kOverview, DeskBarViewBase::State::kExpanded));
      desks_widget->SetBounds(target_widget_bounds);
      bar_view_->set_is_bounds_animation_on_going(true);
      desks_widget->SetBounds(current_widget_bounds);
    }

    ui::ScopedLayerAnimationSettings settings{
        desks_widget->GetLayer()->GetAnimator()};
    const base::TimeDelta animation_duration =
        chromeos::features::IsJellyrollEnabled()
            ? kZeroStateAnimationDurationCrOSNext
            : kZeroStateAnimationDuration;
    InitScopedAnimationSettings(&settings, animation_duration);
    settings.AddObserver(this);
    desks_widget->SetBounds(target_widget_bounds);
  }

  DesksBarBoundsAnimation(const DesksBarBoundsAnimation&) = delete;
  DesksBarBoundsAnimation& operator=(const DesksBarBoundsAnimation&) = delete;

  ~DesksBarBoundsAnimation() override {
    DCHECK(bar_view_);
    bar_view_->set_is_bounds_animation_on_going(false);
    if (Shell::Get()->overview_controller()->InOverviewSession()) {
      // Updated the desk buttons and layout the desks bar to make sure the
      // buttons visibility will be updated on desks bar state changes. Also
      // make sure the button's text will be updated correctly while going back
      // to zero state.
      bar_view_->UpdateDeskButtonsVisibility();
      bar_view_->Layout();
      UpdateAccessibilityFocusInOverview();
    }
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  const raw_ptr<LegacyDeskBarView, ExperimentalAsh> bar_view_;
};

// A self-deleting class that performs the scale up / down animation for the
// desk icon button.
class DeskIconButtonScaleAnimation {
 public:
  DeskIconButtonScaleAnimation(CrOSNextDeskIconButton* button,
                               const gfx::Transform& scale_transform)
      : desk_icon_button_(button) {
    // Please note that since `this` is constructed after `desk_icon_button_` is
    // laid out in its final position, the target state is its current state.
    const CrOSNextDeskIconButton::State target_state =
        desk_icon_button_->state();
    const bool is_scale_up_animation =
        target_state == CrOSNextDeskIconButton::State::kActive;
    const gfx::RoundedCornersF initial_radius =
        gfx::RoundedCornersF(CrOSNextDeskIconButton::GetCornerRadiusOnState(
            is_scale_up_animation ? CrOSNextDeskIconButton::State::kExpanded
                                  : CrOSNextDeskIconButton::State::kActive));

    // Since the corner radius of `desk_icon_button_` is updated on the state
    // changes, to apply the animation for the corner radius change, set and
    // apply the corner radius animation on the layer, and set the solid
    // background (no corner radius) to the new desk button in the meanwhile. At
    // the end of the animation, set the layer's corner radius back to 0, and
    // apply the corner radius back to the background. The reason is that the
    // focus ring is painted on a layer which is a child of
    // `desk_icon_button_`'s layer. If `desk_icon_button_` has a clip rect, the
    // clip rect will affect it's children and the focus ring won't be visible.
    // Please refer to the `Layout` function of `FocusRing` for more
    // implementation details.
    auto* layer = desk_icon_button_->layer();
    layer->SetRoundedCornerRadius(initial_radius);
    desk_icon_button_->SetBackground(views::CreateSolidBackground(
        desk_icon_button_->background()->get_color()));

    layer->SetTransform(scale_transform);

    const auto duration = is_scale_up_animation ? kScaleUpDeskIconButton
                                                : kScaleDownDeskIconButton;
    const gfx::RoundedCornersF end_radius = gfx::RoundedCornersF(
        CrOSNextDeskIconButton::GetCornerRadiusOnState(target_state));
    views::AnimationBuilder()
        .OnEnded(base::BindOnce(
            [](DeskIconButtonScaleAnimation* animation) { delete animation; },
            base::Unretained(this)))
        .OnAborted(base::BindOnce(
            [](DeskIconButtonScaleAnimation* animation) { delete animation; },
            base::Unretained(this)))
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetDuration(duration)
        .SetRoundedCorners(layer, end_radius, gfx::Tween::ACCEL_20_DECEL_100)
        .SetTransform(layer, kEndTransform, gfx::Tween::ACCEL_20_DECEL_100);
  }

  DeskIconButtonScaleAnimation(const DeskIconButtonScaleAnimation&) = delete;
  DeskIconButtonScaleAnimation& operator=(const DeskIconButtonScaleAnimation&) =
      delete;

  ~DeskIconButtonScaleAnimation() {
    if (Shell::Get()->overview_controller()->InOverviewSession()) {
      desk_icon_button_->layer()->SetRoundedCornerRadius(
          gfx::RoundedCornersF());
      desk_icon_button_->SetBackground(views::CreateRoundedRectBackground(
          desk_icon_button_->background()->get_color(),
          CrOSNextDeskIconButton::GetCornerRadiusOnState(
              desk_icon_button_->state())));
    }
  }

 private:
  // `desk_icon_button_` is valid through the lifetime of `this `. Since when
  // the `desk_icon_button_` is destroyed, `OnAborted` will be triggered and
  // then the destructor of `this` will be triggered.
  const raw_ptr<CrOSNextDeskIconButton, ExperimentalAsh> desk_icon_button_;
};

}  // namespace

void PerformNewDeskMiniViewAnimation(
    LegacyDeskBarView* bar_view,
    std::vector<DeskMiniView*> new_mini_views,
    std::vector<DeskMiniView*> mini_views_left,
    std::vector<DeskMiniView*> mini_views_right,
    int shift_x) {
  if (chromeos::features::IsJellyrollEnabled()) {
    DCHECK(bar_view->new_desk_button());
  } else {
    DCHECK(bar_view->expanded_state_new_desk_button());
  }

  gfx::Transform mini_views_left_begin_transform;
  mini_views_left_begin_transform.Translate(shift_x, 0);
  gfx::Transform mini_views_right_begin_transform;
  mini_views_right_begin_transform.Translate(-shift_x, 0);

  for (auto* mini_view : new_mini_views) {
    if (chromeos::features::IsJellyrollEnabled()) {
      if (!mini_view->desk()->is_desk_being_removed()) {
        ScaleUpAndFadeInView(mini_view, mini_view->bounds().CenterPoint().x());
      }
    } else {
      ui::Layer* layer = mini_view->layer();
      layer->SetOpacity(0);

      if (!mini_view->desk()->is_desk_being_removed()) {
        layer->SetTransform(mini_views_left_begin_transform);
      }

      ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
      InitScopedAnimationSettings(&settings,
                                  kExistingMiniViewsAnimationDuration);
      layer->SetOpacity(1);
      layer->SetTransform(kEndTransform);
    }
  }

  AnimateMiniViews(mini_views_left, mini_views_left_begin_transform);
  AnimateMiniViews(mini_views_right, mini_views_right_begin_transform);

  // The new desk button and the library button in the expanded desks bar
  // always move to the right when a new desk is added.
  const auto& button_transform = base::i18n::IsRTL()
                                     ? mini_views_left_begin_transform
                                     : mini_views_right_begin_transform;
  if (chromeos::features::IsJellyrollEnabled()) {
    AnimateView(bar_view->new_desk_button(), button_transform);
    if (bar_view->new_desk_button_label()->GetVisible()) {
      AnimateView(bar_view->new_desk_button_label(), button_transform);
    }
    if (auto* library_button = bar_view->library_button()) {
      AnimateView(library_button, button_transform);
      if (bar_view->library_button_label()->GetVisible()) {
        AnimateView(bar_view->library_button_label(), button_transform);
      }
    }
  } else {
    AnimateView(bar_view->expanded_state_new_desk_button(), button_transform);
    if (auto* expanded_state_library_button =
            bar_view->expanded_state_library_button()) {
      AnimateView(expanded_state_library_button, button_transform);
    }
  }
}

void PerformRemoveDeskMiniViewAnimation(
    LegacyDeskBarView* bar_view,
    DeskMiniView* removed_mini_view,
    std::vector<DeskMiniView*> mini_views_left,
    std::vector<DeskMiniView*> mini_views_right,
    int shift_x) {
  gfx::Transform mini_views_left_begin_transform;
  mini_views_left_begin_transform.Translate(shift_x, 0);
  gfx::Transform mini_views_right_begin_transform;
  mini_views_right_begin_transform.Translate(-shift_x, 0);

  new RemovedMiniViewAnimation(removed_mini_view, bar_view,
                               /*to_zero_state=*/false);

  AnimateMiniViews(mini_views_left, mini_views_left_begin_transform);
  AnimateMiniViews(mini_views_right, mini_views_right_begin_transform);

  const auto& button_transform = base::i18n::IsRTL()
                                     ? mini_views_left_begin_transform
                                     : mini_views_right_begin_transform;
  if (chromeos::features::IsJellyrollEnabled()) {
    AnimateView(bar_view->new_desk_button(), button_transform);
    if (bar_view->new_desk_button_label()->GetVisible()) {
      AnimateView(bar_view->new_desk_button_label(), button_transform);
    }
    if (auto* library_button = bar_view->library_button()) {
      AnimateView(library_button, button_transform);
      if (bar_view->library_button_label()->GetVisible()) {
        AnimateView(bar_view->library_button_label(), button_transform);
      }
    }
  } else {
    AnimateView(bar_view->expanded_state_new_desk_button(), button_transform);
    if (auto* expanded_state_library_button =
            bar_view->expanded_state_library_button()) {
      AnimateView(expanded_state_library_button, button_transform);
    }
  }
}

void PerformZeroStateToExpandedStateMiniViewAnimation(
    LegacyDeskBarView* bar_view) {
  new DesksBarBoundsAnimation(bar_view, /*to_zero_state=*/false);
  const int bar_x_center = bar_view->bounds().CenterPoint().x();
  for (auto* mini_view : bar_view->mini_views())
    ScaleUpAndFadeInView(mini_view, bar_x_center);

  ScaleUpAndFadeInView(bar_view->expanded_state_new_desk_button(),
                       bar_x_center);
  if (auto* expanded_state_library_button =
          bar_view->expanded_state_library_button()) {
    ScaleUpAndFadeInView(expanded_state_library_button, bar_x_center);
  }
  PositionWindowsInOverview();
}

void PerformZeroStateToExpandedStateMiniViewAnimationCrOSNext(
    LegacyDeskBarView* bar_view) {
  bar_view->new_desk_button()->UpdateState(
      CrOSNextDeskIconButton::State::kExpanded);
  auto* library_button = bar_view->library_button();

  if (library_button) {
    if (bar_view->overview_grid()->WillShowSavedDeskLibrary()) {
      // For library button, when it's at zero state and clicked, the desks bar
      // will expand, the overview grid will show the saved desk library, the
      // library button should be activated and focused.
      library_button->UpdateState(CrOSNextDeskIconButton::State::kActive);
    } else {
      library_button->UpdateState(CrOSNextDeskIconButton::State::kExpanded);
    }
  }

  new DesksBarBoundsAnimation(bar_view, /*to_zero_state=*/false);

  const int bar_x_center = bar_view->bounds().CenterPoint().x();
  for (auto* mini_view : bar_view->mini_views()) {
    ScaleUpAndFadeInView(mini_view, bar_x_center);
  }

  ScaleUpAndFadeInView(bar_view->new_desk_button(), bar_x_center);
  if (library_button) {
    ScaleUpAndFadeInView(library_button, bar_x_center);
    // Library button could be at active state when the desks bar is switched
    // from the zero state to the expanded state, for example when clicking on
    // the library button at zero state. Thus we should also try to fade in the
    // library button label here.
    FadeInView(bar_view->library_button_label(),
               /*duration=*/kLabelFadeInDuration,
               /*delay=*/kLabelFadeInDelay);
  }

  PositionWindowsInOverview();
}

void PerformExpandedStateToZeroStateMiniViewAnimation(
    LegacyDeskBarView* bar_view,
    std::vector<DeskMiniView*> removed_mini_views) {
  for (auto* mini_view : removed_mini_views)
    new RemovedMiniViewAnimation(mini_view, bar_view, /*to_zero_state=*/true);
  new DesksBarBoundsAnimation(bar_view, /*to_zero_state=*/true);
  const gfx::Rect bounds = bar_view->bounds();
  ScaleDownAndFadeOutView(bar_view->expanded_state_new_desk_button(),
                          bounds.CenterPoint().x());
  if (auto* expanded_state_library_button =
          bar_view->expanded_state_library_button()) {
    ScaleDownAndFadeOutView(expanded_state_library_button,
                            bounds.CenterPoint().x());
  }

  PositionWindowsInOverview();
}

void PerformReorderDeskMiniViewAnimation(
    int old_index,
    int new_index,
    const std::vector<DeskMiniView*>& mini_views) {
  const int views_size = static_cast<int>(mini_views.size());

  DCHECK_GE(old_index, 0);
  DCHECK_LT(old_index, views_size);
  DCHECK_GE(new_index, 0);
  DCHECK_LT(new_index, views_size);

  if (old_index == new_index)
    return;

  // Reordering should be finished before calling this function. The source view
  // and the target view has been exchanged. The range should be selected
  // according to current mini views position.
  const bool move_right = old_index < new_index;
  const int start_index = move_right ? old_index : new_index + 1;
  const int end_index = move_right ? new_index : old_index + 1;

  // Since |old_index| and |new_index| are unequal valid indices, there
  // must be at least two desks.
  int shift_x = mini_views[0]->GetMirroredBounds().x() -
                mini_views[1]->GetMirroredBounds().x();
  shift_x = move_right ? -shift_x : shift_x;
  gfx::Transform desks_transform;
  desks_transform.Translate(shift_x, 0);

  auto start_iter = mini_views.begin();
  AnimateMiniViews(std::vector<DeskMiniView*>(start_iter + start_index,
                                              start_iter + end_index),
                   desks_transform);

  // Animate the mini view being reordered if it is visible.
  auto* reorder_view = mini_views[new_index];
  ui::Layer* layer = reorder_view->layer();
  if (layer->opacity() == 0.0f)
    return;

  // Back to old position.
  gfx::Transform reorder_desk_transform;
  reorder_desk_transform.Translate(
      mini_views[old_index]->GetMirroredBounds().x() -
          reorder_view->GetMirroredBounds().x(),
      0);
  layer->SetTransform(reorder_desk_transform);

  // Animate movement.
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  InitScopedAnimationSettings(&settings, kExistingMiniViewsAnimationDuration);
  layer->SetTransform(kEndTransform);
}

void PerformLibraryButtonVisibilityAnimation(
    const std::vector<DeskMiniView*>& mini_views,
    views::View* new_desk_button,
    int shift_x) {
  gfx::Transform translation;
  translation.Translate(shift_x, 0);
  AnimateMiniViews(mini_views, translation);
  AnimateView(new_desk_button, translation);
}

void PerformDeskIconButtonScaleAnimationCrOSNext(
    CrOSNextDeskIconButton* button,
    LegacyDeskBarView* bar_view,
    const gfx::Transform& new_desk_button_rects_transform,
    int shift_x) {
  new DeskIconButtonScaleAnimation(button, new_desk_button_rects_transform);

  gfx::Transform left_begin_transform;
  left_begin_transform.Translate(shift_x, 0);
  gfx::Transform right_begin_transform;
  right_begin_transform.Translate(-shift_x, 0);

  AnimateMiniViews(bar_view->mini_views(), left_begin_transform);

  // If `button` is the new desk button, shift the library button to the right.
  // Otherwise if it's the library button, shift the new desk button to the
  // left.
  if (button == bar_view->new_desk_button()) {
    if (auto* library_button = bar_view->library_button()) {
      AnimateView(library_button, right_begin_transform);
      FadeInView(bar_view->new_desk_button_label(),
                 /*duration=*/kLabelFadeInDuration,
                 /*delay=*/kLabelFadeInDelay);
    }
  } else {
    AnimateView(bar_view->new_desk_button(), left_begin_transform);
    FadeInView(bar_view->library_button_label(),
               /*duration=*/kLabelFadeInDuration,
               /*delay=*/kLabelFadeInDelay);
  }
}

}  // namespace ash
