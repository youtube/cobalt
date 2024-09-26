// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_features.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/push_messaging/push_messaging_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/gcm_driver/crypto/gcm_crypto_test_helpers.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/permissions/permission_manager.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/gcm_driver/instance_id/instance_id_android.h"
#include "components/gcm_driver/instance_id/scoped_use_fake_instance_id_android.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/permission_controller.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

const char kTestOrigin[] = "https://example.com";
const char kTestSenderId[] = "1234567890";
const int64_t kTestServiceWorkerId = 42;
const char kTestPayload[] = "Hello, world!";

// NIST P-256 public key in uncompressed format per SEC1 2.3.3.
const uint8_t kTestP256Key[] = {
    0x04, 0x55, 0x52, 0x6A, 0xA5, 0x6E, 0x8E, 0xAA, 0x47, 0x97, 0x36,
    0x10, 0xC1, 0x66, 0x3C, 0x1E, 0x65, 0xBF, 0xA1, 0x7B, 0xEE, 0x48,
    0xC9, 0xC6, 0xBB, 0xBF, 0x02, 0x18, 0x53, 0x72, 0x1D, 0x0C, 0x7B,
    0xA9, 0xE3, 0x11, 0xB7, 0x03, 0x52, 0x21, 0xD3, 0x71, 0x90, 0x13,
    0xA8, 0xC1, 0xCF, 0xED, 0x20, 0xF7, 0x1F, 0xD1, 0x7F, 0xF2, 0x76,
    0xB6, 0x01, 0x20, 0xD8, 0x35, 0xA5, 0xD9, 0x3C, 0x43, 0xFD};

static_assert(sizeof(kTestP256Key) == 65,
              "The fake public key must be a valid P-256 uncompressed point.");

// URL-safe base64 encoded version of the |kTestP256Key|.
const char kTestEncodedP256Key[] =
    "BFVSaqVujqpHlzYQwWY8HmW_oXvuSMnGu78CGFNyHQx7qeMRtwNSIdNxkBOowc_tIPcf0X_ydr"
    "YBINg1pdk8Q_0";

// Implementation of the TestingProfile that provides the Push Messaging Service
// and the Permission Manager, both of which are required for the tests.
class PushMessagingTestingProfile : public TestingProfile {
 public:
  PushMessagingTestingProfile() = default;

  PushMessagingTestingProfile(const PushMessagingTestingProfile&) = delete;
  PushMessagingTestingProfile& operator=(const PushMessagingTestingProfile&) =
      delete;

  ~PushMessagingTestingProfile() override = default;

  PushMessagingServiceImpl* GetPushMessagingService() override {
    return PushMessagingServiceFactory::GetForProfile(this);
  }

  permissions::PermissionManager* GetPermissionControllerDelegate() override {
    return PermissionManagerFactory::GetForProfile(this);
  }
};

std::unique_ptr<KeyedService> BuildFakeGCMProfileService(
    content::BrowserContext* context) {
  return gcm::FakeGCMProfileService::Build(static_cast<Profile*>(context));
}

constexpr base::TimeDelta kPushEventHandleTime = base::Seconds(10);

}  // namespace

