// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {
namespace {

const char kPhoneName[] = "Nexus 6";
const char16_t kPhoneName16[] = u"Nexus 6";

class TestableNotificationController : public EasyUnlockNotificationController {
 public:
  explicit TestableNotificationController(Profile* profile)
      : EasyUnlockNotificationController(profile) {}

  TestableNotificationController(const TestableNotificationController&) =
      delete;
  TestableNotificationController& operator=(
      const TestableNotificationController&) = delete;

  ~TestableNotificationController() override {}

  // EasyUnlockNotificationController:
  MOCK_METHOD0(LaunchMultiDeviceSettings, void());
  MOCK_METHOD0(LockScreen, void());
};

class EasyUnlockNotificationControllerTest : public BrowserWithTestWindowTest {
 protected:
  EasyUnlockNotificationControllerTest() {}

  EasyUnlockNotificationControllerTest(
      const EasyUnlockNotificationControllerTest&) = delete;
  EasyUnlockNotificationControllerTest& operator=(
      const EasyUnlockNotificationControllerTest&) = delete;

  ~EasyUnlockNotificationControllerTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    notification_controller_ =
        std::make_unique<testing::StrictMock<TestableNotificationController>>(
            profile());
  }

  std::unique_ptr<testing::StrictMock<TestableNotificationController>>
      notification_controller_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

TEST_F(EasyUnlockNotificationControllerTest,
       TestShowChromebookAddedNotification) {
  const char kNotificationId[] = "easyunlock_notification_ids.chromebook_added";

  notification_controller_->ShowChromebookAddedNotification();
  absl::optional<message_center::Notification> notification =
      display_service_->GetNotification(kNotificationId);
  ASSERT_TRUE(notification);
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

  // Clicking notification button should launch settings.
  EXPECT_CALL(*notification_controller_, LaunchMultiDeviceSettings());
  notification->delegate()->Click(0, absl::nullopt);

  // Clicking the notification itself should also launch settings.
  EXPECT_CALL(*notification_controller_, LaunchMultiDeviceSettings());
  notification->delegate()->Click(absl::nullopt, absl::nullopt);
}

TEST_F(EasyUnlockNotificationControllerTest,
       TestShowPairingChangeNotification) {
  const char kNotificationId[] = "easyunlock_notification_ids.pairing_change";

  notification_controller_->ShowPairingChangeNotification();
  absl::optional<message_center::Notification> notification =
      display_service_->GetNotification(kNotificationId);
  ASSERT_TRUE(notification);
  ASSERT_EQ(2u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

  // Clicking the notification itself should do nothing.
  notification->delegate()->Click(absl::nullopt, absl::nullopt);

  // Clicking 1st notification button should lock screen settings.
  EXPECT_CALL(*notification_controller_, LockScreen());
  notification->delegate()->Click(0, absl::nullopt);

  // Clicking 2nd notification button should launch settings.
  EXPECT_CALL(*notification_controller_, LaunchMultiDeviceSettings());
  notification->delegate()->Click(1, absl::nullopt);
}

TEST_F(EasyUnlockNotificationControllerTest,
       TestShowPairingChangeAppliedNotification) {
  const char kNotificationId[] =
      "easyunlock_notification_ids.pairing_change_applied";

  notification_controller_->ShowPairingChangeAppliedNotification(kPhoneName);
  absl::optional<message_center::Notification> notification =
      display_service_->GetNotification(kNotificationId);
  ASSERT_TRUE(notification);
  ASSERT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(message_center::SYSTEM_PRIORITY, notification->priority());

  // Check that the phone name is in the notification message.
  EXPECT_NE(std::string::npos, notification->message().find(kPhoneName16));

  // Clicking notification button should launch settings.
  EXPECT_CALL(*notification_controller_, LaunchMultiDeviceSettings());
  notification->delegate()->Click(0, absl::nullopt);

  // Clicking the notification itself should also launch settings.
  EXPECT_CALL(*notification_controller_, LaunchMultiDeviceSettings());
  notification->delegate()->Click(absl::nullopt, absl::nullopt);
}

TEST_F(EasyUnlockNotificationControllerTest,
       PairingAppliedRemovesPairingChange) {
  const char kPairingChangeId[] = "easyunlock_notification_ids.pairing_change";
  const char kPairingAppliedId[] =
      "easyunlock_notification_ids.pairing_change_applied";

  notification_controller_->ShowPairingChangeNotification();
  EXPECT_TRUE(display_service_->GetNotification(kPairingChangeId));

  notification_controller_->ShowPairingChangeAppliedNotification(kPhoneName);
  EXPECT_FALSE(display_service_->GetNotification(kPairingChangeId));
  EXPECT_TRUE(display_service_->GetNotification(kPairingAppliedId));
}

}  // namespace
}  // namespace ash
