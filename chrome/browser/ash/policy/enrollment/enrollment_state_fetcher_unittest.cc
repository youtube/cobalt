// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"
#include <algorithm>
#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_test_support.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/system_clock/fake_system_clock_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

namespace em = enterprise_management;

const char kTestStateKey[] = "test-state-key";
const char kTestSerialNumber[] = "test-serial-number";
const char kTestBrandCode[] = "GOOG";
const char kTestPsmId[] = "474F4F47/test-serial-number";
const char kTestDisabledMessage[] = "test-disabled-message";

class MockStateKeyBroker : public ServerBackedStateKeysBroker {
 public:
  MockStateKeyBroker() : ServerBackedStateKeysBroker(nullptr) {}
  ~MockStateKeyBroker() override = default;

  MOCK_METHOD(void, RequestStateKeys, (StateKeysCallback), (override));
};

class MockDeviceSettingsService : public ash::DeviceSettingsService {
 public:
  MOCK_METHOD(void,
              GetOwnershipStatusAsync,
              (OwnershipStatusCallback callback),
              (override));
};

std::unique_ptr<EnrollmentStateFetcher::RlweClient> CreateRlweClientForTesting(
    const psm::testing::RlweTestCase& test_case,
    const private_membership::rlwe::RlwePlaintextId& plaintext_id) {
  // Below we use test_case.plaintext_id() instead of the computed plaintext_id
  // to ensure that Query/OPRF requests and responses match the ones in the
  // test_case. Hence we check that computed plaintext_id is correct here.
  EXPECT_EQ(plaintext_id.sensitive_id(), kTestPsmId);
  EXPECT_TRUE(plaintext_id.non_sensitive_id().empty());
  auto client =
      private_membership::rlwe::PrivateMembershipRlweClient::CreateForTesting(
          private_membership::rlwe::RlweUseCase::CROS_DEVICE_STATE,
          {test_case.plaintext_id()}, test_case.ec_cipher_key(),
          test_case.seed());
  return std::move(client.value());
}

em::DeviceManagementResponse CreatePsmOprfResponse(
    const psm::testing::RlweTestCase& test_case) {
  em::DeviceManagementResponse response;
  auto* rlwe_response = response.mutable_private_set_membership_response()
                            ->mutable_rlwe_response();
  *rlwe_response->mutable_oprf_response() = test_case.oprf_response();
  return response;
}

MATCHER_P(JobWithPsmRlweRequest, rlwe_request_matcher, "") {
  DeviceManagementService::JobConfiguration* config =
      arg.GetConfigurationForTesting();
  if (config->GetType() != DeviceManagementService::JobConfiguration::
                               TYPE_PSM_HAS_DEVICE_STATE_REQUEST) {
    return false;
  }
  em::DeviceManagementRequest request;
  request.ParseFromString(config->GetPayload());
  if (!request.private_set_membership_request().has_rlwe_request()) {
    return false;
  }
  return testing::Matches(rlwe_request_matcher)(
      request.private_set_membership_request().rlwe_request());
}

MATCHER_P3(JobWithStateRequest, state_key, serial_number, brand_code, "") {
  DeviceManagementService::JobConfiguration* config =
      arg.GetConfigurationForTesting();
  if (config->GetType() !=
      DeviceManagementService::JobConfiguration::TYPE_DEVICE_STATE_RETRIEVAL) {
    return false;
  }
  em::DeviceManagementRequest request;
  request.ParseFromString(config->GetPayload());
  if (!request.has_device_state_retrieval_request()) {
    return false;
  }
  const auto& state_request = request.device_state_retrieval_request();
  return state_request.server_backed_state_key() == state_key &&
         state_request.serial_number() == serial_number &&
         state_request.brand_code() == brand_code;
}

MATCHER_P(WithOprfRequestFor, test_case, "") {
  if (!test_case) {
    return arg.has_oprf_request();
  }
  return arg.oprf_request().SerializeAsString() ==
         test_case->expected_oprf_request().SerializeAsString();
}