class PushMessagingServiceTest : public ::testing::Test {
 public:
  PushMessagingServiceTest() {
    // Override the GCM Profile service so that we can send fake messages.
    gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildFakeGCMProfileService));
  }

  ~PushMessagingServiceTest() override = default;

  void SetPermission(const GURL& origin, ContentSetting value) {
    HostContentSettingsMap* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(&profile_);
    host_content_settings_map->SetContentSettingDefaultScope(
        origin, origin, ContentSettingsType::NOTIFICATIONS, value);
  }

  // Callback to use when the subscription may have been subscribed.
  void DidRegister(std::string* subscription_id_out,
                   GURL* endpoint_out,
                   absl::optional<base::Time>* expiration_time_out,
                   std::vector<uint8_t>* p256dh_out,
                   std::vector<uint8_t>* auth_out,
                   base::OnceClosure done_callback,
                   const std::string& registration_id,
                   const GURL& endpoint,
                   const absl::optional<base::Time>& expiration_time,
                   const std::vector<uint8_t>& p256dh,
                   const std::vector<uint8_t>& auth,
                   blink::mojom::PushRegistrationStatus status) {
    EXPECT_EQ(blink::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE,
              status);

    *subscription_id_out = registration_id;
    *expiration_time_out = expiration_time;
    *endpoint_out = endpoint;
    *p256dh_out = p256dh;
    *auth_out = auth;

    std::move(done_callback).Run();
  }

  // Callback to use when observing messages dispatched by the push service.
  void DidDispatchMessage(
      std::string* app_id_out,
      GURL* origin_out,
      int64_t* service_worker_registration_id_out,
      absl::optional<std::string>* payload_out,
      const std::string& app_id,
      const GURL& origin,
      int64_t service_worker_registration_id,
      absl::optional<std::string> payload,
      PushMessagingServiceImpl::PushEventCallback callback) {
    *app_id_out = app_id;
    *origin_out = origin;
    *service_worker_registration_id_out = service_worker_registration_id;
    *payload_out = std::move(payload);
  }

  class TestPushSubscription {
   public:
    std::string subscription_id_;
    GURL endpoint_;
    absl::optional<base::Time> expiration_time_;
    std::vector<uint8_t> p256dh_;
    std::vector<uint8_t> auth_;
    TestPushSubscription(const std::string& subscription_id,
                         const GURL& endpoint,
                         const absl::optional<base::Time>& expiration_time,
                         const std::vector<uint8_t>& p256dh,
                         const std::vector<uint8_t>& auth)
        : subscription_id_(subscription_id),
          endpoint_(endpoint),
          expiration_time_(expiration_time),
          p256dh_(p256dh),
          auth_(auth) {}
    TestPushSubscription() = default;
  };

  void Subscribe(PushMessagingServiceImpl* push_service,
                 const GURL& origin,
                 TestPushSubscription* subscription = nullptr) {
    std::string subscription_id;
    GURL endpoint;
    absl::optional<base::Time> expiration_time;
    std::vector<uint8_t> p256dh, auth;

    base::RunLoop run_loop;

    auto options = blink::mojom::PushSubscriptionOptions::New();
    options->user_visible_only = true;
    options->application_server_key = std::vector<uint8_t>(
        kTestSenderId,
        kTestSenderId + sizeof(kTestSenderId) / sizeof(char) - 1);

    push_service->SubscribeFromWorker(
        origin, kTestServiceWorkerId, /*render_process_id=*/-1,
        std::move(options),
        base::BindOnce(&PushMessagingServiceTest::DidRegister,
                       base::Unretained(this), &subscription_id, &endpoint,
                       &expiration_time, &p256dh, &auth,
                       run_loop.QuitClosure()));

    EXPECT_EQ(0u, subscription_id.size());  // this must be asynchronous

    run_loop.Run();

    ASSERT_GT(subscription_id.size(), 0u);
    ASSERT_TRUE(endpoint.is_valid());
    ASSERT_GT(endpoint.spec().size(), 0u);
    ASSERT_GT(p256dh.size(), 0u);
    ASSERT_GT(auth.size(), 0u);

    if (subscription) {
      subscription->subscription_id_ = subscription_id;
      subscription->endpoint_ = endpoint;
      subscription->p256dh_ = p256dh;
      subscription->auth_ = auth;
    }
  }

 protected:
  PushMessagingTestingProfile* profile() { return &profile_; }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  PushMessagingTestingProfile profile_;

#if BUILDFLAG(IS_ANDROID)
  instance_id::InstanceIDAndroid::ScopedBlockOnAsyncTasksForTesting
      block_async_;
