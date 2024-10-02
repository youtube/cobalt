// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting {
namespace {

constexpr char kDMToken[] = "token";

Record GetNextRecord(chromeos::MissiveClientTestObserver* observer) {
  const std::tuple<Priority, Record>& enqueued_record =
      observer->GetNextEnqueuedRecord();
  Priority priority = std::get<0>(enqueued_record);
  Record record = std::get<1>(enqueued_record);

  EXPECT_THAT(priority, Eq(Priority::SLOW_BATCH));
  return record;
}

class AudioEventsBrowserTest : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  AudioEventsBrowserTest() {
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidTokenForTesting(kDMToken));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ::policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    if (content::IsPreTest()) {
      // Preliminary setup - set up affiliated user
      ::policy::AffiliationTestHelper::PreLoginUser(
          affiliation_mixin_.account_id());
      return;
    }

    // Login as affiliated user otherwise
    ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  }

  void EnablePolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceAudioStatus, true);
  }

  void DisablePolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceAudioStatus, false);
  }

  ::policy::DevicePolicyCrosTestHelper test_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(AudioEventsBrowserTest,
                       PRE_AudioSevereUnderrunAffiliatedUserAndPolicyEnabled) {
  // Dummy case that sets up the affiliated user through SetUpOnMain
  // PRE-condition.
}

IN_PROC_BROWSER_TEST_F(AudioEventsBrowserTest,
                       AudioSevereUnderrunAffiliatedUserAndPolicyEnabled) {
  chromeos::MissiveClientTestObserver missive_observer_(
      ::reporting::Destination::EVENT_METRIC);

  EnablePolicy();

  ash::cros_healthd::mojom::AudioEventInfo info;
  info.state = ash::cros_healthd::mojom::AudioEventInfo::State::kSevereUnderrun;
  ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
      ash::cros_healthd::mojom::EventCategoryEnum::kAudio,
      ash::cros_healthd::mojom::EventInfo::NewAudioEventInfo(info.Clone()));

  const Record& record = GetNextRecord(&missive_observer_);
  MetricData record_data;
  ASSERT_TRUE(record_data.ParseFromString(record.data()));

  // Testing event found successfully.
  EXPECT_THAT(record_data.event_data().type(),
              Eq(::reporting::MetricEventType::AUDIO_SEVERE_UNDERRUN));
  ASSERT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), ::testing::StrEq(kDMToken));
}

}  // namespace
}  // namespace reporting
