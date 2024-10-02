// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/tablet_state.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/caption_button_model.h"
#include "chromeos/ui/frame/caption_buttons/frame_size_button.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/events/event_sink.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"  // Accessibility names
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_types.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/views/window/vector_icons/vector_icons.h"

namespace chromeos {

namespace {

// Duration of the animation of the position of buttons to the left of
// |size_button_|.
constexpr auto kPositionAnimationDuration = base::Milliseconds(500);

// Duration of the animation of the alpha of |size_button_|.
constexpr auto kAlphaAnimationDuration = base::Milliseconds(250);

// Delay during |tablet_mode_animation_| hide to wait before beginning to
// animate the position of buttons to the left of |size_button_|.
constexpr auto kHidePositionDelay = base::Milliseconds(100);

// Duration of |tablet_mode_animation_| hiding.
// Hiding size button 250
// |------------------------|
// Delay 100      Slide other buttons 500
// |---------|-------------------------------------------------|
constexpr auto kHideAnimationDuration =
    kHidePositionDelay + kPositionAnimationDuration;

// Delay during |tablet_mode_animation_| show to wait before beginning to
// animate the alpha of |size_button_|.
constexpr auto kShowAnimationAlphaDelay = base::Milliseconds(100);

// Duration of |tablet_mode_animation_| showing.
// Slide other buttons 500
// |-------------------------------------------------|
// Delay 100   Show size button 250
// |---------|-----------------------|
constexpr auto kShowAnimationDuration = kPositionAnimationDuration;

// Value of |tablet_mode_animation_| showing to begin animating alpha of
// |size_button_|.
float SizeButtonShowStartValue() {
  return kShowAnimationAlphaDelay / kShowAnimationDuration;
}

// Amount of |tablet_mode_animation_| showing to animate the alpha of
// |size_button_|.
float SizeButtonShowDuration() {
  return kAlphaAnimationDuration / kShowAnimationDuration;
}

// Amount of |tablet_mode_animation_| hiding to animate the alpha of
// |size_button_|.
float SizeButtonHideDuration() {
  return kAlphaAnimationDuration / kHideAnimationDuration;
}

// Value of |tablet_mode_animation_| hiding to begin animating the position of
// buttons to the left of |size_button_|.
float HidePositionStartValue() {
  return 1.0f - kHidePositionDelay / kHideAnimationDuration;
}

// Bounds animation values to the range 0.0 - 1.0. Allows for mapping of offset
// animations to the expected range so that gfx::Tween::CalculateValue() can be
// used.
double CapAnimationValue(double value) {
  return std::clamp(value, 0.0, 1.0);
}

// A default CaptionButtonModel that uses the widget delegate's state
// to determine if each button should be visible and enabled.
class DefaultCaptionButtonModel : public CaptionButtonModel {
 public:
  explicit DefaultCaptionButtonModel(views::Widget* frame) : frame_(frame) {}
  DefaultCaptionButtonModel(const DefaultCaptionButtonModel&) = delete;
  DefaultCaptionButtonModel& operator=(const DefaultCaptionButtonModel&) =
      delete;
  ~DefaultCaptionButtonModel() override {}

  // CaptionButtonModel:
  bool IsVisible(views::CaptionButtonIcon type) const override {
    switch (type) {
      case views::CAPTION_BUTTON_ICON_MINIMIZE:
        return frame_->widget_delegate()->CanMinimize() &&
               !TabletState::Get()->InTabletMode();
      case views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE: {
        if (!frame_->widget_delegate()->CanMaximize()) {
          return false;
        }

        if (!TabletState::Get()->InTabletMode()) {
          return true;
        }

        // In tablet mode, only show the size button if the window is floated.
        return frame_->GetNativeWindow()->GetProperty(kWindowStateTypeKey) ==
               WindowStateType::kFloated;
      }
      // Resizable widget can be snapped.
      case views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED:
      case views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED:
        return frame_->widget_delegate()->CanResize();
      case views::CAPTION_BUTTON_ICON_CLOSE:
        return frame_->widget_delegate()->ShouldShowCloseButton();
      case views::CAPTION_BUTTON_ICON_CUSTOM:
        return true;
      case views::CAPTION_BUTTON_ICON_FLOAT: {
        if (!chromeos::wm::features::IsWindowLayoutMenuEnabled() ||
            !frame_->IsNativeWidgetInitialized()) {
          return false;
        }
        if (chromeos::TabletState::Get()->InTabletMode()) {
          return false;
        }
        // Only need to show the float button for apps that normally can't be
        // resized.
        if (IsVisible(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE)) {
          return false;
        }
        return chromeos::wm::CanFloatWindow(frame_->GetNativeWindow());
      }
      // No back or menu button by default.
      case views::CAPTION_BUTTON_ICON_BACK:
      case views::CAPTION_BUTTON_ICON_MENU:
      case views::CAPTION_BUTTON_ICON_ZOOM:
      case views::CAPTION_BUTTON_ICON_CENTER:
        return false;
      case views::CAPTION_BUTTON_ICON_LOCATION:
        // not used
        return false;
      case views::CAPTION_BUTTON_ICON_COUNT:
        break;
    }
    NOTREACHED();
    return false;
  }
  bool IsEnabled(views::CaptionButtonIcon type) const override { return true; }
  bool InZoomMode() const override { return false; }