#endif  // BUILDFLAG(IS_ANDROID)
};

// Fails too often on Linux TSAN builder: http://crbug.com/1211350.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_PayloadEncryptionTest DISABLED_PayloadEncryptionTest
#else
#define MAYBE_PayloadEncryptionTest PayloadEncryptionTest
#endif
TEST_F(PushMessagingServiceTest, MAYBE_PayloadEncryptionTest) {
  PushMessagingServiceImpl* push_service = profile()->GetPushMessagingService();
  ASSERT_TRUE(push_service);

  const GURL origin(kTestOrigin);
  SetPermission(origin, CONTENT_SETTING_ALLOW);

  // (1) Make sure that |kExampleOrigin| has access to use Push Messaging.
  ASSERT_EQ(blink::mojom::PermissionStatus::GRANTED,
            push_service->GetPermissionStatus(origin, true /* user_visible */));

  // (2) Subscribe for Push Messaging, and verify that we've got the required
  // information in order to be able to create encrypted messages.
  TestPushSubscription subscription;
  Subscribe(push_service, origin, &subscription);

  // (3) Encrypt a message using the public key and authentication secret that
  // are associated with the subscription.

  gcm::IncomingMessage message;
  message.sender_id = kTestSenderId;

  ASSERT_TRUE(gcm::CreateEncryptedPayloadForTesting(
      kTestPayload,
      base::StringPiece(reinterpret_cast<char*>(subscription.p256dh_.data()),
                        subscription.p256dh_.size()),
      base::StringPiece(reinterpret_cast<char*>(subscription.auth_.data()),
                        subscription.auth_.size()),
      &message));

  ASSERT_GT(message.raw_data.size(), 0u);
  ASSERT_NE(kTestPayload, message.raw_data);
  ASSERT_FALSE(message.decrypted);

  // (4) Find the app_id that has been associated with the subscription.
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(profile(), origin,
                                                      kTestServiceWorkerId);

  ASSERT_FALSE(app_identifier.is_null());

  std::string app_id;
  GURL dispatched_origin;
  int64_t service_worker_registration_id;
  absl::optional<std::string> payload;

  // (5) Observe message dispatchings from the Push Messaging service, and
  // then dispatch the |message| on the GCM driver as if it had actually
  // been received by Google Cloud Messaging.
  push_service->SetMessageDispatchedCallbackForTesting(base::BindRepeating(
      &PushMessagingServiceTest::DidDispatchMessage, base::Unretained(this),
      &app_id, &dispatched_origin, &service_worker_registration_id, &payload));

  gcm::FakeGCMProfileService* fake_profile_service =
      static_cast<gcm::FakeGCMProfileService*>(
          gcm::GCMProfileServiceFactory::GetForProfile(profile()));

  fake_profile_service->DispatchMessage(app_identifier.app_id(), message);

  base::RunLoop().RunUntilIdle();

  // (6) Verify that the message, as received by the Push Messaging Service, has
  // indeed been decrypted by the GCM Driver, and has been forwarded to the
  // Service Worker that has been associated with the subscription.
  EXPECT_EQ(app_identifier.app_id(), app_id);
  EXPECT_EQ(origin, dispatched_origin);
  EXPECT_EQ(service_worker_registration_id, kTestServiceWorkerId);

  EXPECT_TRUE(payload);
  EXPECT_EQ(kTestPayload, *payload);
}

