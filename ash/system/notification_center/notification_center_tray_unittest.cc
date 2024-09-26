// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_tray.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

constexpr char kNotificationCenterTrayNoNotificationsToastId[] =
    "notification_center_tray_toast_ids.no_notifications";

class NotificationCenterTrayTest : public AshTestBase {
 public:
  NotificationCenterTrayTest() = default;
  NotificationCenterTrayTest(const NotificationCenterTrayTest&) = delete;
  NotificationCenterTrayTest& operator=(const NotificationCenterTrayTest&) =
      delete;
  ~NotificationCenterTrayTest() override = default;

  void SetUp() override {
    // Enable quick settings revamp feature.
    scoped_feature_list_.InitAndEnableFeature(features::kQsRevamp);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCameraEffectsSupportedByHardware);

    AshTestBase::SetUp();

    test_api_ = std::make_unique<NotificationCenterTestApi>(
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()
            ->notification_center_tray());
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<NotificationCenterTestApi> test_api_;
};

// Test the initial state.
TEST_F(NotificationCenterTrayTest, ShowTrayButtonOnNotificationAvailability) {
  EXPECT_FALSE(test_api()->GetTray()->GetVisible());

  std::string id = test_api()->AddNotification();
  EXPECT_TRUE(test_api()->GetTray()->GetVisible());

  message_center::MessageCenter::Get()->RemoveNotification(id, true);

  EXPECT_FALSE(test_api()->GetTray()->GetVisible());
}