 private:
  raw_ptr<views::Widget> frame_;
};

}  // namespace

FrameCaptionButtonContainerView::FrameCaptionButtonContainerView(
    views::Widget* frame,
    std::unique_ptr<views::FrameCaptionButton> custom_button)
    : views::AnimationDelegateViews(frame->GetRootView()),
      frame_(frame),
      model_(std::make_unique<DefaultCaptionButtonModel>(frame)) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  tablet_mode_animation_ = std::make_unique<gfx::SlideAnimation>(this);
  tablet_mode_animation_->SetTweenType(gfx::Tween::LINEAR);

  // Ensure animation tracks visibility of size button.
  if (model_->IsVisible(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE) ||
      model_->InZoomMode()) {
    tablet_mode_animation_->Reset(1.0f);
  }

  // Insert the buttons left to right.
  if (custom_button)
    custom_button_ = AddChildView(std::move(custom_button));

  menu_button_ = new views::FrameCaptionButton(
      base::BindRepeating(&FrameCaptionButtonContainerView::MenuButtonPressed,
                          base::Unretained(this)),
      views::CAPTION_BUTTON_ICON_MENU, HTMENU);
  menu_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MENU));
  AddChildView(menu_button_.get());

  minimize_button_ = new views::FrameCaptionButton(
      base::BindRepeating(
          &FrameCaptionButtonContainerView::MinimizeButtonPressed,
          base::Unretained(this)),
      views::CAPTION_BUTTON_ICON_MINIMIZE, HTMINBUTTON);
  minimize_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));
  AddChildView(minimize_button_.get());

  if (chromeos::wm::features::IsWindowLayoutMenuEnabled()) {
    float_button_ = AddChildView(std::make_unique<views::FrameCaptionButton>(
        base::BindRepeating(
            &FrameCaptionButtonContainerView::FloatButtonPressed,
            base::Unretained(this)),
        views::CAPTION_BUTTON_ICON_FLOAT, HTMENU));
    float_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_MULTITASK_MENU_FLOAT_BUTTON_NAME));
  }

  size_button_ = new FrameSizeButton(
      base::BindRepeating(&FrameCaptionButtonContainerView::SizeButtonPressed,
                          base::Unretained(this)),
      this);
  size_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MAXIMIZE));
  AddChildView(size_button_.get());

  close_button_ = new views::FrameCaptionButton(
      base::BindRepeating(&FrameCaptionButtonContainerView::CloseButtonPressed,
                          base::Unretained(this)),
      views::CAPTION_BUTTON_ICON_CLOSE, HTCLOSE);
  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  AddChildView(close_button_.get());

  SetButtonImage(views::CAPTION_BUTTON_ICON_FLOAT,
                 chromeos::kWindowControlFloatIcon);
  // TODO(hewer): Resolve this so two float icons are no longer needed.
  SetButtonImage(views::CAPTION_BUTTON_ICON_MENU, chromeos::kFloatWindowIcon);
  SetButtonImage(views::CAPTION_BUTTON_ICON_MINIMIZE,
                 views::kWindowControlMinimizeIcon);
  SetButtonImage(views::CAPTION_BUTTON_ICON_CLOSE,
                 views::kWindowControlCloseIcon);

  // The float button relies on minimum size to know if it can be floated, which
  // can only be checked after the widget has been initialized.
  if (frame->IsNativeWidgetInitialized()) {
    UpdateCaptionButtonState(/*animate=*/false);
  } else {
    frame->widget_delegate()->RegisterWidgetInitializedCallback(base::BindOnce(
        &FrameCaptionButtonContainerView::UpdateCaptionButtonState,
        // base::Unretained is safe as the container and widget delegate are
        // destroyed when the widget is destroyed.
        base::Unretained(this), /*animate=*/false));
  }

  UpdateCaptionButtonState(/*animate=*/false);

  frame_observer_.Observe(frame_);
}