TEST_F(PushMessagingServiceTest, NormalizeSenderInfo) {
  PushMessagingServiceImpl* push_service = profile()->GetPushMessagingService();
  ASSERT_TRUE(push_service);

  std::string p256dh(kTestP256Key, kTestP256Key + std::size(kTestP256Key));
  ASSERT_EQ(65u, p256dh.size());

  // NIST P-256 public keys in uncompressed format will be encoded using the
  // URL-safe base64 encoding by the normalization function.
  EXPECT_EQ(kTestEncodedP256Key, push_messaging::NormalizeSenderInfo(p256dh));

  // Any other value, binary or not, will be passed through as-is.
  EXPECT_EQ("1234567890", push_messaging::NormalizeSenderInfo("1234567890"));
  EXPECT_EQ("foo@bar.com", push_messaging::NormalizeSenderInfo("foo@bar.com"));

  p256dh[0] = 0x05;  // invalidate |p256dh| as a public key.

  EXPECT_EQ(p256dh, push_messaging::NormalizeSenderInfo(p256dh));
}

// Fails too often on Linux TSAN builder: http://crbug.com/1211350.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_RemoveExpiredSubscriptions DISABLED_RemoveExpiredSubscriptions
#else
#define MAYBE_RemoveExpiredSubscriptions RemoveExpiredSubscriptions
#endif
TEST_F(PushMessagingServiceTest, MAYBE_RemoveExpiredSubscriptions) {
  // (1) Enable push subscriptions with expiration time and
  // `pushsubscriptionchange` events
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      /* enabled features */
      {features::kPushSubscriptionWithExpirationTime,
       features::kPushSubscriptionChangeEvent},
      /* disabled features */
      {});

  const GURL origin(kTestOrigin);
  SetPermission(origin, CONTENT_SETTING_ALLOW);

  // (2) Set up push service and test origin
  PushMessagingServiceImpl* push_service = profile()->GetPushMessagingService();
  ASSERT_TRUE(push_service);

  // (3) Subscribe origin to push service and find corresponding
  // |app_identifier|
  Subscribe(push_service, origin);
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(profile(), origin,
                                                      kTestServiceWorkerId);
  ASSERT_FALSE(app_identifier.is_null());

  // (4) Manually set the time as expired, save the time in preferences
  app_identifier.set_expiration_time(base::Time::UnixEpoch());
  app_identifier.PersistToPrefs(profile());
  ASSERT_EQ(1u, PushMessagingAppIdentifier::GetCount(profile()));

  // (3) Remove all expired subscriptions
  base::RunLoop run_loop;
  push_service->SetRemoveExpiredSubscriptionsCallbackForTesting(
      run_loop.QuitClosure());
  push_service->RemoveExpiredSubscriptions();
  run_loop.Run();

  // (5) We expect the subscription to be deleted
  ASSERT_EQ(0u, PushMessagingAppIdentifier::GetCount(profile()));
  PushMessagingAppIdentifier deleted_identifier =
      PushMessagingAppIdentifier::FindByAppId(profile(),
                                              app_identifier.app_id());
  EXPECT_TRUE(deleted_identifier.is_null());
}