MATCHER_P(WithQueryRequestFor, test_case, "") {
  return arg.query_request().SerializeAsString() ==
         test_case->expected_query_request().SerializeAsString();
}

}  // namespace

class EnrollmentStateFetcherTest : public testing::Test {
 public:
  void SetUp() override {
    psm_test_case_ = psm::testing::LoadTestCase(/*is_member=*/true);
    fake_dm_service_ =
        std::make_unique<FakeDeviceManagementService>(&job_creation_handler_);
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableUnifiedStateDetermination,
        AutoEnrollmentTypeChecker::kUnifiedStateDeterminationAlways);
    system_clock_.SetNetworkSynchronized(true);

    DeviceCloudPolicyManagerAsh::RegisterPrefs(local_state_.registry());
    EnrollmentStateFetcher::RegisterPrefs(local_state_.registry());

    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);
    statistics_provider_.SetMachineStatistic(
        ash::system::kSerialNumberKeyForTest, kTestSerialNumber);
    statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                             kTestBrandCode);
  }

  AutoEnrollmentState FetchEnrollmentState() {
    base::test::TestFuture<AutoEnrollmentState> future;
    auto fetcher = EnrollmentStateFetcher::Create(
        future.GetCallback(), &local_state_,
        base::BindRepeating(&CreateRlweClientForTesting, psm_test_case_),
        fake_dm_service_.get(), shared_url_loader_factory_, &system_clock_,
        &state_key_broker_, &device_settings_service_);
    fetcher->Start();
    return future.Get();
  }

 protected:
  void ExpectOwnershipCheck() {
    EXPECT_CALL(device_settings_service_, GetOwnershipStatusAsync)
        .WillOnce(base::test::RunOnceCallback<0>(
            ash::DeviceSettingsService::OWNERSHIP_NONE));
  }

  void ExpectStateKeysRequest() {
    EXPECT_CALL(state_key_broker_, RequestStateKeys)
        .WillOnce(base::test::RunOnceCallback<0>(
            std::vector<std::string>{kTestStateKey}));
  }

  void ExpectOprfRequest(bool any = false) {
    EXPECT_CALL(job_creation_handler_,
                OnJobCreation(JobWithPsmRlweRequest(
                    WithOprfRequestFor((any ? nullptr : &psm_test_case_)))))
        .WillOnce(fake_dm_service_->SendJobOKAsync(
            CreatePsmOprfResponse(psm_test_case_)));
  }

  void ExpectQueryRequest() {
    em::DeviceManagementResponse response;
    auto* rlwe_response = response.mutable_private_set_membership_response()
                              ->mutable_rlwe_response();
    *rlwe_response->mutable_query_response() = psm_test_case_.query_response();
    EXPECT_CALL(job_creation_handler_,
                OnJobCreation(JobWithPsmRlweRequest(
                    WithQueryRequestFor(&psm_test_case_))))
        .WillOnce(fake_dm_service_->SendJobOKAsync(response));
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedCommandLine command_line_;
  TestingPrefServiceSimple local_state_;
  ash::FakeSystemClockClient system_clock_;
  ash::system::FakeStatisticsProvider statistics_provider_;
  ash::ScopedStubInstallAttributes install_attributes_;
  MockStateKeyBroker state_key_broker_;
  MockDeviceSettingsService device_settings_service_;
  psm::testing::RlweTestCase psm_test_case_;

  // Fake URL loader factories.
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  // Fake DMService.
  MockJobCreationHandler job_creation_handler_;
  std::unique_ptr<FakeDeviceManagementService> fake_dm_service_;
};

TEST_F(EnrollmentStateFetcherTest, RegisterPrefs) {
  TestingPrefServiceSimple local_state;
  auto* registry = local_state.registry();

  DeviceCloudPolicyManagerAsh::RegisterPrefs(registry);
  EnrollmentStateFetcher::RegisterPrefs(registry);

  const base::Value* value;
  auto defaults = registry->defaults();
  ASSERT_TRUE(defaults->GetValue(prefs::kServerBackedDeviceState, &value));
  ASSERT_TRUE(value->is_dict());
  EXPECT_TRUE(value->GetDict().empty());
  ASSERT_TRUE(defaults->GetValue(prefs::kEnrollmentPsmResult, &value));
  EXPECT_EQ(value->GetInt(), -1);
  ASSERT_TRUE(
      defaults->GetValue(prefs::kEnrollmentPsmDeterminationTime, &value));
  EXPECT_EQ(value->GetString(), "0");
}

