// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/extension_event_observer.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/common/extensions/api/gcm.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ExtensionEventObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  ExtensionEventObserverTest()
      : fake_user_manager_(new FakeChromeUserManager()),
        scoped_user_manager_enabler_(
            base::WrapUnique(fake_user_manager_.get())) {}

  ExtensionEventObserverTest(const ExtensionEventObserverTest&) = delete;
  ExtensionEventObserverTest& operator=(const ExtensionEventObserverTest&) =
      delete;

  ~ExtensionEventObserverTest() override = default;

  // ChromeRenerViewHostTestHarness overrides:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    chromeos::PowerManagerClient::InitializeFake();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());

    // Must be called from ::testing::Test::SetUp.
    ASSERT_TRUE(profile_manager_->SetUp());

    extension_event_observer_ = std::make_unique<ExtensionEventObserver>();
    test_api_ = extension_event_observer_->CreateTestApi();

    const char kUserProfile[] = "profile1@example.com";
    const AccountId account_id(AccountId::FromUserEmail(kUserProfile));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    profile_ =
        profile_manager_->CreateTestingProfile(account_id.GetUserEmail());
  }

  void TearDown() override {
    extension_event_observer_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    chromeos::PowerManagerClient::Shutdown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  scoped_refptr<const extensions::Extension> CreateApp(const std::string& name,
                                                       bool uses_gcm) {
    scoped_refptr<const extensions::Extension> app =
        extensions::ExtensionBuilder()
            .SetManifest(
                base::Value::Dict()
                    .Set("name", name)
                    .Set("version", "1.0.0")
                    .Set("manifest_version", 2)
                    .Set("app", base::Value::Dict().Set(
                                    "background",
                                    base::Value::Dict().Set(
                                        "scripts", base::Value::List().Append(
                                                       "background.js"))))
                    .Set("permissions",
                         base::Value::List().Append(uses_gcm ? "gcm" : "")))
            .Build();

    created_apps_.push_back(app);

    return app;
  }

  extensions::ExtensionHost* CreateHostForApp(
      Profile* profile,
      const extensions::Extension* app) {
    extensions::ProcessManager::Get(profile)->CreateBackgroundHost(
        app, extensions::BackgroundInfo::GetBackgroundURL(app));
    base::RunLoop().RunUntilIdle();

    return extensions::ProcessManager::Get(profile)
        ->GetBackgroundHostForExtension(app->id());
  }

  std::unique_ptr<ExtensionEventObserver> extension_event_observer_;
  std::unique_ptr<ExtensionEventObserver::TestApi> test_api_;

  // Owned by |profile_manager_|.
  raw_ptr<TestingProfile, ExperimentalAsh> profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  // Chrome OS needs the CrosSettings test helper.
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;

  // Owned by |scoped_user_manager_enabler_|.
  raw_ptr<FakeChromeUserManager, DanglingUntriaged | ExperimentalAsh>
      fake_user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_enabler_;

  std::vector<scoped_refptr<const extensions::Extension>> created_apps_;
};

// Tests that the ExtensionEventObserver reports readiness for suspend when
// there is nothing interesting going on.
TEST_F(ExtensionEventObserverTest, BasicSuspendAndDarkSuspend) {
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  chromeos::FakePowerManagerClient::Get()->SendDarkSuspendImminent();
  EXPECT_EQ(1, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());
}

// Tests that the ExtensionEventObserver properly handles a canceled suspend
// attempt.
TEST_F(ExtensionEventObserverTest, CanceledSuspend) {
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_FALSE(test_api_->MaybeRunSuspendReadinessCallback());
}

