// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/message_center/fake_message_center.h"

namespace ash {

namespace {

int GetPrefNotificationCount(const char* pref_name) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  return prefs->GetInteger(pref_name);
}

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() = default;

  TestMessageCenter(const TestMessageCenter&) = delete;
  TestMessageCenter& operator=(const TestMessageCenter&) = delete;

  ~TestMessageCenter() override = default;

  void ClickOnNotification(const std::string& id) override {
    message_center::Notification* notification =
        FindVisibleNotificationById(id);
    CHECK(notification);
    notification->delegate()->Click(absl::nullopt, absl::nullopt);
  }

  void ClickOnNotificationButton(const std::string& id,
                                 int button_index) override {
    message_center::Notification* notification =
        FindVisibleNotificationById(id);
    CHECK(notification);
    notification->delegate()->Click(button_index, absl::nullopt);
  }
};

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

}  // namespace

class InputDeviceSettingsNotificationControllerTest : public AshTestBase {
 public:
  InputDeviceSettingsNotificationControllerTest() = default;

  InputDeviceSettingsNotificationControllerTest(
      const InputDeviceSettingsNotificationControllerTest&) = delete;
  InputDeviceSettingsNotificationControllerTest& operator=(
      const InputDeviceSettingsNotificationControllerTest&) = delete;

  ~InputDeviceSettingsNotificationControllerTest() override = default;

  message_center::FakeMessageCenter* message_center() {
    return message_center_.get();
  }
  InputDeviceSettingsNotificationController* controller() {
    return controller_.get();
  }

  // AshTestBase:
  void SetUp() override {
    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(std::move(delegate));
    AshTestBase::SetUp();
    message_center_ = std::make_unique<TestMessageCenter>();
    controller_ = std::make_unique<InputDeviceSettingsNotificationController>(
        message_center_.get());
  }

  void TearDown() override {
    controller_.reset();
    message_center_.reset();
    AshTestBase::TearDown();
  }

 protected:
  MockNewWindowDelegate& new_window_delegate() { return *new_window_delegate_; }
  raw_ptr<MockNewWindowDelegate, DanglingUntriaged | ExperimentalAsh>
      new_window_delegate_;
  std::unique_ptr<TestMessageCenter> message_center_;
  std::unique_ptr<InputDeviceSettingsNotificationController> controller_;
  std::unique_ptr<TestNewWindowDelegateProvider> delegate_provider_;
};

TEST_F(InputDeviceSettingsNotificationControllerTest,
       NotifyRightClickRewriteBlockedBySetting) {
  size_t expected_notification_count = 1;
  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "alt_right_click_rewrite_blocked_by_setting"));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kSearch,
      ui::mojom::SimulateRightClickModifier::kAlt);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "search_right_click_rewrite_blocked_by_setting"));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kNone);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "right_click_rewrite_disabled_by_setting"));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       RemapToRightClickNotificationOnlyShownForActiveUserSessions) {
  GetSessionControllerClient()->LockScreen();

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  EXPECT_EQ(message_center()->NotificationCount(), 0u);
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       SixPackKeyNotificationShownAtMostThreeTimes) {
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kSixPackKeyDeleteNotificationsRemaining));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(1u, message_center()->NotificationCount());
  EXPECT_EQ(2, GetPrefNotificationCount(
                   prefs::kSixPackKeyDeleteNotificationsRemaining));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(1, GetPrefNotificationCount(
                   prefs::kSixPackKeyDeleteNotificationsRemaining));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(0, GetPrefNotificationCount(
                   prefs::kSixPackKeyDeleteNotificationsRemaining));

  message_center()->RemoveAllNotifications(
      false, message_center::MessageCenter::RemoveType::ALL);
  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(0u, message_center()->NotificationCount());

  // Only the delete notification pref should have changed.
  EXPECT_EQ(
      3, GetPrefNotificationCount(prefs::kSixPackKeyEndNotificationsRemaining));
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kSixPackKeyHomeNotificationsRemaining));
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kSixPackKeyInsertNotificationsRemaining));
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kSixPackKeyPageUpNotificationsRemaining));
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kSixPackKeyPageDownNotificationsRemaining));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       RightClickNotificationShownAtMostThreeTimes) {
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  EXPECT_EQ(1u, message_center()->NotificationCount());
  EXPECT_EQ(2, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kSearch,
      ui::mojom::SimulateRightClickModifier::kAlt);
  EXPECT_EQ(2u, message_center()->NotificationCount());
  EXPECT_EQ(1, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kNone);
  EXPECT_EQ(3u, message_center()->NotificationCount());
  EXPECT_EQ(0, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  EXPECT_EQ(3u, message_center()->NotificationCount());
  EXPECT_EQ(0, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       StopShowingNotificationIfUserClicksOnIt) {
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  message_center()->ClickOnNotification(
      "alt_right_click_rewrite_blocked_by_setting");
  EXPECT_EQ(0, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));
}

// TODO(b/279503977): Add test that verifies behavior of clicking on the
// "Learn more" button.
TEST_F(InputDeviceSettingsNotificationControllerTest,
       ShowTouchpadSettingsOnRightClickNotificationClick) {
  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  message_center()->ClickOnNotificationButton(
      "alt_right_click_rewrite_blocked_by_setting",
      NotificationButtonIndex::BUTTON_EDIT_SHORTCUT);
  EXPECT_EQ(GetSystemTrayClient()->show_touchpad_settings_count(), 1);
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       ShowRemapKeysSettingsOnSixPackNotificationClick) {
  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  message_center()->ClickOnNotificationButton(
      "delete_six_pack_rewrite_blocked_by_setting_1",
      NotificationButtonIndex::BUTTON_EDIT_SHORTCUT);
  EXPECT_EQ(GetSystemTrayClient()->show_remap_keys_subpage_count(), 1);
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       NotifySixPackRewriteBlockedBySetting) {
  size_t expected_notification_count = 1;
  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "delete_six_pack_rewrite_blocked_by_setting_1"));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_INSERT, ui::mojom::SixPackShortcutModifier::kSearch,
      ui::mojom::SixPackShortcutModifier::kNone,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "insert_six_pack_rewrite_blocked_by_setting_1"));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_HOME, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "home_six_pack_rewrite_blocked_by_setting_1"));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_END, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "end_six_pack_rewrite_blocked_by_setting_1"));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_PRIOR, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "page_up_six_pack_rewrite_blocked_by_setting_1"));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_NEXT, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "page_down_six_pack_rewrite_blocked_by_setting_1"));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       SixPackRewriteNotificationOnlyShownForActiveUserSessions) {
  GetSessionControllerClient()->LockScreen();
  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_PRIOR, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(message_center()->NotificationCount(), 0u);
}

TEST_F(InputDeviceSettingsNotificationControllerTest, LearnMoreButtonClicked) {
  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_CALL(
      new_window_delegate(),
      OpenUrl(GURL("https://support.google.com/chromebook?p=keyboard_settings"),
              NewWindowDelegate::OpenUrlFrom::kUserInteraction,
              NewWindowDelegate::Disposition::kNewForegroundTab));
  message_center()->ClickOnNotificationButton(
      "delete_six_pack_rewrite_blocked_by_setting_1",
      NotificationButtonIndex::BUTTON_LEARN_MORE);
}

}  // namespace ash