TEST_F(EnrollmentStateFetcherTest, DisabledViaSwitches) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, SystemClockNotSyncronized) {
  system_clock_.DisableService();

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kConnectionError);
}

TEST_F(EnrollmentStateFetcherTest, EmbargoDateNotPassed) {
  base::Time::Exploded exploded;
  base::Time embargo_date = base::Time::Now() + base::Days(7);
  embargo_date.UTCExplode(&exploded);
  statistics_provider_.SetMachineStatistic(
      ash::system::kRlzEmbargoEndDateKey,
      base::StringPrintf("%04d-%02d-%02d", exploded.year, exploded.month,
                         exploded.day_of_month));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, RlzBrandCodeMissing) {
  statistics_provider_.ClearMachineStatistic(ash::system::kRlzBrandCodeKey);

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, SerialNumberMissing) {
  statistics_provider_.ClearMachineStatistic(
      ash::system::kSerialNumberKeyForTest);

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, OwnershipTaken) {
  EXPECT_CALL(device_settings_service_, GetOwnershipStatusAsync)
      .WillOnce(base::test::RunOnceCallback<0>(
          ash::DeviceSettingsService::OWNERSHIP_TAKEN));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, OwnershipUnknown) {
  EXPECT_CALL(device_settings_service_, GetOwnershipStatusAsync)
      .WillOnce(base::test::RunOnceCallback<0>(
          ash::DeviceSettingsService::OWNERSHIP_UNKNOWN));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, StateKeysMissing) {
  ExpectOwnershipCheck();
  EXPECT_CALL(state_key_broker_, RequestStateKeys)
      .WillRepeatedly(
          base::test::RunOnceCallback<0>(std::vector<std::string>{}));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, EmptyOprfResponse) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  EXPECT_CALL(
      job_creation_handler_,
      OnJobCreation(JobWithPsmRlweRequest(WithOprfRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobOKAsync(""));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kServerError);
}

TEST_F(EnrollmentStateFetcherTest, ConnectionErrorOnOprfRequest) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  EXPECT_CALL(
      job_creation_handler_,
      OnJobCreation(JobWithPsmRlweRequest(WithOprfRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(-1, 0));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kConnectionError);
}

TEST_F(EnrollmentStateFetcherTest, ServerErrorOnOprfRequest) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  EXPECT_CALL(
      job_creation_handler_,
      OnJobCreation(JobWithPsmRlweRequest(WithOprfRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(
          0, DM_STATUS_HTTP_STATUS_ERROR));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kServerError);
}

TEST_F(EnrollmentStateFetcherTest, FailToCreateQueryRequest) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest(/*any=*/true);
  base::test::TestFuture<AutoEnrollmentState> future;
  auto fetcher = EnrollmentStateFetcher::Create(
      future.GetCallback(), &local_state_,
      base::BindRepeating(
          [](const private_membership::rlwe::RlwePlaintextId& plaintext_id) {
            return private_membership::rlwe::PrivateMembershipRlweClient::
                CreateForTesting(
                       private_membership::rlwe::RlweUseCase::CROS_DEVICE_STATE,
                       // Using fake ID, cipher key and seed will cause failure
                       // to create query request since it won't match the
                       // ecrypted ID in OPRF response from the psm_test_case_.
                       {plaintext_id}, "ec_cipher_key",
                       "seed4567890123456789012345678912")
                    .value();
          }),
      fake_dm_service_.get(), shared_url_loader_factory_, &system_clock_,
      &state_key_broker_, &device_settings_service_);

  fetcher->Start();
  AutoEnrollmentState state = future.Get();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, EmptyQueryResponse) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  EXPECT_CALL(job_creation_handler_, OnJobCreation(JobWithPsmRlweRequest(
                                         WithQueryRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobOKAsync(""));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kServerError);
}

