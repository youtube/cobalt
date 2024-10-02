// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connections_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash::quick_start {

namespace {

using Observer = TargetDeviceBootstrapController::Observer;
using Status = TargetDeviceBootstrapController::Status;
using Step = TargetDeviceBootstrapController::Step;
using ErrorCode = TargetDeviceBootstrapController::ErrorCode;
using ConnectionClosedReason =
    TargetDeviceConnectionBroker::ConnectionClosedReason;

class FakeObserver : public Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  void OnStatusChanged(const Status& status) override {
    // Step must change.
    ASSERT_NE(status.step, last_status.step);

    last_status = status;
  }

  Status last_status;
};

}  // namespace

class TargetDeviceBootstrapControllerTest : public testing::Test {
 public:
  static constexpr char kSourceDeviceId[] = "fake-source-device-id";

  TargetDeviceBootstrapControllerTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  TargetDeviceBootstrapControllerTest(TargetDeviceBootstrapControllerTest&) =
      delete;
  TargetDeviceBootstrapControllerTest& operator=(
      TargetDeviceBootstrapControllerTest&) = delete;
  ~TargetDeviceBootstrapControllerTest() override = default;

  void SetUp() override { CreateBootstrapController(); }
  void TearDown() override {
    bootstrap_controller_->RemoveObserver(fake_observer_.get());
    TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(nullptr);
  }

  void CreateBootstrapController() {
    TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
        &connection_broker_factory_);

    bootstrap_controller_ = std::make_unique<TargetDeviceBootstrapController>(
        fake_nearby_connections_manager_.GetWeakPtr());
    fake_observer_ = std::make_unique<FakeObserver>();
    bootstrap_controller_->AddObserver(fake_observer_.get());
  }

  FakeTargetDeviceConnectionBroker* connection_broker() {
    EXPECT_EQ(1u, connection_broker_factory_.instances().size());
    return connection_broker_factory_.instances().back();
  }

  PrefService* GetLocalState() { return local_state_.Get(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeTargetDeviceConnectionBroker::Factory connection_broker_factory_;
  FakeNearbyConnectionsManager fake_nearby_connections_manager_;
  std::unique_ptr<FakeObserver> fake_observer_;
  std::unique_ptr<TargetDeviceBootstrapController> bootstrap_controller_;
  ScopedTestingLocalState local_state_;
};

TEST_F(TargetDeviceBootstrapControllerTest, StartAdvertising) {
  bootstrap_controller_->StartAdvertising();
  EXPECT_EQ(1u, connection_broker()->num_start_advertising_calls());
  EXPECT_EQ(bootstrap_controller_.get(),
            connection_broker()->connection_lifecycle_listener());

  connection_broker()->on_start_advertising_callback().Run(/*success=*/true);
  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING);
}

TEST_F(TargetDeviceBootstrapControllerTest, StartAdvertisingFail) {
  bootstrap_controller_->StartAdvertising();
  connection_broker()->on_start_advertising_callback().Run(/*success=*/false);
  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::START_ADVERTISING_FAILED);
}

TEST_F(TargetDeviceBootstrapControllerTest, StopAdvertising) {
  bootstrap_controller_->StartAdvertising();
  connection_broker()->on_start_advertising_callback().Run(/*success=*/true);
  ASSERT_EQ(fake_observer_->last_status.step, Step::ADVERTISING);

  bootstrap_controller_->StopAdvertising();
  EXPECT_EQ(1u, connection_broker()->num_stop_advertising_calls());

  // Status changes only after the `on_stop_advertising_callback` run.
  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING);

  connection_broker()->on_stop_advertising_callback().Run();
  EXPECT_EQ(fake_observer_->last_status.step, Step::NONE);
}

TEST_F(TargetDeviceBootstrapControllerTest, InitiateConnection_QRCode) {
  bootstrap_controller_->StartAdvertising();
  connection_broker()->on_start_advertising_callback().Run(/*success=*/true);
  ASSERT_EQ(fake_observer_->last_status.step, Step::ADVERTISING);

  connection_broker()->InitiateConnection(kSourceDeviceId);

  EXPECT_EQ(fake_observer_->last_status.step, Step::QR_CODE_VERIFICATION);
  using QRCodePixelData = TargetDeviceBootstrapController::QRCodePixelData;
  EXPECT_TRUE(absl::holds_alternative<QRCodePixelData>(
      fake_observer_->last_status.payload));
}

TEST_F(TargetDeviceBootstrapControllerTest, InitiateConnection_Pin) {
  connection_broker()->set_use_pin_authentication(true);
  bootstrap_controller_->StartAdvertising();
  connection_broker()->on_start_advertising_callback().Run(/*success=*/true);
  ASSERT_EQ(fake_observer_->last_status.step, Step::ADVERTISING);

  connection_broker()->InitiateConnection(kSourceDeviceId);

  EXPECT_EQ(fake_observer_->last_status.step, Step::PIN_VERIFICATION);
  // TODO: Test PIN payload
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));
}