// Bubble creation and destruction.
TEST_F(NotificationCenterTrayTest, ShowAndHideBubbleOnUserInteraction) {
  test_api()->AddNotification();

  auto* tray = test_api()->GetTray();

  // Clicking on the tray button should show  the bubble.
  LeftClickOn(tray);
  EXPECT_TRUE(test_api()->IsBubbleShown());

  // Clicking a second time should destroy the bubble.
  LeftClickOn(test_api()->GetTray());
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Hitting escape after opening the bubble should destroy the bubble
// gracefully.
TEST_F(NotificationCenterTrayTest, EscapeClosesBubble) {
  auto* tray = test_api()->GetTray();
  LeftClickOn(tray);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Removing all notifications by hitting the `clear_all_button_` should result
// in the bubble being destroyed and the tray bubble going invisible.
TEST_F(NotificationCenterTrayTest,
       ClearAllNotificationsDestroysBubbleAndHidesTray) {
  test_api()->AddNotification();
  test_api()->AddNotification();
  test_api()->AddNotification();

  auto* tray = test_api()->GetTray();

  LeftClickOn(tray);
  LeftClickOn(test_api()->GetClearAllButton());
  EXPECT_FALSE(test_api()->IsBubbleShown());
  EXPECT_FALSE(test_api()->IsTrayShown());
}

// The last notification being removed directly by the
// `message_center::MessageCenter` API should result in the bubble being
// destroyed and tray visibilty being updated.
TEST_F(NotificationCenterTrayTest, NotificationsRemovedByMessageCenterApi) {
  std::string id = test_api()->AddNotification();
  test_api()->RemoveNotification(id);

  EXPECT_FALSE(test_api()->IsBubbleShown());
  EXPECT_FALSE(test_api()->IsTrayShown());
}

// Tests that opening the bubble results in existing popups being dismissed
// and no new ones being created.
TEST_F(NotificationCenterTrayTest, NotificationPopupsHiddenWithBubble) {
  // Adding a notification should generate a popup.
  std::string id = test_api()->AddNotification();
  EXPECT_TRUE(test_api()->IsPopupShown(id));

  // Opening the notification center should result in the popup being dismissed.
  test_api()->ToggleBubble();
  EXPECT_FALSE(test_api()->IsPopupShown(id));

  // New notifications should not generate popups while the notification center
  // is visible.
  std::string id2 = test_api()->AddNotification();
  EXPECT_FALSE(test_api()->IsPopupShown(id));
}

// Tests that popups are shown after the notification center is closed.
TEST_F(NotificationCenterTrayTest, NotificationPopupsShownAfterBubbleClose) {
  test_api()->AddNotification();

  // Open and close bubble to dismiss existing popups.
  test_api()->ToggleBubble();
  test_api()->ToggleBubble();

  // New notifications should show up as popups after the bubble is closed.
  std::string id = test_api()->AddNotification();
  EXPECT_TRUE(test_api()->IsPopupShown(id));
}

// Keyboard accelerator shows/hides the bubble.
TEST_F(NotificationCenterTrayTest, AcceleratorTogglesBubble) {
  test_api()->AddNotification();
  EXPECT_FALSE(test_api()->IsBubbleShown());
  // Pressing the accelerator should show the bubble.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
  EXPECT_TRUE(test_api()->IsBubbleShown());
  // Pressing the acccelerator again should hide the bubble.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Keyboard accelerator shows a toast when there are no notifications.
TEST_F(NotificationCenterTrayTest, AcceleratorShowsToastWhenNoNotifications) {
  ASSERT_EQ(test_api()->GetNotificationCount(), 0u);
  EXPECT_FALSE(ToastManager::Get()->IsRunning(
      kNotificationCenterTrayNoNotificationsToastId));
  // Pressing the accelerator should show the toast and not the bubble.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
  EXPECT_TRUE(ToastManager::Get()->IsRunning(
      kNotificationCenterTrayNoNotificationsToastId));
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Tests that the bubble automatically hides if it is visible when another
// bubble becomes visible, and otherwise does not automatically show or hide.
TEST_F(NotificationCenterTrayTest, BubbleHideBehavior) {
  // Basic verification test that the notification center tray bubble can
  // show/hide itself when no other bubbles are visible.
  EXPECT_FALSE(test_api()->IsBubbleShown());
  test_api()->AddNotification();
  test_api()->ToggleBubble();
  EXPECT_TRUE(test_api()->IsBubbleShown());
  test_api()->ToggleBubble();
  EXPECT_FALSE(test_api()->IsBubbleShown());

  // Test that the notification center tray bubble automatically hides when it
  // is currently visible while another bubble becomes visible.
  test_api()->ToggleBubble();
  EXPECT_TRUE(test_api()->IsBubbleShown());
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_FALSE(test_api()->IsBubbleShown());

  // Hide all currently visible bubbles.
  GetPrimaryUnifiedSystemTray()->CloseBubble();
  EXPECT_FALSE(test_api()->IsBubbleShown());

  // Test that the notification center tray bubble stays hidden when showing
  // another bubble.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_FALSE(test_api()->IsBubbleShown());
}

// Tests that visibility of the Do not disturb icon changes with Do not disturb
// mode.
TEST_F(NotificationCenterTrayTest, DoNotDisturbIconVisibility) {
  // Test the case where the tray is not initially visible.
  ASSERT_FALSE(test_api()->IsTrayShown());
  EXPECT_FALSE(test_api()->IsDoNotDisturbIconShown());
  message_center::MessageCenter::Get()->SetQuietMode(true);
  EXPECT_TRUE(test_api()->IsTrayShown());
  EXPECT_TRUE(test_api()->IsDoNotDisturbIconShown());
  message_center::MessageCenter::Get()->SetQuietMode(false);
  EXPECT_FALSE(test_api()->IsTrayShown());
  EXPECT_FALSE(test_api()->IsDoNotDisturbIconShown());

  // Test the case where the tray is initially visible.
  test_api()->AddNotification();
  ASSERT_TRUE(test_api()->IsTrayShown());
  EXPECT_FALSE(test_api()->IsDoNotDisturbIconShown());
  message_center::MessageCenter::Get()->SetQuietMode(true);
  EXPECT_TRUE(test_api()->IsTrayShown());
  EXPECT_TRUE(test_api()->IsDoNotDisturbIconShown());
  message_center::MessageCenter::Get()->SetQuietMode(false);
  EXPECT_TRUE(test_api()->IsTrayShown());
  EXPECT_FALSE(test_api()->IsDoNotDisturbIconShown());
}

TEST_F(NotificationCenterTrayTest, DoNotDisturbUpdatesPinnedIcons) {
  test_api()->AddPinnedNotification();
  EXPECT_TRUE(test_api()->IsPinnedIconShown());

  message_center::MessageCenter::Get()->SetQuietMode(true);
  EXPECT_FALSE(test_api()->IsPinnedIconShown());

  message_center::MessageCenter::Get()->SetQuietMode(false);
  EXPECT_TRUE(test_api()->IsPinnedIconShown());
}

TEST_F(NotificationCenterTrayTest, NoPrivacyIndicators) {
  // No privacy indicators when the feature is not enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kVideoConference,
                             features::kPrivacyIndicators});

  auto notification_tray =
      std::make_unique<NotificationCenterTray>(GetPrimaryShelf());
  EXPECT_FALSE(notification_tray->privacy_indicators_view());
}

TEST_F(NotificationCenterTrayTest, NoPrivacyIndicatorsWhenVcEnabled) {
  // No privacy indicators when `kVideoConference` is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kVideoConference,
                            features::kPrivacyIndicators},
      /*disabled_features=*/{});

  auto notification_tray =
      std::make_unique<NotificationCenterTray>(GetPrimaryShelf());
  EXPECT_FALSE(notification_tray->privacy_indicators_view());
}