TEST_F(EnrollmentStateFetcherTest, ConnectionErrorOnQueryRequest) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  EXPECT_CALL(job_creation_handler_, OnJobCreation(JobWithPsmRlweRequest(
                                         WithQueryRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(-1, 0));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kConnectionError);
}

TEST_F(EnrollmentStateFetcherTest, ServerErrorOnQueryRequest) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  EXPECT_CALL(job_creation_handler_, OnJobCreation(JobWithPsmRlweRequest(
                                         WithQueryRequestFor(&psm_test_case_))))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(
          0, DM_STATUS_HTTP_STATUS_ERROR));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kServerError);
}

TEST_F(EnrollmentStateFetcherTest, EmptyEnrollmentStateResponse) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(
          fake_dm_service_->SendJobOKAsync(em::DeviceManagementResponse()));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kServerError);
}

TEST_F(EnrollmentStateFetcherTest, ConnectionErrorOnEnrollmentStateRequest) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(-1, 0));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kConnectionError);
}

TEST_F(EnrollmentStateFetcherTest, ServerErrorOnEnrollmentStateRequest) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobResponseAsync(
          0, DM_STATUS_HTTP_STATUS_ERROR));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kServerError);
}

TEST_F(EnrollmentStateFetcherTest, NoEnrollment) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  response.mutable_device_state_retrieval_response();
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  EXPECT_TRUE(device_state.empty());
}

TEST_F(EnrollmentStateFetcherTest, InitialEnrollmentEnforced) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED);
  state_response->set_management_domain("example.org");
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateInitialModeEnrollmentEnforced);
  ASSERT_TRUE(device_state.FindString(kDeviceStateManagementDomain));
  EXPECT_EQ(*device_state.FindString(kDeviceStateManagementDomain),
            "example.org");
  EXPECT_FALSE(device_state.FindString(kDeviceStateDisabledMessage));
  EXPECT_FALSE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_FALSE(device_state.FindString(kDeviceStatePackagedLicense));
  EXPECT_FALSE(device_state.FindString(kDeviceStateAssignedUpgradeType));
}

TEST_F(EnrollmentStateFetcherTest, InitialEnrollmentDisabled) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_DISABLED);
  state_response->mutable_disabled_state()->set_message(kTestDisabledMessage);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kDisabled);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateModeDisabled);
  ASSERT_TRUE(device_state.FindString(kDeviceStateDisabledMessage));
  EXPECT_EQ(*device_state.FindString(kDeviceStateDisabledMessage),
            kTestDisabledMessage);
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithPackagedEnterpriseLicense) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_is_license_packaged_with_device(true);
  state_response->set_license_packaging_sku(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          CHROME_ENTERPRISE);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateInitialModeEnrollmentZeroTouch);
  ASSERT_TRUE(device_state.FindBool(kDeviceStatePackagedLicense));
  EXPECT_EQ(*device_state.FindBool(kDeviceStatePackagedLicense), true);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeEnterprise);
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithEducationLicense) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_is_license_packaged_with_device(false);
  state_response->set_license_packaging_sku(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          CHROME_EDUCATION);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindBool(kDeviceStatePackagedLicense));
  EXPECT_EQ(*device_state.FindBool(kDeviceStatePackagedLicense), false);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeEducation);
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithTerminalLicense) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_license_packaging_sku(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          CHROME_TERMINAL);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeTerminal);
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithUnspecifiedUpgrade) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_assigned_upgrade_type(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          ASSIGNED_UPGRADE_TYPE_UNSPECIFIED);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateAssignedUpgradeType));
  EXPECT_TRUE(
      device_state.FindString(kDeviceStateAssignedUpgradeType)->empty());
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithChromeEnterpriseUpgrade) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_assigned_upgrade_type(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          ASSIGNED_UPGRADE_TYPE_CHROME_ENTERPRISE);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateAssignedUpgradeType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateAssignedUpgradeType),
            kDeviceStateAssignedUpgradeTypeChromeEnterprise);
}