TEST_F(TargetDeviceBootstrapControllerTest, AuthenticateConnection) {
  bootstrap_controller_->StartAdvertising();
  connection_broker()->on_start_advertising_callback().Run(/*success=*/true);
  connection_broker()->InitiateConnection(kSourceDeviceId);
  connection_broker()->AuthenticateConnection(kSourceDeviceId);

  EXPECT_EQ(fake_observer_->last_status.step, Step::CONNECTED);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));
}

TEST_F(TargetDeviceBootstrapControllerTest, FeatureSupportStatus) {
  absl::optional<TargetDeviceConnectionBroker::FeatureSupportStatus>
      feature_status;

  connection_broker()->set_feature_support_status(
      FakeTargetDeviceConnectionBroker::FeatureSupportStatus::kUndetermined);

  bootstrap_controller_->GetFeatureSupportStatusAsync(
      base::BindLambdaForTesting(
          [&](TargetDeviceConnectionBroker::FeatureSupportStatus status) {
            feature_status = status;
          }));
  EXPECT_FALSE(feature_status.has_value());

  connection_broker()->set_feature_support_status(
      FakeTargetDeviceConnectionBroker::FeatureSupportStatus::kNotSupported);
  ASSERT_TRUE(feature_status.has_value());
  EXPECT_EQ(
      feature_status.value(),
      FakeTargetDeviceConnectionBroker::FeatureSupportStatus::kNotSupported);
}

TEST_F(TargetDeviceBootstrapControllerTest, RejectConnection) {
  bootstrap_controller_->StartAdvertising();
  connection_broker()->on_start_advertising_callback().Run(/*success=*/true);
  connection_broker()->InitiateConnection(kSourceDeviceId);

  connection_broker()->RejectConnection();

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::CONNECTION_REJECTED);
}

TEST_F(TargetDeviceBootstrapControllerTest, CloseConnection) {
  bootstrap_controller_->StartAdvertising();
  connection_broker()->on_start_advertising_callback().Run(/*success=*/true);
  connection_broker()->InitiateConnection(kSourceDeviceId);

  connection_broker()->CloseConnection(ConnectionClosedReason::kConnectionLost);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::CONNECTION_CLOSED);
}

TEST_F(TargetDeviceBootstrapControllerTest, GetPhoneInstanceId) {
  // TODO(b/234655072): Build out this unittest once phone instance ID is
  // retrieved from Gaia credentials exchange.
  ASSERT_TRUE(bootstrap_controller_->GetPhoneInstanceId().empty());
}

TEST_F(TargetDeviceBootstrapControllerTest, PrepareForUpdate_NotConnected) {
  ASSERT_FALSE(
      GetLocalState()->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  ASSERT_TRUE(GetLocalState()
                  ->GetDict(prefs::kResumeQuickStartAfterRebootInfo)
                  .empty());

  // PrepareForUpdate() shouldn't do anything if the connection is not
  // established.
  bootstrap_controller_->StartAdvertising();
  connection_broker()->on_start_advertising_callback().Run(/*success=*/true);
  ASSERT_NE(fake_observer_->last_status.step, Step::CONNECTED);

  bootstrap_controller_->PrepareForUpdate();
  EXPECT_FALSE(
      GetLocalState()->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  EXPECT_TRUE(GetLocalState()
                  ->GetDict(prefs::kResumeQuickStartAfterRebootInfo)
                  .empty());
}

TEST_F(TargetDeviceBootstrapControllerTest, PrepareForUpdate) {
  ASSERT_FALSE(
      GetLocalState()->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  ASSERT_TRUE(GetLocalState()
                  ->GetDict(prefs::kResumeQuickStartAfterRebootInfo)
                  .empty());

  bootstrap_controller_->StartAdvertising();
  connection_broker()->on_start_advertising_callback().Run(/*success=*/true);
  connection_broker()->InitiateConnection(kSourceDeviceId);
  connection_broker()->AuthenticateConnection(kSourceDeviceId);
  ASSERT_EQ(fake_observer_->last_status.step, Step::CONNECTED);

  bootstrap_controller_->PrepareForUpdate();
  // Pref shouldn't change until the connection is closed.
  EXPECT_FALSE(
      GetLocalState()->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  EXPECT_TRUE(GetLocalState()
                  ->GetDict(prefs::kResumeQuickStartAfterRebootInfo)
                  .empty());
  connection_broker()->CloseConnection(ConnectionClosedReason::kConnectionLost);
  EXPECT_TRUE(
      GetLocalState()->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  EXPECT_FALSE(GetLocalState()
                   ->GetDict(prefs::kResumeQuickStartAfterRebootInfo)
                   .empty());
  GetLocalState()->ClearPref(prefs::kShouldResumeQuickStartAfterReboot);
  GetLocalState()->ClearPref(prefs::kResumeQuickStartAfterRebootInfo);
}

}  // namespace ash::quick_start
