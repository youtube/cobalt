// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_handler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/metrics/mock_notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/safe_browsing/mock_notification_content_detection_service.h"
#include "chrome/browser/safe_browsing/notification_content_detection_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_service.h"
#include "components/safe_browsing/content/browser/notification_content_detection/test_model_observer_tracker.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/permission_result.h"
#include "content/public/common/persistent_notification_status.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

using ::testing::_;
using ::testing::Return;
using PermissionStatus = blink::mojom::PermissionStatus;

namespace {

const char kExampleNotificationId[] = "example_notification_id";
const char kExampleOrigin[] = "https://example.com";

class TestingProfileWithPermissionManager : public TestingProfile {
 public:
  TestingProfileWithPermissionManager()
      : permission_manager_(
            std::make_unique<
                testing::NiceMock<content::MockPermissionManager>>()) {}
  TestingProfileWithPermissionManager(
      const TestingProfileWithPermissionManager&) = delete;
  TestingProfileWithPermissionManager& operator=(
      const TestingProfileWithPermissionManager&) = delete;

  ~TestingProfileWithPermissionManager() override = default;

  // Sets the notification permission status to |permission_status|.
  void SetNotificationPermissionStatus(PermissionStatus permission_status) {
    ON_CALL(*permission_manager_,
            GetPermissionResultForOriginWithoutContext(
                blink::PermissionType::NOTIFICATIONS, _, _))
        .WillByDefault(Return(content::PermissionResult(
            permission_status, content::PermissionStatusSource::UNSPECIFIED)));
  }

  // TestingProfile overrides:
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override {
    return permission_manager_.get();
  }

 private:
  std::unique_ptr<content::MockPermissionManager> permission_manager_;
};

}  // namespace

class PersistentNotificationHandlerTest : public ::testing::Test {
 public:
  PersistentNotificationHandlerTest()
      : display_service_tester_(&profile_), origin_(kExampleOrigin) {}
  PersistentNotificationHandlerTest(const PersistentNotificationHandlerTest&) =
      delete;
  PersistentNotificationHandlerTest& operator=(
      const PersistentNotificationHandlerTest&) = delete;

  ~PersistentNotificationHandlerTest() override = default;

  // ::testing::Test overrides:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {}, {safe_browsing::kOnDeviceNotificationContentDetectionModel,
             safe_browsing::kShowWarningsForSuspiciousNotifications});

    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, HistoryServiceFactory::GetDefaultFactory());

    mock_logger_ = static_cast<MockNotificationMetricsLogger*>(
        NotificationMetricsLoggerFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                &profile_,
                base::BindRepeating(
                    &MockNotificationMetricsLogger::FactoryForTests)));

    PlatformNotificationServiceFactory::GetForProfile(&profile_)
        ->ClearClosedNotificationsForTesting();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileWithPermissionManager profile_;
  NotificationDisplayServiceTester display_service_tester_;

  // The origin for which these tests are being run.
  GURL origin_;

  // Owned by the |profile_| as a keyed service.
  raw_ptr<MockNotificationMetricsLogger> mock_logger_ = nullptr;
};

TEST_F(PersistentNotificationHandlerTest, OnClick_WithoutPermission) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClickWithoutPermission());
  profile_.SetNotificationPermissionStatus(PermissionStatus::DENIED);

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClick(&profile_, origin_, kExampleNotificationId,
                   std::nullopt /* action_index */, std::nullopt /* reply */,
                   base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest,
       OnClick_CloseUnactionableNotifications) {
  // Show a notification for a particular origin.
  {
    base::RunLoop run_loop;
    display_service_tester_.SetNotificationAddedClosure(run_loop.QuitClosure());

    EXPECT_CALL(*mock_logger_, LogPersistentNotificationShown());

    PlatformNotificationServiceFactory::GetForProfile(&profile_)
        ->DisplayPersistentNotification(
            kExampleNotificationId, origin_ /* service_worker_scope */, origin_,
            blink::PlatformNotificationData(), blink::NotificationResources());

    run_loop.Run();
  }

  ASSERT_TRUE(display_service_tester_.GetNotification(kExampleNotificationId));

  // Revoke permission for any origin to display notifications.
  profile_.SetNotificationPermissionStatus(PermissionStatus::DENIED);

  // Now simulate a click on the notification. It should be automatically closed
  // by the PersistentNotificationHandler.
  {
    EXPECT_CALL(*mock_logger_,
                LogPersistentNotificationClickWithoutPermission());

    display_service_tester_.SimulateClick(
        NotificationHandler::Type::WEB_PERSISTENT, kExampleNotificationId,
        std::nullopt /* action_index */, std::nullopt /* reply */);
  }

  EXPECT_FALSE(display_service_tester_.GetNotification(kExampleNotificationId));
}

