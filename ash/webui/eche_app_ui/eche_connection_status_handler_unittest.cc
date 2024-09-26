// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::eche_app {

namespace {

class FakeObserver : public EcheConnectionStatusHandler::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_connection_status_changed_calls() const {
    return num_connection_status_changed_calls_;
  }

  size_t num_connection_status_for_ui_changed_calls() const {
    return num_connection_status_for_ui_changed_calls_;
  }

  size_t num_request_background_connection_attempt_calls() const {
    return num_request_background_connection_attempt_calls_;
  }

  size_t num_request_close_connection_calls() const {
    return num_request_close_connection_calls_;
  }

  mojom::ConnectionStatus last_connection_changed_status() const {
    return last_connection_changed_status_;
  }

  mojom::ConnectionStatus last_connection_for_ui_changed_status() const {
    return last_connection_for_ui_changed_status_;
  }

  // EcheConnectionStatusHandler::Observer:
  void OnConnectionStatusChanged(
      mojom::ConnectionStatus connection_status) override {
    ++num_connection_status_changed_calls_;
    last_connection_changed_status_ = connection_status;
  }

  void OnConnectionStatusForUiChanged(
      mojom::ConnectionStatus connection_status) override {
    ++num_connection_status_for_ui_changed_calls_;
    last_connection_for_ui_changed_status_ = connection_status;
  }

  void OnRequestBackgroundConnectionAttempt() override {
    ++num_request_background_connection_attempt_calls_;
  }

  void OnRequestCloseConnnection() override {
    ++num_request_close_connection_calls_;
  }

 private:
  size_t num_connection_status_changed_calls_ = 0;
  size_t num_connection_status_for_ui_changed_calls_ = 0;
  size_t num_request_background_connection_attempt_calls_ = 0;
  size_t num_request_close_connection_calls_ = 0;
  mojom::ConnectionStatus last_connection_changed_status_ =
      mojom::ConnectionStatus::kConnectionStatusDisconnected;
  mojom::ConnectionStatus last_connection_for_ui_changed_status_ =
      mojom::ConnectionStatus::kConnectionStatusDisconnected;
};

}  // namespace

class EcheConnectionStatusHandlerTest : public testing::Test {
 public:
  EcheConnectionStatusHandlerTest() = default;
  EcheConnectionStatusHandlerTest(const EcheConnectionStatusHandlerTest&) =
      delete;
  EcheConnectionStatusHandlerTest& operator=(
      const EcheConnectionStatusHandlerTest&) = delete;
  ~EcheConnectionStatusHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA,
                              features::kEcheNetworkConnectionState},
        /*disabled_features=*/{});

    handler_ = std::make_unique<EcheConnectionStatusHandler>();
    handler_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    handler_->RemoveObserver(&fake_observer_);
    handler_.reset();
  }

  EcheConnectionStatusHandler& handler() { return *handler_; }

  void NotifyConnectionStatusChanged(
      mojom::ConnectionStatus connection_status) {
    handler_->OnConnectionStatusChanged(connection_status);
  }

  void SetFeatureStatus(FeatureStatus feature_status) {
    handler_->set_feature_status_for_test(feature_status);
  }

  size_t GetNumConnectionStatusChangedCalls() const {
    return fake_observer_.num_connection_status_changed_calls();
  }

  size_t GetNumConnectionStatusForUiChangedCalls() const {
    return fake_observer_.num_connection_status_for_ui_changed_calls();
  }

  size_t GetNumRequestBackgroundConnectionAttemptCalls() const {
    return fake_observer_.num_request_background_connection_attempt_calls();
  }

  size_t GetNumRequestCloceConnectionCalls() const {
    return fake_observer_.num_request_close_connection_calls();
  }

  mojom::ConnectionStatus GetLastConnectionChangedStatus() const {
    return fake_observer_.last_connection_changed_status();
  }

  mojom::ConnectionStatus GetLastConnectionForUiChangedStatus() const {
    return fake_observer_.last_connection_for_ui_changed_status();
  }

  mojom::ConnectionStatus GetConnectionStatusForUi() const {
    return handler_->get_connection_status_for_ui_for_test();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  FakeObserver fake_observer_;
  std::unique_ptr<EcheConnectionStatusHandler> handler_;
};

TEST_F(EcheConnectionStatusHandlerTest, OnConnectionStatusChanged) {
  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnecting);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 1u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 2u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusFailed);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusFailed);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 3u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 4u);
}

TEST_F(EcheConnectionStatusHandlerTest, OnConnectionStatusChangedFlagDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA},
      /*disabled_features=*/{features::kEcheNetworkConnectionState});

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnecting);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusFailed);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);
}

TEST_F(EcheConnectionStatusHandlerTest, CheckConnectionStatusForUi) {
  SetFeatureStatus(FeatureStatus::kDisconnected);

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnecting);

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 1u);

  handler().CheckConnectionStatusForUi();

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 1u);

  SetFeatureStatus(FeatureStatus::kConnected);
  handler().CheckConnectionStatusForUi();

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 2u);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 2u);
}

TEST_F(EcheConnectionStatusHandlerTest,
       CheckConnectionStatusForUi_TimeSinceLastCheckIncreases) {
  SetFeatureStatus(FeatureStatus::kDisconnected);
  handler().CheckConnectionStatusForUi();

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);
  EXPECT_EQ(GetNumRequestBackgroundConnectionAttemptCalls(), 0u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 1u);

  // After more than 10 seconds pass, extra calls should happen.
  SetFeatureStatus(FeatureStatus::kConnected);
  task_environment_.FastForwardBy(base::Seconds(11));
  handler().CheckConnectionStatusForUi();

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 2u);
  EXPECT_EQ(GetNumRequestBackgroundConnectionAttemptCalls(), 1u);

  // Reset to Disconnected
  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 3u);
  EXPECT_EQ(GetNumRequestBackgroundConnectionAttemptCalls(), 1u);

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);

  // After more than 10 minutes pass, state should go back to Connecting.
  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 4u);

  task_environment_.FastForwardBy(base::Minutes(11));
  handler().CheckConnectionStatusForUi();

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 5u);
  EXPECT_EQ(GetNumRequestBackgroundConnectionAttemptCalls(), 2u);
}

TEST_F(EcheConnectionStatusHandlerTest, SetConnectionStatusForUi) {
  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);

  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 1u);

  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 2u);

  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusFailed);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusFailed);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 3u);

  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 4u);
}

TEST_F(EcheConnectionStatusHandlerTest, OnFeatureStatusChanged) {
  handler().OnFeatureStatusChanged(FeatureStatus::kDisconnected);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);

  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusConnected);

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 1u);

  handler().OnFeatureStatusChanged(FeatureStatus::kConnected);

  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 2u);
}

}  // namespace ash::eche_app