FrameCaptionButtonContainerView::~FrameCaptionButtonContainerView() = default;

void FrameCaptionButtonContainerView::TestApi::EndAnimations() {
  container_view_->tablet_mode_animation_->End();
}

void FrameCaptionButtonContainerView::SetPaintAsActive(bool paint_as_active) {
  if (custom_button_) {
    custom_button_->SetPaintAsActive(paint_as_active);
  }
  if (float_button_) {
    float_button_->SetPaintAsActive(paint_as_active);
  }
  menu_button_->SetPaintAsActive(paint_as_active);
  minimize_button_->SetPaintAsActive(paint_as_active);
  size_button_->SetPaintAsActive(paint_as_active);
  close_button_->SetPaintAsActive(paint_as_active);
  SchedulePaint();
}

void FrameCaptionButtonContainerView::SetButtonImage(
    views::CaptionButtonIcon icon,
    const gfx::VectorIcon& icon_definition) {
  button_icon_map_[icon] = &icon_definition;
  views::FrameCaptionButton* buttons[] = {menu_button_, minimize_button_,
                                          size_button_, float_button_,
                                          close_button_};
  for (views::FrameCaptionButton* button : buttons) {
    if (button && button->GetIcon() == icon) {
      button->SetImage(icon, views::FrameCaptionButton::Animate::kNo,
                       icon_definition);
    }
  }
}

void FrameCaptionButtonContainerView::SetBackgroundColor(
    SkColor background_color) {
  if (custom_button_) {
    custom_button_->SetBackgroundColor(background_color);
  }
  if (float_button_) {
    float_button_->SetBackgroundColor(background_color);
  }
  menu_button_->SetBackgroundColor(background_color);
  minimize_button_->SetBackgroundColor(background_color);
  size_button_->SetBackgroundColor(background_color);
  close_button_->SetBackgroundColor(background_color);

  // When buttons' background color changes, the entire view's background color
  // changes if WCO is enabled.
  if (window_controls_overlay_enabled_) {
    SetBackground(views::CreateSolidBackground(background_color));
  }
}

void FrameCaptionButtonContainerView::ResetWindowControls() {
  SetButtonsToNormal(Animate::kNo);
}

void FrameCaptionButtonContainerView::OnWindowControlsOverlayEnabledChanged(
    bool enabled,
    SkColor background_color) {
  window_controls_overlay_enabled_ = enabled;
  if (enabled) {
    SetBackground(views::CreateSolidBackground(background_color));
    // The view needs to paint to a layer so that it is painted on top of the
    // web content.
    SetPaintToLayer();
  } else {
    SetBackground(nullptr);
    DestroyLayer();
  }
}

void FrameCaptionButtonContainerView::UpdateBorderlessModeEnabled(
    bool enabled) {
  if (is_borderless_mode_enabled_ == enabled)
    return;

  // In borderless mode, the windowing controls will be drawn in web content,
  // so similarly to hiding the title bar, also the caption button container
  // containing them will be hidden.
  is_borderless_mode_enabled_ = enabled;
  SetVisible(enabled);
}