// Tests that the focus ring is visible and has proper size when the
// notification center tray is focused.
TEST_F(NotificationCenterTrayTest, FocusRing) {
  // Add a notification to make the notification center tray visible.
  test_api()->AddNotification();
  ASSERT_TRUE(test_api()->IsTrayShown());

  // Verify that the focus ring is not already visible.
  EXPECT_FALSE(test_api()->GetFocusRing()->GetVisible());

  // Focus the notification center tray.
  test_api()->FocusTray();

  // Verify that the focus ring is visible and is larger than the notification
  // center tray by `kTrayBackgroundFocusPadding`.
  EXPECT_TRUE(test_api()->GetFocusRing()->GetVisible());
  EXPECT_EQ(test_api()->GetFocusRing()->size(),
            test_api()->GetTray()->size() + kTrayBackgroundFocusPadding.size());
}

// Test suite for the notification center when `kPrivacyIndicators` is enabled.
class NotificationCenterTrayPrivacyIndicatorsTest : public AshTestBase {
 public:
  NotificationCenterTrayPrivacyIndicatorsTest() = default;
  NotificationCenterTrayPrivacyIndicatorsTest(
      const NotificationCenterTrayPrivacyIndicatorsTest&) = delete;
  NotificationCenterTrayPrivacyIndicatorsTest& operator=(
      const NotificationCenterTrayPrivacyIndicatorsTest&) = delete;
  ~NotificationCenterTrayPrivacyIndicatorsTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kQsRevamp, features::kPrivacyIndicators}, {});

    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the privacy indicators view is created and show/hide accordingly
// when updated.
TEST_F(NotificationCenterTrayPrivacyIndicatorsTest,
       PrivacyIndicatorsVisibility) {
  auto* notification_tray = StatusAreaWidgetTestHelper::GetStatusAreaWidget()
                                ->notification_center_tray();
  auto* privacy_indicators_view = notification_tray->privacy_indicators_view();
  EXPECT_TRUE(privacy_indicators_view);

  EXPECT_FALSE(privacy_indicators_view->GetVisible());

  scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate =
      base::MakeRefCounted<PrivacyIndicatorsNotificationDelegate>();

  // Updates the controller to simulate camera access, the privacy indicators
  // should become visible.
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      /*app_id=*/"app_id", /*app_name=*/u"App Name",
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false, delegate, PrivacyIndicatorsSource::kApps);
  EXPECT_TRUE(privacy_indicators_view->GetVisible());

  // Updates the controller to simulate that camera and microphone are not
  // accessed, the privacy indicators should be hidden.
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      /*app_id=*/"app_id", /*app_name=*/u"App Name",
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false, delegate, PrivacyIndicatorsSource::kApps);
  EXPECT_FALSE(privacy_indicators_view->GetVisible());
}

// TODO(b/252875025):
// Add following test cases as we add relevant functionality:
// - Focus Change dismissing bubble
// - Popup notifications are dismissed when the bubble appears.
// - Display removed while the bubble is shown.
// - Tablet mode transition with the bubble open.

}  // namespace ash
