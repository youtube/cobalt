// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_utils.h"

namespace {

class MockWidgetObserver : public views::WidgetObserver {
 public:
  MOCK_METHOD(void, OnWidgetClosing, (views::Widget*), ());
};

}  // namespace

class AutoPipSettingOverlayViewTest : public views::ViewsTestBase {
 public:
  AutoPipSettingOverlayViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create setting overlay widget.
    widget_ = CreateTestWidget();
    widget_->Show();

    // Create parent Widget.
    parent_widget_ = CreateTestWidget();
    parent_widget_->Show();

    // Create the anchor Widget.
    anchor_view_widget_ = CreateTestWidget();
    anchor_view_widget_->Show();
    auto* anchor_view =
        anchor_view_widget_->SetContentsView(std::make_unique<views::View>());

    // Define the browser view overridden bounds.
    const gfx::Rect browser_view_overridden_bounds(0, 0, 500, 500);

    animation_duration_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    setting_overlay_ =
        widget_->SetContentsView(std::make_unique<AutoPipSettingOverlayView>(
            cb().Get(), origin_, browser_view_overridden_bounds, anchor_view,
            views::BubbleBorder::TOP_CENTER));
  }

  void TearDown() override {
    animation_duration_.reset();
    anchor_view_widget_.reset();
    parent_widget_.reset();
    setting_overlay_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  AutoPipSettingOverlayView* setting_overlay() { return setting_overlay_; }

  views::Widget* anchor_view_widget() { return anchor_view_widget_.get(); }

  views::View* background() const {
    return setting_overlay_->get_background_for_testing();
  }

  const views::Widget* widget() const { return widget_.get(); }

  using UiResult = AutoPipSettingView::UiResult;

  base::MockOnceCallback<void(UiResult)>& cb() { return cb_; }

  void RemoveAndDeleteSettingOverlay() {
    setting_overlay_ = nullptr;

    // Setting the contents view will remove and delete the existing contents
    // view, which is the setting overlay.
    widget_->SetContentsView(std::make_unique<views::View>());
  }

 private:
  base::MockOnceCallback<void(UiResult)> cb_;
  std::unique_ptr<views::Widget> parent_widget_;
  std::unique_ptr<views::Widget> anchor_view_widget_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AutoPipSettingOverlayView> setting_overlay_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  const GURL origin_{"https://example.com"};

  // Used to force a non-zero animation duration.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> animation_duration_;
};

TEST_F(AutoPipSettingOverlayViewTest, TestViewInitialization) {
  EXPECT_TRUE(widget()->IsVisible());
  EXPECT_EQ(
      background()->GetColorProvider()->GetColor(kColorPipWindowBackground),
      background()->GetBackground()->get_color());
}

TEST_F(AutoPipSettingOverlayViewTest, TestBackgroundLayerAnimation) {
  // Background layer opacity should start at 0.0f and end at 0.70f.
  EXPECT_EQ(0.0f, background()->layer()->opacity());
  EXPECT_EQ(0.70f, background()->layer()->GetTargetOpacity());

  // Progress animation to its end position. Background layer should fade in to
  // a 0.70f opacity.
  background()->layer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(0.70f, background()->layer()->GetTargetOpacity());
}

TEST_F(AutoPipSettingOverlayViewTest, TestWantsEvent) {
  setting_overlay()->ShowBubble(
      anchor_view_widget()->GetNativeView(),
      AutoPipSettingOverlayView::PipWindowType::kDocumentPip);
  // Assume nothing is at screen coordinate 0,0.
  EXPECT_FALSE(setting_overlay()->WantsEvent(gfx::Point(0, 0)));

  // Make sure that the buttons work.
  auto* view = setting_overlay()->get_view_for_testing();
  EXPECT_TRUE(setting_overlay()->WantsEvent(
      view->get_allow_always_button_center_in_screen_for_testing()));
  EXPECT_TRUE(setting_overlay()->WantsEvent(
      view->get_allow_once_button_center_in_screen_for_testing()));
  EXPECT_TRUE(setting_overlay()->WantsEvent(
      view->get_block_button_center_in_screen_for_testing()));
}