void FrameCaptionButtonContainerView::UpdateCaptionButtonState(bool animate) {
  bool size_button_visible =
      (model_->IsVisible(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE) ||
       model_->InZoomMode());
  if (size_button_visible) {
    size_button_->SetVisible(true);
    if (animate) {
      tablet_mode_animation_->SetSlideDuration(kShowAnimationDuration);
      tablet_mode_animation_->Show();
    }
  } else {
    if (animate) {
      tablet_mode_animation_->SetSlideDuration(kHideAnimationDuration);
      tablet_mode_animation_->Hide();
    } else {
      size_button_->SetVisible(false);
    }
  }
  if (custom_button_) {
    custom_button_->SetEnabled(
        model_->IsEnabled(views::CAPTION_BUTTON_ICON_CUSTOM));
    custom_button_->SetVisible(
        model_->IsVisible(views::CAPTION_BUTTON_ICON_CUSTOM));
  }
  if (float_button_) {
    float_button_->SetEnabled(
        model_->IsEnabled(views::CAPTION_BUTTON_ICON_FLOAT));
    float_button_->SetVisible(
        model_->IsVisible(views::CAPTION_BUTTON_ICON_FLOAT));
  }
  size_button_->SetEnabled(
      (model_->IsEnabled(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE) ||
       model_->InZoomMode()));
  minimize_button_->SetVisible(
      model_->IsVisible(views::CAPTION_BUTTON_ICON_MINIMIZE));
  minimize_button_->SetEnabled(
      model_->IsEnabled(views::CAPTION_BUTTON_ICON_MINIMIZE));
  menu_button_->SetVisible(model_->IsVisible(views::CAPTION_BUTTON_ICON_MENU));
  menu_button_->SetEnabled(model_->IsEnabled(views::CAPTION_BUTTON_ICON_MENU));
  close_button_->SetVisible(
      model_->IsVisible(views::CAPTION_BUTTON_ICON_CLOSE));
}

void FrameCaptionButtonContainerView::UpdateButtonsImageAndTooltip() {
  UpdateSizeButton();
  UpdateSnapButtons();
  UpdateFloatButton();
}

void FrameCaptionButtonContainerView::SetButtonSize(const gfx::Size& size) {
  if (custom_button_) {
    custom_button_->SetPreferredSize(size);
  }
  if (float_button_) {
    float_button_->SetPreferredSize(size);
  }
  menu_button_->SetPreferredSize(size);
  minimize_button_->SetPreferredSize(size);
  size_button_->SetPreferredSize(size);
  if (features::IsJellyrollEnabled()) {
    // When feature Jellyroll is enabled, make the target width of close button
    // 8 DIP wider on the right side than other caption buttons.
    constexpr int kExtraTargetSpaceForCloseButton = 8;
    close_button_->SetPreferredSize(gfx::Size(
        size.width() + kExtraTargetSpaceForCloseButton, size.height()));
    // Since we want the space between the caption buttons to remain the same,
    // the extra space for the close button should be added to the right side of
    // it if RTL is disabled, otherwise the extra space should be added to the
    // left side.
    close_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        0, base::i18n::IsRTL() ? kExtraTargetSpaceForCloseButton : 0, 0,
        base::i18n::IsRTL() ? 0 : kExtraTargetSpaceForCloseButton)));
  } else {
    close_button_->SetPreferredSize(size);
  }

  SetMinimumCrossAxisSize(size.height());
}

void FrameCaptionButtonContainerView::SetModel(
    std::unique_ptr<CaptionButtonModel> model) {
  model_ = std::move(model);
}

void FrameCaptionButtonContainerView::SetOnSizeButtonPressedCallback(
    base::RepeatingCallback<bool()> callback) {
  on_size_button_pressed_callback_ = std::move(callback);
}

void FrameCaptionButtonContainerView::ClearOnSizeButtonPressedCallback() {
  on_size_button_pressed_callback_.Reset();
}

void FrameCaptionButtonContainerView::Layout() {
  views::View::Layout();

  // This ensures that the first frame of the animation to show the size button
  // pushes the buttons to the left of the size button into the center.
  if (tablet_mode_animation_->is_animating())
    AnimationProgressed(tablet_mode_animation_.get());

#if DCHECK_IS_ON()
  if (close_button_->GetVisible()) {
    // The top right corner must be occupied by the close button for easy mouse
    // access. This check is agnostic to RTL layout.
    DCHECK_EQ(close_button_->y(), 0);
    DCHECK_EQ(close_button_->bounds().right(), width());
  }
#endif  // DCHECK_IS_ON()
}

void FrameCaptionButtonContainerView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

void FrameCaptionButtonContainerView::ChildVisibilityChanged(View* child) {
  PreferredSizeChanged();
}

void FrameCaptionButtonContainerView::AnimationEnded(
    const gfx::Animation* animation) {
  // Ensure that position is calculated at least once.
  AnimationProgressed(animation);

  double current_value = tablet_mode_animation_->GetCurrentValue();
  if (current_value == 0.0)
    size_button_->SetVisible(false);
}