// Tests that the ExtensionEventObserver delays suspends and dark suspends while
// there is a push message pending for an app that uses GCM.
TEST_F(ExtensionEventObserverTest, PushMessagesDelaySuspend) {
  scoped_refptr<const extensions::Extension> gcm_app =
      CreateApp("DelaysSuspendForPushMessages", true /* uses_gcm */);
  extensions::ExtensionHost* host = CreateHostForApp(profile_, gcm_app.get());
  ASSERT_TRUE(host);
  EXPECT_TRUE(test_api_->WillDelaySuspendForExtensionHost(host));

  // Test that a push message received before a suspend attempt delays the
  // attempt.
  const int kSuspendPushId = 23874;
  extension_event_observer_->OnBackgroundEventDispatched(
      host, extensions::api::gcm::OnMessage::kEventName, kSuspendPushId);
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(1, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  extension_event_observer_->OnBackgroundEventAcked(host, kSuspendPushId);
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  // Now test receiving the suspend attempt before the push message.
  const int kDarkSuspendPushId = 56674;
  chromeos::FakePowerManagerClient::Get()->SendDarkSuspendImminent();
  extension_event_observer_->OnBackgroundEventDispatched(
      host, extensions::api::gcm::OnMessage::kEventName, kDarkSuspendPushId);

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(1, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  extension_event_observer_->OnBackgroundEventAcked(host, kDarkSuspendPushId);
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  // Test that non-push messages do not delay the suspend.
  const int kNonPushId = 5687;
  chromeos::FakePowerManagerClient::Get()->SendDarkSuspendImminent();
  extension_event_observer_->OnBackgroundEventDispatched(host, "FakeMessage",
                                                         kNonPushId);

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());
}

// Tests that messages sent for apps that don't use GCM are ignored.
TEST_F(ExtensionEventObserverTest, IgnoresNonGCMApps) {
  scoped_refptr<const extensions::Extension> app = CreateApp("Non-GCM", false);
  extensions::ExtensionHost* host = CreateHostForApp(profile_, app.get());
  ASSERT_TRUE(host);

  EXPECT_FALSE(test_api_->WillDelaySuspendForExtensionHost(host));

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());
}

// Tests that network requests started by an app while it is processing a push
// message delay any suspend attempt.
TEST_F(ExtensionEventObserverTest, NetworkRequestsMayDelaySuspend) {
  scoped_refptr<const extensions::Extension> app =
      CreateApp("NetworkRequests", true);
  extensions::ExtensionHost* host = CreateHostForApp(profile_, app.get());
  ASSERT_TRUE(host);
  EXPECT_TRUE(test_api_->WillDelaySuspendForExtensionHost(host));

  // Test that network requests started while there is no pending push message
  // are ignored.
  const uint64_t kNonPushRequestId = 5170725;
  extension_event_observer_->OnNetworkRequestStarted(host, kNonPushRequestId);
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  // Test that network requests started while a push message is pending delay
  // the suspend even after the push message has been acked.
  const int kPushMessageId = 178674;
  const uint64_t kNetworkRequestId = 78917089;
  chromeos::FakePowerManagerClient::Get()->SendDarkSuspendImminent();
  extension_event_observer_->OnBackgroundEventDispatched(
      host, extensions::api::gcm::OnMessage::kEventName, kPushMessageId);

  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(1, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  extension_event_observer_->OnNetworkRequestStarted(host, kNetworkRequestId);
  extension_event_observer_->OnBackgroundEventAcked(host, kPushMessageId);
  EXPECT_EQ(1, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  extension_event_observer_->OnNetworkRequestDone(host, kNetworkRequestId);
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());
}

// Tests that any outstanding push messages or network requests for an
// ExtensionHost that is destroyed do not end up blocking system suspend.
TEST_F(ExtensionEventObserverTest, DeletedExtensionHostDoesNotBlockSuspend) {
  scoped_refptr<const extensions::Extension> app =
      CreateApp("DeletedExtensionHost", true);

  extensions::ExtensionHost* host = CreateHostForApp(profile_, app.get());
  ASSERT_TRUE(host);
  EXPECT_TRUE(test_api_->WillDelaySuspendForExtensionHost(host));

  const int kPushId = 156178;
  const uint64_t kNetworkId = 791605;
  extension_event_observer_->OnBackgroundEventDispatched(
      host, extensions::api::gcm::OnMessage::kEventName, kPushId);
  extension_event_observer_->OnNetworkRequestStarted(host, kNetworkId);

  // Now delete the ExtensionHosts.
  host->Close();

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_TRUE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());
}

// Tests that the ExtensionEventObserver does not delay suspend attempts when it
// is disabled.
TEST_F(ExtensionEventObserverTest, DoesNotDelaySuspendWhenDisabled) {
  scoped_refptr<const extensions::Extension> app =
      CreateApp("NoDelayWhenDisabled", true);
  extensions::ExtensionHost* host = CreateHostForApp(profile_, app.get());
  ASSERT_TRUE(host);
  EXPECT_TRUE(test_api_->WillDelaySuspendForExtensionHost(host));

  // Test that disabling the suspend delay while a suspend is pending will cause
  // the ExtensionEventObserver to immediately report readiness.
  const int kPushId = 416753;
  extension_event_observer_->OnBackgroundEventDispatched(
      host, extensions::api::gcm::OnMessage::kEventName, kPushId);
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  extension_event_observer_->SetShouldDelaySuspend(false);
  EXPECT_FALSE(test_api_->MaybeRunSuspendReadinessCallback());
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());

  // Test that the ExtensionEventObserver does not delay suspend attempts when
  // it is disabled.
  chromeos::FakePowerManagerClient::Get()->SendDarkSuspendImminent();
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());
}

}  // namespace ash
