// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_notification_manager.h"

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace ash::cloud_upload {

class CloudUploadNotificationManagerTest : public testing::Test {
 public:
  CloudUploadNotificationManagerTest() = default;

  CloudUploadNotificationManagerTest(
      const CloudUploadNotificationManagerTest&) = delete;
  CloudUploadNotificationManagerTest& operator=(
      const CloudUploadNotificationManagerTest&) = delete;

  // testing::Test:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    display_service_ = static_cast<StubNotificationDisplayService*>(
        NotificationDisplayServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_.get(),
                base::BindRepeating(
                    &StubNotificationDisplayService::FactoryForTests)));
  }

 protected:
  Profile* profile() { return profile_.get(); }

  absl::optional<message_center::Notification> notification() {
    auto notifications = display_service_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::TRANSIENT);
    if (notifications.size()) {
      return notifications[0];
    }
    return absl::nullopt;
  }

  bool HaveProgressNotification() {
    return notification().has_value() &&
           notification()->type() ==
               message_center::NotificationType::NOTIFICATION_TYPE_PROGRESS &&
           notification()->display_source() ==
               l10n_util::GetStringUTF16(
                   IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES);
  }

  bool HaveCompleteNotification() {
    return notification().has_value() &&
           notification()->type() ==
               message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE &&
           notification()->title().starts_with(u"Moved ") &&
           notification()->display_source() ==
               l10n_util::GetStringUTF16(
                   IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES) &&
           !notification()->buttons().empty() &&
           (notification()->buttons().front().title == u"Show in folder");
  }

  bool HaveErrorNotification() {
    return notification().has_value() &&
           notification()->type() ==
               message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE &&
           notification()->title().starts_with(u"Can't move file") &&
           notification()->display_source() ==
               l10n_util::GetStringUTF16(
                   IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME_FILES);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<StubNotificationDisplayService, ExperimentalAsh> display_service_;
  std::string file_name_ = "foo.docx";
  base::FilePath file_path_ = base::FilePath("/some/path/foo.doc");
};

TEST_F(CloudUploadNotificationManagerTest,
       ShowUploadProgressCreatesNotification) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), file_name_, "Google Drive", "Google Docs", 1);

  ASSERT_EQ(absl::nullopt, notification());
  manager->ShowUploadProgress(1);
  ASSERT_TRUE(HaveProgressNotification());

  manager->CloseForTest();
}

TEST_F(CloudUploadNotificationManagerTest, MinimumTiming) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), file_name_, "Google Drive", "Google Docs", 1);

  manager->ShowUploadProgress(1);
  manager->ShowUploadProgress(100);
  manager->SetDestinationPath(file_path_);
  manager->MarkUploadComplete();
  ASSERT_TRUE(HaveProgressNotification());

  // The progress notification should still be showing.
  task_environment_.FastForwardBy(base::Milliseconds(4900));
  ASSERT_TRUE(HaveProgressNotification());

  // Now we see the Complete nofication after 5s.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  ASSERT_TRUE(HaveCompleteNotification());

  // Now we're at 9900 ms total - we still expect the Complete notification.
  task_environment_.FastForwardBy(base::Milliseconds(4500));
  ASSERT_TRUE(HaveCompleteNotification());

  // After > 10s total, the notification should be closed.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  ASSERT_EQ(absl::nullopt, notification());
}

TEST_F(CloudUploadNotificationManagerTest, CompleteWithoutProgress) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), file_name_, "Google Drive", "Google Docs", 1);

  manager->SetDestinationPath(file_path_);
  manager->MarkUploadComplete();
  ASSERT_TRUE(HaveCompleteNotification());

  // The complete notification should still be showing.
  task_environment_.FastForwardBy(base::Milliseconds(4900));
  ASSERT_TRUE(HaveCompleteNotification());

  // After > 5s total, the notification should be closed.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  ASSERT_EQ(absl::nullopt, notification());
}

TEST_F(CloudUploadNotificationManagerTest, ShowInFolderClick) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), file_name_, "Google Drive", "Google Docs", 1);

  manager->SetDestinationPath(file_path_);
  manager->MarkUploadComplete();
  ASSERT_TRUE(HaveCompleteNotification());

  base::RunLoop run_loop;
  manager->SetHandleNotificationClickCallbackForTesting(
      base::BindLambdaForTesting(
          [&run_loop, this](base::FilePath destination_path) {
            // Check |destination_path| is as expected.
            EXPECT_EQ(destination_path, file_path_);
            run_loop.Quit();
          }));

  // Click "Show in folder" button (0th button) which triggers
  // |HandleNotificationClick|.
  display_service_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                  notification()->id(), /*action_index=*/0,
                                  absl::nullopt);

  // Run loop until |HandleNotificationClick| is called.
  run_loop.Run();
}

TEST_F(CloudUploadNotificationManagerTest, ErrorStaysOpen) {
  scoped_refptr<CloudUploadNotificationManager> manager =
      base::MakeRefCounted<CloudUploadNotificationManager>(
          profile(), file_name_, "Google Drive", "Google Docs", 1);

  manager->ShowUploadProgress(1);
  manager->ShowUploadProgress(100);
  manager->ShowUploadError("error");
  // The error is shown straight away.
  ASSERT_TRUE(HaveErrorNotification());

  // The error notification should still be showing.
  task_environment_.FastForwardBy(base::Seconds(60));
  ASSERT_TRUE(HaveErrorNotification());

  manager->CloseForTest();
}

TEST_F(CloudUploadNotificationManagerTest, ManagerLifetime) {
  {
    scoped_refptr<CloudUploadNotificationManager> manager =
        base::MakeRefCounted<CloudUploadNotificationManager>(
            profile(), file_name_, "Google Drive", "Google Docs", 1);

    manager->ShowUploadProgress(1);
    manager->ShowUploadError("error");
    ASSERT_TRUE(HaveErrorNotification());
  }
  // We still have a ref to manager until the notification is dismissed.
  ASSERT_TRUE(HaveErrorNotification());

  notification()->delegate()->Click(absl::nullopt, absl::nullopt);
  ASSERT_EQ(absl::nullopt, notification());
}

}  // namespace ash::cloud_upload