void FrameCaptionButtonContainerView::AnimationProgressed(
    const gfx::Animation* animation) {
  double current_value = animation->GetCurrentValue();
  int size_alpha = 0;
  int x_slide = 0;
  if (tablet_mode_animation_->IsShowing()) {
    double scaled_value_alpha =
        CapAnimationValue((current_value - SizeButtonShowStartValue()) /
                          SizeButtonShowDuration());
    double tweened_value_alpha =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, scaled_value_alpha);
    size_alpha = gfx::Tween::LinearIntValueBetween(tweened_value_alpha, 0, 255);

    double tweened_value_slide =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, current_value);
    x_slide = gfx::Tween::LinearIntValueBetween(tweened_value_slide,
                                                size_button_->width(), 0);
  } else {
    double scaled_value_alpha =
        CapAnimationValue((1.0f - current_value) / SizeButtonHideDuration());
    double tweened_value_alpha =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_IN, scaled_value_alpha);
    size_alpha = gfx::Tween::LinearIntValueBetween(tweened_value_alpha, 255, 0);

    double scaled_value_position = CapAnimationValue(
        (HidePositionStartValue() - current_value) / HidePositionStartValue());
    double tweened_value_slide =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, scaled_value_position);
    x_slide = gfx::Tween::LinearIntValueBetween(tweened_value_slide, 0,
                                                size_button_->width());
  }
  size_button_->SetAlpha(size_alpha);

  // Slide all buttons to the left of the size button. Usually this is just the
  // minimize button but it can also include a PWA menu button.
  int previous_x = 0;
  for (auto* button : children()) {
    if (button == size_button_)
      break;
    button->SetX(previous_x + x_slide);
    previous_x += button->width();
  }
}

void FrameCaptionButtonContainerView::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (!active) {
    return;
  }

  // Tablet nudge is controlled by ash by another class
  // (`::ash::TabletModeMultitaskCue`).
  if (TabletState::Get()->InTabletMode()) {
    return;
  }

  nudge_controller_.MaybeShowNudge(
      widget->GetNativeWindow(),
      /*anchor_view=*/static_cast<views::View*>(size_button_));
}

void FrameCaptionButtonContainerView::SetButtonIcon(
    views::FrameCaptionButton* button,
    views::CaptionButtonIcon icon,
    Animate animate) {
  // The early return is dependent on |animate| because callers use
  // SetButtonIcon() with Animate::kNo to progress |button|'s crossfade
  // animation to the end.
  if (button->GetIcon() == icon &&
      (animate == Animate::kYes || !button->IsAnimatingImageSwap())) {
    return;
  }

  views::FrameCaptionButton::Animate fcb_animate =
      (animate == Animate::kYes) ? views::FrameCaptionButton::Animate::kYes
                                 : views::FrameCaptionButton::Animate::kNo;
  auto it = button_icon_map_.find(icon);
  if (it != button_icon_map_.end())
    button->SetImage(icon, fcb_animate, *it->second);
}

void FrameCaptionButtonContainerView::UpdateSizeButton() {
  const bool use_zoom_icons = model_->InZoomMode();
  const bool floated = frame_->GetNativeWindow()->GetProperty(
                           kWindowStateTypeKey) == WindowStateType::kFloated;

  const gfx::VectorIcon& restore_icon = use_zoom_icons
                                            ? chromeos::kWindowControlDezoomIcon
                                            : views::kWindowControlRestoreIcon;
  const gfx::VectorIcon& maximize_icon =
      use_zoom_icons ? chromeos::kWindowControlZoomIcon
                     : (floated ? chromeos::kUnfloatButtonIcon
                                : views::kWindowControlMaximizeIcon);

  const bool use_restore_frame = chromeos::ShouldUseRestoreFrame(frame_);
  SetButtonImage(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
                 use_restore_frame ? maximize_icon : restore_icon);

  int message_id;
  if (floated) {
    message_id = IDS_MULTITASK_MENU_EXIT_FLOAT_BUTTON_NAME;
  } else if (use_restore_frame) {
    message_id = IDS_APP_ACCNAME_MAXIMIZE;
  } else {
    message_id = IDS_APP_ACCNAME_RESTORE;
  }
  size_button_->SetTooltipText(l10n_util::GetStringUTF16(message_id));

  // Size button also needs to update its visibility when float state changes.
  UpdateCaptionButtonState(/*animate=*/true);
  size_button_->SetEnabled(
      model_->IsEnabled(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE) ||
      use_zoom_icons);

  // Alpha may be not fully opaque from a previous tablet mode animation.
  if (size_button_->GetVisible()) {
    size_button_->SetAlpha(255);
  }
}