TEST_F(PushMessagingServiceTest, TestMultipleIncomingPushMessages) {
  base::HistogramTester histograms;

  const GURL origin(kTestOrigin);
  SetPermission(origin, CONTENT_SETTING_ALLOW);

  PushMessagingServiceImpl* push_service = profile()->GetPushMessagingService();
  ASSERT_TRUE(push_service);

  // Subscribe |origin| to push service.
  Subscribe(push_service, origin);
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(profile(), origin,
                                                      kTestServiceWorkerId);
  ASSERT_FALSE(app_identifier.is_null());

  // Setup decrypted test message.
  gcm::IncomingMessage message;
  message.sender_id = kTestSenderId;
  message.raw_data = "testdata";
  message.decrypted = true;

  // Setup callbacks for dispatch and handled push events.
  auto dispatched_run_loop = std::make_unique<base::RunLoop>();
  auto handled_run_loop = std::make_unique<base::RunLoop>();
  PushMessagingServiceImpl::PushEventCallback handle_push_event;

  push_service->SetMessageDispatchedCallbackForTesting(
      base::BindLambdaForTesting(
          [&](const std::string& app_id, const GURL& origin,
              int64_t service_worker_registration_id,
              absl::optional<std::string> payload,
              PushMessagingServiceImpl::PushEventCallback callback) {
            handle_push_event = std::move(callback);
            dispatched_run_loop->Quit();
          }));

  push_service->SetMessageCallbackForTesting(
      base::BindLambdaForTesting([&]() { handled_run_loop->Quit(); }));

  // Simulate two incoming push messages at the same time.
  push_service->OnMessage(app_identifier.app_id(), message);
  push_service->OnMessage(app_identifier.app_id(), message);

  // First wait until we dispatched the first push message.
  dispatched_run_loop->Run();
  dispatched_run_loop = std::make_unique<base::RunLoop>();
  auto handled_first = std::move(handle_push_event);
  handle_push_event = PushMessagingServiceImpl::PushEventCallback();

  histograms.ExpectUniqueTimeSample("PushMessaging.CheckOriginForAbuseTime",
                                    base::Seconds(0),
                                    /*expected_bucket_count=*/1);
  histograms.ExpectUniqueTimeSample("PushMessaging.DeliverQueuedMessageTime",
                                    base::Seconds(0),
                                    /*expected_bucket_count=*/1);

  // Run all tasks until idle so we can verify that we don't dispatch the second
  // push message until the first one is handled.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handle_push_event);

  // Simulate handling the first push event takes some time.
  task_environment().FastForwardBy(kPushEventHandleTime);

  // Now signal that the first push event has been handled and wait until we
  // checked for visibility requirements.
  std::move(handled_first).Run(blink::mojom::PushEventStatus::SUCCESS);
  handled_run_loop->Run();
  handled_run_loop = std::make_unique<base::RunLoop>();

  histograms.ExpectUniqueTimeSample("PushMessaging.MessageHandledTime",
                                    kPushEventHandleTime,
                                    /*expected_bucket_count=*/1);

  // Simulate handling the second push event takes some time.
  task_environment().FastForwardBy(kPushEventHandleTime);

  // Now wait until we dispatched the second push message and handle it too.
  dispatched_run_loop->Run();
  std::move(handle_push_event).Run(blink::mojom::PushEventStatus::SUCCESS);
  handled_run_loop->Run();

  // Checking origins for abuse happens immediately on receiving a push message
  // one at a time. Both messages do that instantly in this test.
  histograms.ExpectTimeBucketCount("PushMessaging.CheckOriginForAbuseTime",
                                   base::Seconds(0),
                                   /*count=*/2);
  // Delivering messages should be done in series so the second message should
  // have waited for the first one to be handled.
  histograms.ExpectTimeBucketCount("PushMessaging.DeliverQueuedMessageTime",
                                   kPushEventHandleTime,
                                   /*count=*/1);
  // The total time from receiving until handling of the second message.
  histograms.ExpectTimeBucketCount("PushMessaging.MessageHandledTime",
                                   kPushEventHandleTime * 2,
                                   /*count=*/1);
}

#if BUILDFLAG(IS_ANDROID)
class FCMRevocationTest : public PushMessagingServiceTest {
 public:
  FCMRevocationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kRevokeNotificationsPermissionIfDisabledOnAppLevel);
    PushMessagingServiceImpl::RegisterPrefs(prefs_.registry());
  }

  ~FCMRevocationTest() override = default;

  GURL GetUrl() { return origin_; }

  url::Origin GetOrigin() { return url::Origin::Create(origin_); }

  PrefService* pref() { return &prefs_; }

  void SetPermission(const GURL& origin,
                     ContentSetting value,
                     TestingProfile* profile) {
    HostContentSettingsMap* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile);
    host_content_settings_map->SetContentSettingDefaultScope(
        origin, origin, ContentSettingsType::NOTIFICATIONS, value);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  GURL origin_ = GURL("https://example.com");
  TestingPrefServiceSimple prefs_;
};