TEST_F(EnrollmentStateFetcherTest, ZTEWithKioskAndSignageUpgrade) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response()
                             ->mutable_initial_state_response();
  state_response->set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  state_response->set_assigned_upgrade_type(
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          ASSIGNED_UPGRADE_TYPE_KIOSK_AND_SIGNAGE);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateAssignedUpgradeType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateAssignedUpgradeType),
            kDeviceStateAssignedUpgradeTypeKiosk);
}

TEST_F(EnrollmentStateFetcherTest, ReEnrollmentRequested) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_REQUESTED);
  state_response->set_management_domain("example.org");
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateRestoreModeReEnrollmentRequested);
  ASSERT_TRUE(device_state.FindString(kDeviceStateManagementDomain));
  EXPECT_EQ(*device_state.FindString(kDeviceStateManagementDomain),
            "example.org");
  EXPECT_FALSE(device_state.FindString(kDeviceStateDisabledMessage));
  EXPECT_FALSE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_FALSE(device_state.FindString(kDeviceStatePackagedLicense));
  EXPECT_FALSE(device_state.FindString(kDeviceStateAssignedUpgradeType));
}

TEST_F(EnrollmentStateFetcherTest, ReEnrollmentEnforced) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateRestoreModeReEnrollmentEnforced);
}

TEST_F(EnrollmentStateFetcherTest, ReEnrollmentDisabled) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED);
  state_response->mutable_disabled_state()->set_message(kTestDisabledMessage);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kDisabled);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateModeDisabled);
  ASSERT_TRUE(device_state.FindString(kDeviceStateDisabledMessage));
  EXPECT_EQ(*device_state.FindString(kDeviceStateDisabledMessage),
            kTestDisabledMessage);
}

TEST_F(EnrollmentStateFetcherTest, AutoREWithPerpetualLicense) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH);
  state_response->mutable_license_type()->set_license_type(
      em::LicenseType::CDM_PERPETUAL);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateMode));
  EXPECT_EQ(*device_state.FindString(kDeviceStateMode),
            kDeviceStateRestoreModeReEnrollmentZeroTouch);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeEnterprise);
}

TEST_F(EnrollmentStateFetcherTest, AutoREWithUndefinedLicense) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH);
  state_response->mutable_license_type()->set_license_type(
      em::LicenseType::UNDEFINED);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_TRUE(device_state.FindString(kDeviceStateLicenseType)->empty());
}

TEST_F(EnrollmentStateFetcherTest, AutoREWithAnnualLicense) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH);
  state_response->mutable_license_type()->set_license_type(
      em::LicenseType::CDM_ANNUAL);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeEnterprise);
}

TEST_F(EnrollmentStateFetcherTest, AutoREWithKioskLicense) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH);
  state_response->mutable_license_type()->set_license_type(
      em::LicenseType::KIOSK);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeTerminal);
}

TEST_F(EnrollmentStateFetcherTest, AutoREWithPackagedLicense) {
  ExpectOwnershipCheck();
  ExpectStateKeysRequest();
  ExpectOprfRequest();
  ExpectQueryRequest();
  em::DeviceManagementResponse response;
  auto* state_response = response.mutable_device_state_retrieval_response();
  state_response->set_restore_mode(
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH);
  state_response->mutable_license_type()->set_license_type(
      em::LicenseType::CDM_PACKAGED);
  EXPECT_CALL(job_creation_handler_,
              OnJobCreation(JobWithStateRequest(
                  kTestStateKey, kTestSerialNumber, kTestBrandCode)))
      .WillOnce(fake_dm_service_->SendJobOKAsync(response));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kEnrollment);
  const base::Value::Dict& device_state =
      local_state_.GetDict(prefs::kServerBackedDeviceState);
  ASSERT_TRUE(device_state.FindString(kDeviceStateLicenseType));
  EXPECT_EQ(*device_state.FindString(kDeviceStateLicenseType),
            kDeviceStateLicenseTypeEnterprise);
}

}  // namespace policy