void FrameCaptionButtonContainerView::UpdateSnapButtons() {
  const bool is_horizontal_display = chromeos::IsDisplayLayoutHorizontal(
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          frame_->GetNativeWindow()));
  SetButtonImage(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
                 is_horizontal_display
                     ? chromeos::kWindowControlLeftSnappedIcon
                     : chromeos::kWindowControlTopSnappedIcon);
  SetButtonImage(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
                 is_horizontal_display
                     ? chromeos::kWindowControlRightSnappedIcon
                     : chromeos::kWindowControlBottomSnappedIcon);
}

void FrameCaptionButtonContainerView::UpdateFloatButton() {
  if (!float_button_) {
    return;
  }

  const bool floated = frame_->GetNativeWindow()->GetProperty(
                           kWindowStateTypeKey) == WindowStateType::kFloated;
  SetButtonImage(views::CAPTION_BUTTON_ICON_FLOAT,
                 floated ? chromeos::kWindowControlUnfloatIcon
                         : chromeos::kWindowControlFloatIcon);
  float_button_->SetTooltipText(l10n_util::GetStringUTF16(
      floated ? IDS_MULTITASK_MENU_EXIT_FLOAT_BUTTON_NAME
              : IDS_MULTITASK_MENU_FLOAT_BUTTON_NAME));

  // Float button also needs to update its visibility when float state changes.
  float_button_->SetEnabled(
      model_->IsEnabled(views::CAPTION_BUTTON_ICON_FLOAT));
  float_button_->SetVisible(
      model_->IsVisible(views::CAPTION_BUTTON_ICON_FLOAT));
}

void FrameCaptionButtonContainerView::MinimizeButtonPressed() {
  // Abort any animations of the button icons.
  SetButtonsToNormal(Animate::kNo);

  frame_->Minimize();
  base::RecordAction(base::UserMetricsAction("MinButton_Clk"));
}

void FrameCaptionButtonContainerView::SizeButtonPressed() {
  // Abort any animations of the button icons.
  SetButtonsToNormal(Animate::kNo);

  if (on_size_button_pressed_callback_ &&
      on_size_button_pressed_callback_.Run()) {
    // no-op if the override callback returned true.
  } else if (frame_->IsFullscreen()) {
    // Can be clicked in immersive fullscreen.
    frame_->Restore();
    base::RecordAction(base::UserMetricsAction("MaxButton_Clk_ExitFS"));
  } else if (frame_->IsMaximized()) {
    frame_->Restore();
    base::RecordAction(base::UserMetricsAction("MaxButton_Clk_Restore"));
  } else if (frame_->GetNativeWindow()->GetProperty(kWindowStateTypeKey) ==
             WindowStateType::kFloated) {
    FloatControllerBase::Get()->ToggleFloat(frame_->GetNativeWindow());
  } else {
    frame_->Maximize();
    base::RecordAction(base::UserMetricsAction("MaxButton_Clk_Maximize"));
  }
}

void FrameCaptionButtonContainerView::CloseButtonPressed() {
  // Abort any animations of the button icons.
  SetButtonsToNormal(Animate::kNo);

  frame_->Close();
  if (TabletState::Get()->InTabletMode()) {
    base::RecordAction(
        base::UserMetricsAction("Tablet_WindowCloseFromCaptionButton"));
  } else {
    base::RecordAction(base::UserMetricsAction("CloseButton_Clk"));
  }
}

void FrameCaptionButtonContainerView::MenuButtonPressed() {
  // Abort any animations of the button icons.
  SetButtonsToNormal(Animate::kNo);

  // Send up event as well as down event as ARC++ clients expect this sequence.
  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::KeyEvent press_key_event(ui::ET_KEY_PRESSED, ui::VKEY_APPS, ui::EF_NONE);
  std::ignore = root_window->GetHost()->GetEventSink()->OnEventFromSource(
      &press_key_event);
  ui::KeyEvent release_key_event(ui::ET_KEY_RELEASED, ui::VKEY_APPS,
                                 ui::EF_NONE);
  std::ignore = root_window->GetHost()->GetEventSink()->OnEventFromSource(
      &release_key_event);
  // TODO(oshima): Add metrics
}

// TODO(b/279198474): Fix the delay that occurs the first time the button is
// pressed.
void FrameCaptionButtonContainerView::FloatButtonPressed() {
  // Abort any animations of the button icons.
  SetButtonsToNormal(Animate::kNo);
  CHECK(chromeos::wm::features::IsWindowLayoutMenuEnabled());

  FloatControllerBase::Get()->ToggleFloat(GetWidget()->GetNativeWindow());
}