TEST_F(PersistentNotificationHandlerTest, OnClose_ByUser) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClosedByUser());

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClose(&profile_, origin_, kExampleNotificationId,
                   /* by_user= */ true, base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest, OnClose_Programmatically) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClosedProgrammatically());

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClose(&profile_, origin_, kExampleNotificationId,
                   /* by_user= */ false, base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest, DisableNotifications) {
  std::unique_ptr<NotificationPermissionContext> permission_context =
      std::make_unique<NotificationPermissionContext>(&profile_);

  ASSERT_EQ(permission_context
                ->GetPermissionStatus(nullptr /* render_frame_host */, origin_,
                                      origin_)
                .status,
            PermissionStatus::ASK);

  // Set `ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER` to true for
  // `origin_`.
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(&profile_);
  hcsm->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(origin_),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER,
      base::Value(base::Value::Dict().Set(
          safe_browsing::kIsAllowlistedByUserKey, true)));

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->DisableNotifications(&profile_, origin_);

  // Disabling the permission should set
  // `ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER` to false.
  content_settings::SettingInfo info;
  base::Value value = hcsm->GetWebsiteSetting(
      origin_, origin_,
      ContentSettingsType::ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER,
      &info);
  EXPECT_EQ(
      false,
      value.GetDict().FindBool(safe_browsing::kIsAllowlistedByUserKey).value());

#if BUILDFLAG(IS_ANDROID)
  PermissionStatus kExpectedDisabledStatus = PermissionStatus::ASK;
#else
  PermissionStatus kExpectedDisabledStatus = PermissionStatus::DENIED;
#endif
  ASSERT_EQ(permission_context
                ->GetPermissionStatus(nullptr /* render_frame_host */, origin_,
                                      origin_)
                .status,
            kExpectedDisabledStatus);
}

class PersistentNotificationHandlerWithNotificationContentDetection
    : public PersistentNotificationHandlerTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    if (IsNotificationContentDetectionEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {safe_browsing::kOnDeviceNotificationContentDetectionModel},
          {safe_browsing::kShowWarningsForSuspiciousNotifications});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {safe_browsing::kOnDeviceNotificationContentDetectionModel,
               safe_browsing::kShowWarningsForSuspiciousNotifications});
    }
    if (IsSafeBrowsingEnabled()) {
      profile_.GetTestingPrefService()->SetManagedPref(
          prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(true));
    } else {
      profile_.GetTestingPrefService()->SetManagedPref(
          prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(false));
    }
    mock_notification_content_detection_service_ = static_cast<
        safe_browsing::MockNotificationContentDetectionService*>(
        safe_browsing::NotificationContentDetectionServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                &profile_,
                base::BindRepeating(
                    &safe_browsing::MockNotificationContentDetectionService::
                        FactoryForTests,
                    &model_observer_tracker_,
                    base::ThreadPool::CreateSequencedTaskRunner(
                        {base::MayBlock()}))));
  }

  bool IsSafeBrowsingEnabled() { return std::get<0>(GetParam()); }

  bool IsNotificationContentDetectionEnabled() {
    return std::get<1>(GetParam());
  }

 protected:
  raw_ptr<safe_browsing::MockNotificationContentDetectionService>
      mock_notification_content_detection_service_ = nullptr;
  safe_browsing::TestModelObserverTracker model_observer_tracker_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    PersistentNotificationHandlerWithNotificationContentDetection,
    testing::Combine(testing::Bool(), testing::Bool()));

// TODO(crbug.com/378566914): Test fails on Linux MSAN
#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
#define MAYBE_PerformNotificationContentDetectionWhenEnabled \
  DISABLED_PerformNotificationContentDetectionWhenEnabled
#else
#define MAYBE_PerformNotificationContentDetectionWhenEnabled \
  PerformNotificationContentDetectionWhenEnabled
#endif
TEST_P(PersistentNotificationHandlerWithNotificationContentDetection,
       MAYBE_PerformNotificationContentDetectionWhenEnabled) {
  base::RunLoop run_loop;
  display_service_tester_.SetNotificationAddedClosure(run_loop.QuitClosure());

  int expected_number_of_calls = 0;
  if (IsSafeBrowsingEnabled() && IsNotificationContentDetectionEnabled()) {
    expected_number_of_calls = 1;
  }
  EXPECT_CALL(*mock_notification_content_detection_service_,
              MaybeCheckNotificationContentDetectionModel(_, _, _, _))
      .Times(expected_number_of_calls);

  PlatformNotificationServiceFactory::GetForProfile(&profile_)
      ->DisplayPersistentNotification(
          kExampleNotificationId, origin_ /* service_worker_scope */, origin_,
          blink::PlatformNotificationData(), blink::NotificationResources());

  run_loop.Run();
}