TEST_F(FCMRevocationTest, ResetPrefs) {
  content::PermissionController* permission_controller =
      profile()->GetPermissionController();

  content::PermissionResult result =
      permission_controller->GetPermissionResultForOriginWithoutContext(
          blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::ASK);

  SetPermission(GetUrl(), ContentSetting::CONTENT_SETTING_ALLOW, profile());

  result = permission_controller->GetPermissionResultForOriginWithoutContext(
      blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::GRANTED);

  const char kNotificationsPermissionRevocationGracePeriodDate[] =
      "notifications_permission_revocation_grace_period";

  // Just random value to make sure it is reset.
  base::Time time = base::Time::FromTimeT(100);

  pref()->SetTime(kNotificationsPermissionRevocationGracePeriodDate, time);

  PushMessagingServiceImpl::RevokePermissionIfPossible(
      GetUrl(), /*app_level_notifications_enabled=*/true, pref(), profile());

  // Time is reset.
  base::Time grace_period_date =
      pref()->GetTime(kNotificationsPermissionRevocationGracePeriodDate);
  EXPECT_EQ(grace_period_date, base::Time());
  EXPECT_NE(grace_period_date, time);

  // Permission is not reset.
  result = permission_controller->GetPermissionResultForOriginWithoutContext(
      blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::GRANTED);
}

// This test verifies that if the grace period is not started, and there is no
// app-level Notifications permissions, we init the grace period prefs without
// revoking permissions.
TEST_F(FCMRevocationTest, NoAppLevelPermissionInitGracePeriodPrefsTest) {
  content::PermissionController* permission_controller =
      profile()->GetPermissionController();

  content::PermissionResult result =
      permission_controller->GetPermissionResultForOriginWithoutContext(
          blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::ASK);

  SetPermission(GetUrl(), ContentSetting::CONTENT_SETTING_ALLOW, profile());

  result = permission_controller->GetPermissionResultForOriginWithoutContext(
      blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::GRANTED);

  const char kNotificationsPermissionRevocationGracePeriodDate[] =
      "notifications_permission_revocation_grace_period";

  // The grace period is not initialized.
  EXPECT_EQ(pref()->GetTime(kNotificationsPermissionRevocationGracePeriodDate),
            base::Time());

  PushMessagingServiceImpl::RevokePermissionIfPossible(
      GetUrl(), /*app_level_notifications_enabled=*/false, pref(), profile());

  // The grace period is initialized with non-default time value.
  EXPECT_NE(pref()->GetTime(kNotificationsPermissionRevocationGracePeriodDate),
            base::Time());

  // Permission is still granted.
  result = permission_controller->GetPermissionResultForOriginWithoutContext(
      blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::GRANTED);
}

// This test verifies that if the grace period is over and there is no app-level
// Notifications permissions, site-level Notifications permission will be
// revoked.
TEST_F(FCMRevocationTest, NoAppLevelPermissionRevocationTest) {
  content::PermissionController* permission_controller =
      profile()->GetPermissionController();

  content::PermissionResult result =
      permission_controller->GetPermissionResultForOriginWithoutContext(
          blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::ASK);

  SetPermission(GetUrl(), ContentSetting::CONTENT_SETTING_ALLOW, profile());

  result = permission_controller->GetPermissionResultForOriginWithoutContext(
      blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::GRANTED);

  const char kNotificationsPermissionRevocationGracePeriodDate[] =
      "notifications_permission_revocation_grace_period";

  // The grace period is not initialized.
  EXPECT_EQ(pref()->GetTime(kNotificationsPermissionRevocationGracePeriodDate),
            base::Time());

  // Init `time` with 4 days old time value.
  const base::Time time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Time::Now() -
      base::Time::FromDeltaSinceWindowsEpoch(base::Days(4)));

  // Init the grace period date with a value that is older than 3 days (the
  // default grace period).
  pref()->SetTime(kNotificationsPermissionRevocationGracePeriodDate, time);

  PushMessagingServiceImpl::RevokePermissionIfPossible(
      GetUrl(), /*app_level_notifications_enabled=*/false, pref(), profile());

  EXPECT_EQ(pref()->GetTime(kNotificationsPermissionRevocationGracePeriodDate),
            time);

  // Permission is revoked.
  result = permission_controller->GetPermissionResultForOriginWithoutContext(
      blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::ASK);
}