bool FrameCaptionButtonContainerView::IsMinimizeButtonVisible() const {
  return minimize_button_->GetVisible();
}

void FrameCaptionButtonContainerView::SetButtonsToNormal(Animate animate) {
  SetButtonIcons(views::CAPTION_BUTTON_ICON_MINIMIZE,
                 views::CAPTION_BUTTON_ICON_CLOSE, animate);
  if (custom_button_) {
    custom_button_->SetState(views::Button::STATE_NORMAL);
  }
  if (float_button_) {
    float_button_->SetState(views::Button::STATE_NORMAL);
  }
  menu_button_->SetState(views::Button::STATE_NORMAL);
  minimize_button_->SetState(views::Button::STATE_NORMAL);
  size_button_->SetState(views::Button::STATE_NORMAL);
  close_button_->SetState(views::Button::STATE_NORMAL);
}

void FrameCaptionButtonContainerView::SetButtonIcons(
    views::CaptionButtonIcon minimize_button_icon,
    views::CaptionButtonIcon close_button_icon,
    Animate animate) {
  SetButtonIcon(minimize_button_, minimize_button_icon, animate);
  SetButtonIcon(close_button_, close_button_icon, animate);
}

const views::FrameCaptionButton*
FrameCaptionButtonContainerView::GetButtonClosestTo(
    const gfx::Point& position_in_screen) const {
  // Since the buttons all have the same size, the closest button is the button
  // with the center point closest to |position_in_screen|.
  // TODO(pkotwicz): Make the caption buttons not overlap.
  gfx::Point position(position_in_screen);
  views::View::ConvertPointFromScreen(this, &position);

  views::FrameCaptionButton* buttons[] = {custom_button_,   menu_button_,
                                          minimize_button_, size_button_,
                                          float_button_,    close_button_};
  int min_squared_distance = INT_MAX;
  views::FrameCaptionButton* closest_button = nullptr;
  for (size_t i = 0; i < std::size(buttons); ++i) {
    views::FrameCaptionButton* button = buttons[i];
    if (!button || !button->GetVisible())
      continue;

    gfx::Point center_point = button->GetLocalBounds().CenterPoint();
    views::View::ConvertPointToTarget(button, this, &center_point);
    int squared_distance = static_cast<int>(
        pow(static_cast<double>(position.x() - center_point.x()), 2) +
        pow(static_cast<double>(position.y() - center_point.y()), 2));
    if (squared_distance < min_squared_distance) {
      min_squared_distance = squared_distance;
      closest_button = button;
    }
  }
  return closest_button;
}

void FrameCaptionButtonContainerView::SetHoveredAndPressedButtons(
    const views::FrameCaptionButton* to_hover,
    const views::FrameCaptionButton* to_press) {
  views::FrameCaptionButton* buttons[] = {custom_button_,   menu_button_,
                                          minimize_button_, size_button_,
                                          float_button_,    close_button_};
  for (views::FrameCaptionButton* button : buttons) {
    if (!button)
      continue;
    views::Button::ButtonState new_state = views::Button::STATE_NORMAL;
    if (button == to_hover)
      new_state = views::Button::STATE_HOVERED;
    else if (button == to_press)
      new_state = views::Button::STATE_PRESSED;
    button->SetState(new_state);
  }
}

bool FrameCaptionButtonContainerView::CanSnap() {
  return SnapController::Get()->CanSnap(frame_->GetNativeWindow());
}

void FrameCaptionButtonContainerView::ShowSnapPreview(
    SnapDirection snap,
    bool allow_haptic_feedback) {
  SnapController::Get()->ShowSnapPreview(frame_->GetNativeWindow(), snap,
                                         allow_haptic_feedback);
}

void FrameCaptionButtonContainerView::CommitSnap(SnapDirection snap) {
  SnapController::Get()->CommitSnap(frame_->GetNativeWindow(), snap,
                                    kDefaultSnapRatio);
}

MultitaskMenuNudgeController*
FrameCaptionButtonContainerView::GetMultitaskMenuNudgeController() {
  return &nudge_controller_;
}

BEGIN_METADATA(FrameCaptionButtonContainerView, views::View)
END_METADATA

}  // namespace chromeos
