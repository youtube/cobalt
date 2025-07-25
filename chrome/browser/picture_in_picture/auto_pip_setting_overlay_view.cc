// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_view.h"

constexpr float kOverlayViewOpacity = 0.7f;

// The time duration for |background_| to fade in.
constexpr int kFadeInDurationMs = 500;

AutoPipSettingOverlayView::AutoPipSettingOverlayView(
    ResultCb result_cb,
    const GURL& origin,
    const gfx::Rect& browser_view_overridden_bounds,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow)
    : show_timer_(std::make_unique<base::OneShotTimer>()) {
  CHECK(result_cb);

  init_.auto_pip_setting_view_ = std::make_unique<AutoPipSettingView>(
      std::move(result_cb),
      base::BindOnce(&AutoPipSettingOverlayView::OnHideView,
                     weak_factory_.GetWeakPtr()),
      origin, browser_view_overridden_bounds, anchor_view, arrow);
  // Create the content setting UI.
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  // Add the semi-opaque background layer.
  background_ =
      AddChildView(views::Builder<views::View>()
                       .SetPaintToLayer()
                       .SetBackground(views::CreateThemedSolidBackground(
                           kColorPipWindowBackground))
                       .Build());
  background_->layer()->SetOpacity(0.0f);
  FadeInLayer(background_->layer());
}

void AutoPipSettingOverlayView::ShowBubble(gfx::NativeView parent,
                                           PipWindowType pip_window_type) {
  DCHECK(parent);
  init_.auto_pip_setting_view_->set_parent_window(parent);
  auto_pip_setting_view_ = init_.auto_pip_setting_view_.get();
  widget_ = views::BubbleDialogDelegate::CreateBubble(
      std::move(init_.auto_pip_setting_view_));

  // Delay showing the bubble, for document pip, until after the scrim animation
  // completes.
  if (pip_window_type == PipWindowType::kDocumentPip) {
    show_timer_->Start(
        FROM_HERE, base::Milliseconds(kFadeInDurationMs),
        base::BindOnce(
            &views::Widget::ShowInactive,
            // base::Unretained() is safe since the timer is cancelled if the
            // WidgetObserver notices that the widget is being destroyed.
            base::Unretained(widget_)));
  } else {
    widget_->ShowInactive();
  }

  bubble_size_ = widget_->GetWindowBoundsInScreen().size();
  widget_->AddObserver(this);
}

void AutoPipSettingOverlayView::OnHideView() {
  // Hide the semi-opaque background layer.
  SetVisible(false);
}

gfx::Size AutoPipSettingOverlayView::GetBubbleSize() const {
  return bubble_size_;
}

void AutoPipSettingOverlayView::FadeInLayer(ui::Layer* layer) {
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::Milliseconds(kFadeInDurationMs))
      .SetOpacity(layer, kOverlayViewOpacity, gfx::Tween::LINEAR);
}

bool AutoPipSettingOverlayView::WantsEvent(const gfx::Point& point) {
  if (!auto_pip_setting_view_) {
    return false;
  }

  return auto_pip_setting_view_->WantsEvent(point);
}

AutoPipSettingOverlayView::~AutoPipSettingOverlayView() {
  if (widget_) {
    // If we're being deleted and our widget still exists, then ensure that it
    // closes.
    widget_->RemoveObserver(this);
    widget_.ExtractAsDangling()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
  background_ = nullptr;
  auto_pip_setting_view_ = nullptr;
}

void AutoPipSettingOverlayView::OnWidgetDestroying(views::Widget*) {
  show_timer_.reset();
  auto_pip_setting_view_ = nullptr;
  widget_->RemoveObserver(this);
  widget_ = nullptr;
}

BEGIN_METADATA(AutoPipSettingOverlayView, views::View)
END_METADATA
