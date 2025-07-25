// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_countdown_view.h"

#include "ash/constants/ash_features.h"
#include "ash/style/pill_button.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_tray.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"

namespace ash {

class FocusModeCountdownViewTest : public AshTestBase {
 public:
  FocusModeCountdownViewTest() : scoped_feature_(features::kFocusMode) {}
  ~FocusModeCountdownViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    focus_mode_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->focus_mode_tray();
  }

  void TearDown() override {
    focus_mode_tray_ = nullptr;
    AshTestBase::TearDown();
  }

  PillButton* GetExtendTimeButton() {
    return focus_mode_tray_->countdown_view_for_testing()
        ->extend_session_duration_button_;
  }

  views::Label* GetTimeRemainingLabel() {
    return focus_mode_tray_->countdown_view_for_testing()
        ->time_remaining_label_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_;
  raw_ptr<FocusModeTray> focus_mode_tray_ = nullptr;
};

// Tests that the bubble is not visible until the tray is activated, and the
// bubble goes away when the tray is deactivated or focus mode ends.
TEST_F(FocusModeCountdownViewTest, ToggleVisibility) {
  ASSERT_TRUE(focus_mode_tray_);
  EXPECT_FALSE(focus_mode_tray_->tray_bubble_wrapper_for_testing());

  // Start the focus session and activate the tray.
  FocusModeController* controller = FocusModeController::Get();
  controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_tray_->tray_bubble_wrapper_for_testing());
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->tray_bubble_wrapper_for_testing());

  // Click the tray again to toggle the bubble.
  LeftClickOn(focus_mode_tray_);
  EXPECT_FALSE(focus_mode_tray_->tray_bubble_wrapper_for_testing());

  // Bring the bubble back, then end focus mode to toggle the bubble.
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->tray_bubble_wrapper_for_testing());
  controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_tray_->tray_bubble_wrapper_for_testing());
}

// Tests that in an active focus session, the user clicks the `+10 min` button
// while the valid time to extend is less than 10 minutes. Check if the `+10
// min` button is disabled and the text of the remaining time label in the
// progress bar should be equal to the maximum session duration.
TEST_F(FocusModeCountdownViewTest, ExtendSessionDurationUntilUpperBound) {
  FocusModeController* controller = FocusModeController::Get();
  controller->SetSessionDuration(focus_mode_util::kMaximumDuration -
                                 base::Minutes(1));
  controller->ToggleFocusMode();

  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->tray_bubble_wrapper_for_testing());

  auto* button = GetExtendTimeButton();
  EXPECT_TRUE(button->GetEnabled());

  auto* label = GetTimeRemainingLabel();
  EXPECT_EQ(u"4 hr, 59 min, 0 sec", label->GetText());

  // Extend the session duration.
  LeftClickOn(button);

  EXPECT_FALSE(button->GetEnabled());
  EXPECT_EQ(u"5 hr, 0 min, 0 sec", label->GetText());
  EXPECT_EQ(focus_mode_util::kMaximumDuration, controller->session_duration());
}

}  // namespace ash