TEST_F(AutoPipSettingOverlayViewTest,
       TestOverlayViewDoesNotHaveFocusForDocumentPip) {
  setting_overlay()->ShowBubble(
      anchor_view_widget()->GetNativeView(),
      AutoPipSettingOverlayView::PipWindowType::kDocumentPip);
  EXPECT_FALSE(setting_overlay()->HasFocus());
}

TEST_F(AutoPipSettingOverlayViewTest,
       TestOverlayViewDoesNotHaveFocusForVideoPip) {
  setting_overlay()->ShowBubble(
      anchor_view_widget()->GetNativeView(),
      AutoPipSettingOverlayView::PipWindowType::kVideoPip);
  EXPECT_FALSE(setting_overlay()->HasFocus());
}

#if BUILDFLAG(IS_MAC)
// Ensure that bubbles requested for Document Picture-in-Picture windows are
// shown not shown before an 180Ms delay, on Mac.
TEST_F(AutoPipSettingOverlayViewTest,
       TestOverlayViewNotShownBeforeDelayForDocumentPip) {
  // Initially bubble should not be shown.
  setting_overlay()->ShowBubble(
      anchor_view_widget()->GetNativeView(),
      AutoPipSettingOverlayView::PipWindowType::kDocumentPip);
  EXPECT_FALSE(
      setting_overlay()->get_view_for_testing()->GetWidget()->IsVisible());

  // Bubble should not be shown before an 180Ms delay. This is to ensure that
  // the Mac window animation has completed, before we show the permission
  // prompt.
  task_environment()->FastForwardBy(base::Milliseconds(179));
  EXPECT_FALSE(
      setting_overlay()->get_view_for_testing()->GetWidget()->IsVisible());
}
#endif  // BUILDFLAG(IS_MAC)

// Ensure that bubbles requested for Document Picture-in-Picture windows are
// shown after the scrim animation.
TEST_F(AutoPipSettingOverlayViewTest,
       TestOverlayViewShownWithDelayForDocumentPip) {
  setting_overlay()->ShowBubble(
      anchor_view_widget()->GetNativeView(),
      AutoPipSettingOverlayView::PipWindowType::kDocumentPip);
  EXPECT_FALSE(
      setting_overlay()->get_view_for_testing()->GetWidget()->IsVisible());

  // Bubble should be shown after an 500Ms delay.
  task_environment()->FastForwardBy(base::Milliseconds(505));
  EXPECT_TRUE(
      setting_overlay()->get_view_for_testing()->GetWidget()->IsVisible());
}

TEST_F(AutoPipSettingOverlayViewTest,
       TestOverlayViewShownWithoutDelayForVideoPip) {
  // Ensure that bubbles requested for Video Picture-in-Picture windows are
  // shown without delay, regardless of platform.
  setting_overlay()->ShowBubble(
      anchor_view_widget()->GetNativeView(),
      AutoPipSettingOverlayView::PipWindowType::kVideoPip);
  EXPECT_TRUE(
      setting_overlay()->get_view_for_testing()->GetWidget()->IsVisible());
}

TEST_F(AutoPipSettingOverlayViewTest, TestDeletingOverlayClosesBubble) {
  setting_overlay()->ShowBubble(
      anchor_view_widget()->GetNativeView(),
      AutoPipSettingOverlayView::PipWindowType::kVideoPip);

  MockWidgetObserver widget_observer;
  views::Widget* widget =
      setting_overlay()->get_view_for_testing()->GetWidget();
  widget->AddObserver(&widget_observer);

  // Removing and deleting the setting overlay should close the bubble widget.
  EXPECT_CALL(widget_observer, OnWidgetClosing(widget))
      .WillOnce(testing::InvokeWithoutArgs(
          [&]() { widget->RemoveObserver(&widget_observer); }));
  RemoveAndDeleteSettingOverlay();
  testing::Mock::VerifyAndClearExpectations(&widget_observer);
}