// This test verifies that if the grace period is not over and there is no
// app-level Notifications permissions, site-level Notifications permission will
// not be revoked.
TEST_F(FCMRevocationTest, NoAppLevelPermissionIgnoreTest) {
  content::PermissionController* permission_controller =
      profile()->GetPermissionController();

  content::PermissionResult result =
      permission_controller->GetPermissionResultForOriginWithoutContext(
          blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::ASK);

  SetPermission(GetUrl(), ContentSetting::CONTENT_SETTING_ALLOW, profile());

  result = permission_controller->GetPermissionResultForOriginWithoutContext(
      blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::GRANTED);

  const char kNotificationsPermissionRevocationGracePeriodDate[] =
      "notifications_permission_revocation_grace_period";

  // The grace period is not initialized.
  EXPECT_EQ(pref()->GetTime(kNotificationsPermissionRevocationGracePeriodDate),
            base::Time());

  // Init `time` with 2 days old time value.
  const base::Time time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Time::Now() -
      base::Time::FromDeltaSinceWindowsEpoch(base::Days(2)));
  // Init the grace period date with a value that is fewer than 3 days (the
  // default grace period).
  pref()->SetTime(kNotificationsPermissionRevocationGracePeriodDate, time);

  PushMessagingServiceImpl::RevokePermissionIfPossible(
      GetUrl(), /*app_level_notifications_enabled=*/false, pref(), profile());

  EXPECT_EQ(pref()->GetTime(kNotificationsPermissionRevocationGracePeriodDate),
            time);

  // Permission is revoked.
  result = permission_controller->GetPermissionResultForOriginWithoutContext(
      blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::GRANTED);
}

// This test verifies that if the grace period is not over and there is
// app-level Notifications permissions, the grace period reset will be tracked.
TEST_F(FCMRevocationTest, ResetAndRecordGracePeriodTest) {
  content::PermissionController* permission_controller =
      profile()->GetPermissionController();

  content::PermissionResult result =
      permission_controller->GetPermissionResultForOriginWithoutContext(
          blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::ASK);

  SetPermission(GetUrl(), ContentSetting::CONTENT_SETTING_ALLOW, profile());

  result = permission_controller->GetPermissionResultForOriginWithoutContext(
      blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::GRANTED);

  const char kNotificationsPermissionRevocationGracePeriodDate[] =
      "notifications_permission_revocation_grace_period";

  // The grace period is not initialized.
  EXPECT_EQ(pref()->GetTime(kNotificationsPermissionRevocationGracePeriodDate),
            base::Time());

  // Init `time` with 2 days old time value.
  const base::Time time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Time::Now() -
      base::Time::FromDeltaSinceWindowsEpoch(base::Days(2)));
  // Init the grace period date with a value that is fewer than 3 days (the
  // default grace period).
  pref()->SetTime(kNotificationsPermissionRevocationGracePeriodDate, time);

  PushMessagingServiceImpl::RevokePermissionIfPossible(
      GetUrl(), /*app_level_notifications_enabled=*/true, pref(), profile());

  EXPECT_EQ(pref()->GetTime(kNotificationsPermissionRevocationGracePeriodDate),
            base::Time());

  // Permission is revoked.
  result = permission_controller->GetPermissionResultForOriginWithoutContext(
      blink::PermissionType::NOTIFICATIONS, GetOrigin());
  EXPECT_EQ(result.status, blink::mojom::PermissionStatus::GRANTED);
}

#endif
