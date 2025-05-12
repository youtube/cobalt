// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "cobalt/browser/h5vcc_metrics/public/mojom/h5vcc_metrics.mojom.h"
#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"
#include "cobalt/browser/metrics/cobalt_metrics_logs_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/shell/browser/shell_paths.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

using ::testing::_;
using ::testing::ByMove;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

// Mock for MetricsService to verify construction of a specific MetricsService
// in tests.
class MockMetricsService : public metrics::MetricsService {
 public:
  MockMetricsService(metrics::MetricsStateManager* state_manager,
                     metrics::MetricsServiceClient* client,
                     PrefService* local_state)
      : metrics::MetricsService(state_manager, client, local_state) {}
};

class MockCobaltMetricsLogUploader : public CobaltMetricsLogUploader {
 public:
  MockCobaltMetricsLogUploader()
      : CobaltMetricsLogUploader(
            ::metrics::MetricsLogUploader::MetricServiceType::UMA) {}
  MOCK_METHOD(void,
              UploadLog,
              (const std::string& compressed_log_data,
               const std::string& log_hash,
               const std::string& log_signature,
               const metrics::ReportingInfo& reporting_info),
              (override));
  MOCK_METHOD(
      void,
      SetMetricsListener,
      (mojo::PendingRemote<h5vcc_metrics::mojom::MetricsListener> listener),
      (override));

  // Provide a way to call the real GetWeakPtr for the mock framework.
  base::WeakPtr<CobaltMetricsLogUploader> RealGetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  // Mock GetWeakPtr itself if specific test scenarios require controlling it.
  MOCK_METHOD(base::WeakPtr<CobaltMetricsLogUploader>, GetWeakPtr, ());

  MOCK_METHOD(void,
              setOnUploadComplete,
              (const ::metrics::MetricsLogUploader::UploadCallback&),
              (override));

 private:
  base::WeakPtrFactory<MockCobaltMetricsLogUploader> weak_ptr_factory_{this};
};

class MockH5vccMetricsListener : public h5vcc_metrics::mojom::MetricsListener {
 public:
  MOCK_METHOD(void,
              OnMetrics,
              (h5vcc_metrics::mojom::H5vccMetricType metric_type,
               const std::string& serialized_proto),
              (override));
};

// Test version of CobaltMetricsServiceClient to allow injecting mock
// dependencies. This requires CobaltMetricsServiceClient to have virtual
// methods for creating its owned dependencies (MetricsService,
// CobaltMetricsLogUploader).
class TestCobaltMetricsServiceClient : public CobaltMetricsServiceClient {
 public:
  TestCobaltMetricsServiceClient(
      metrics::MetricsStateManager* state_manager,
      variations::SyntheticTrialRegistry* synthetic_trial_registry,
      PrefService* local_state)
      : CobaltMetricsServiceClient(state_manager,
                                   synthetic_trial_registry,
                                   local_state) {}

  // Override internal factory for MetricsService
  std::unique_ptr<metrics::MetricsService> CreateMetricsServiceInternal(
      metrics::MetricsStateManager* state_manager,
      metrics::MetricsServiceClient* client,
      PrefService* local_state) override {
    auto mock_service = std::make_unique<StrictMock<MockMetricsService>>(
        state_manager, client, local_state);
    mock_metrics_service_ = mock_service.get();
    return std::move(mock_service);
  }

  // Override internal factory for CobaltMetricsLogUploader.
  std::unique_ptr<CobaltMetricsLogUploader> CreateLogUploaderInternal()
      override {
    auto mock_uploader =
        std::make_unique<StrictMock<MockCobaltMetricsLogUploader>>();
    // Setup the mock's GetWeakPtr to return a usable weak pointer.
    ON_CALL(*mock_uploader, GetWeakPtr())
        .WillByDefault([uploader_ptr = mock_uploader.get()]() {
          return static_cast<MockCobaltMetricsLogUploader*>(uploader_ptr)
              ->RealGetWeakPtr();
        });
    mock_log_uploader_ = mock_uploader.get();
    return std::move(mock_uploader);
  }

  void OnApplicationNotIdleInternal() override {
    on_application_not_idle_internal_called_ = true;
  }

  bool GetOnApplicationNotIdleInternalCalled() {
    return on_application_not_idle_internal_called_;
  }

  // Expose Initialize for finer-grained control in tests if necessary,
  // though the base class's static Create method normally handles this.
  void CallInitialize() { Initialize(); }

  StrictMock<MockMetricsService>* mock_metrics_service() const {
    return mock_metrics_service_;
  }
  StrictMock<MockCobaltMetricsLogUploader>* mock_log_uploader() const {
    return mock_log_uploader_;
  }

 private:
  raw_ptr<StrictMock<MockMetricsService>> mock_metrics_service_ = nullptr;
  raw_ptr<StrictMock<MockCobaltMetricsLogUploader>> mock_log_uploader_ =
      nullptr;
  bool on_application_not_idle_internal_called_ = false;
};

class CobaltMetricsServiceClientTest : public ::testing::Test {
 protected:
  CobaltMetricsServiceClientTest() { mojo::core::Init(); }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        content::SHELL_DIR_USER_DATA, temp_dir_.GetPath());

    // Register all metric-related prefs, otherwise tests crash when calling
    // upstream metrics code.
    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    prefs_.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, false);

    // Make sure user_metrics.cc has a g_task_runner.
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    // The client under test receives MetricsStateManager, so create one.
    // A real CobaltEnabledStateProvider is simple enough.
    enabled_state_provider_ =
        std::make_unique<CobaltEnabledStateProvider>(&prefs_);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &prefs_, enabled_state_provider_.get(), std::wstring(),
        temp_dir_.GetPath(), metrics::StartupVisibility::kForeground);
    ASSERT_THAT(metrics_state_manager_, NotNull());

    synthetic_trial_registry_ =
        std::make_unique<variations::SyntheticTrialRegistry>();

    // Instantiate the test client and call Initialize to trigger mock creation.
    // This simulates the two-phase initialization of the static Create().
    client_ = std::make_unique<TestCobaltMetricsServiceClient>(
        metrics_state_manager_.get(), synthetic_trial_registry_.get(), &prefs_);
    client_->CallInitialize();  // This will use the overridden factory methods.

    ASSERT_THAT(client_->mock_metrics_service(), NotNull());
    ASSERT_THAT(client_->mock_log_uploader(), NotNull());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  TestingPrefServiceSimple prefs_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  std::unique_ptr<CobaltEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;
  std::unique_ptr<TestCobaltMetricsServiceClient> client_;
};

TEST_F(CobaltMetricsServiceClientTest, PostCreateInitialization) {
  EXPECT_EQ(client_->GetMetricsService(), client_->mock_metrics_service());
}

TEST_F(CobaltMetricsServiceClientTest,
       GetSyntheticTrialRegistryReturnsInjectedInstance) {
  EXPECT_EQ(client_->GetSyntheticTrialRegistry(),
            synthetic_trial_registry_.get());
}

TEST_F(CobaltMetricsServiceClientTest,
       GetMetricsServiceReturnsCreatedInstance) {
  EXPECT_EQ(client_->GetMetricsService(), client_->mock_metrics_service());
  EXPECT_THAT(client_->GetMetricsService(), NotNull());
}

TEST_F(CobaltMetricsServiceClientTest, SetMetricsClientIdIsNoOp) {
  client_->SetMetricsClientId("test_client_id");
  // No specific side-effect to check other than not crashing.
  SUCCEED();
}

TEST_F(CobaltMetricsServiceClientTest, GetProductReturnsDefault) {
  EXPECT_EQ(0, client_->GetProduct());
}

TEST_F(CobaltMetricsServiceClientTest, GetApplicationLocaleReturnsDefault) {
  EXPECT_EQ("en-US", client_->GetApplicationLocale());
}

TEST_F(CobaltMetricsServiceClientTest,
       GetNetworkTimeTrackerNotImplementedReturnsNull) {
  // NOTIMPLEMENTED() for now.
  EXPECT_THAT(client_->GetNetworkTimeTracker(), IsNull());
}

TEST_F(CobaltMetricsServiceClientTest, GetBrandReturnsFalse) {
  std::string brand_code;
  EXPECT_FALSE(client_->GetBrand(&brand_code));
  EXPECT_TRUE(brand_code.empty());
}

TEST_F(CobaltMetricsServiceClientTest, GetChannelReturnsUnknown) {
  EXPECT_EQ(metrics::SystemProfileProto::CHANNEL_UNKNOWN,
            client_->GetChannel());
}

TEST_F(CobaltMetricsServiceClientTest, IsExtendedStableChannelReturnsFalse) {
  EXPECT_FALSE(client_->IsExtendedStableChannel());
}

TEST_F(CobaltMetricsServiceClientTest, GetVersionStringReturnsNonEmpty) {
  // base::Version().GetString() should provide a default like "0.0.0.0".
  EXPECT_EQ(base::Version().GetString(), client_->GetVersionString());
  EXPECT_FALSE(client_->GetVersionString().empty());
}

TEST_F(CobaltMetricsServiceClientTest,
       CollectFinalMetricsForLogInvokesDoneCallbackAndNotifiesService) {
  base::MockCallback<base::OnceClosure> done_callback_mock;

  EXPECT_CALL(done_callback_mock, Run());
  EXPECT_FALSE(client_->GetOnApplicationNotIdleInternalCalled());
  client_->CollectFinalMetricsForLog(done_callback_mock.Get());
  // Most ideally, we could assert that the "real" OnApplicationIdle was
  // called. However, this is impossible without modifying MetricsService itself
  // which we really don't want to do. There is no real virtual method in
  // MetricsService that seems to indicate OnApplicationNotIdle was called.
  // The second best we can do within these constraints is to assert our
  // internal method is called, which should call OnApplicationNotIdle() in the
  // real impl.
  EXPECT_TRUE(client_->GetOnApplicationNotIdleInternalCalled());
}

TEST_F(CobaltMetricsServiceClientTest, GetMetricsServerUrlReturnsPlaceholder) {
  EXPECT_EQ(GURL("https://youtube.com/tv/uma"), client_->GetMetricsServerUrl());
}

TEST_F(CobaltMetricsServiceClientTest,
       CreateUploaderReturnsMockUploaderAndSetsCallback) {
  base::MockCallback<metrics::MetricsLogUploader::UploadCallback>
      on_upload_complete_mock;

  // Variable to capture the callback passed to setOnUploadComplete().
  metrics::MetricsLogUploader::UploadCallback captured_callback;

  // Expect setOnUploadComplete to be called with any callback of the correct
  // type, and use .WillOnce() to save the actual argument passed into
  // 'captured_callback'.
  EXPECT_CALL(
      *client_->mock_log_uploader(),
      setOnUploadComplete(
          testing::A<const metrics::MetricsLogUploader::UploadCallback&>()))
      .WillOnce(testing::SaveArg<0>(&captured_callback));

  // The mock_log_uploader_ is already created and its GetWeakPtr is stubbed.
  // CreateUploader will move the uploader from the client's internal
  // unique_ptr. The returned uploader should be the one our factory provided.
  std::unique_ptr<metrics::MetricsLogUploader> uploader =
      client_->CreateUploader(GURL(), GURL(), "",
                              metrics::MetricsLogUploader::UMA,
                              on_upload_complete_mock.Get());

  // The uploader returned should be the same mock instance.
  EXPECT_EQ(uploader.get(), client_->mock_log_uploader());
  EXPECT_THAT(uploader, NotNull());

  // Verify that a callback was indeed captured by SaveArg.
  ASSERT_TRUE(captured_callback) << "Callback was not captured by SaveArg. "
                                    "Was setOnUploadComplete called?";

  // Invoke the 'captured_callback' and expect on_upload_complete_mock' (the
  // original one you created) to have its Run method called. This proves that
  // the callback set by CreateUploader is functionally the mock callback.
  const int kExpectedStatusCode = 204;
  const int kExpectedNetError = 0;
  const bool kExpectedHttps = true;
  const bool kExpectedForceDiscard = false;
  constexpr base::StringPiece kExpectedGuid = "test-guid-123";

  EXPECT_CALL(on_upload_complete_mock,
              Run(kExpectedStatusCode, kExpectedNetError, kExpectedHttps,
                  kExpectedForceDiscard, kExpectedGuid));

  // Run the callback that was captured from the actual call to
  // setOnUploadComplete.
  captured_callback.Run(kExpectedStatusCode, kExpectedNetError, kExpectedHttps,
                        kExpectedForceDiscard, kExpectedGuid);
}

TEST_F(CobaltMetricsServiceClientTest,
       GetStandardUploadIntervalReturnsDefaultOrSetValue) {
  EXPECT_EQ(kStandardUploadIntervalMinutes,
            client_->GetStandardUploadInterval());

  const base::TimeDelta new_interval = base::Minutes(23);
  client_->SetUploadInterval(new_interval);
  EXPECT_EQ(new_interval, client_->GetStandardUploadInterval());
}

TEST_F(CobaltMetricsServiceClientTest,
       SetMetricsListenerDelegatesToLogUploader) {
  mojo::PendingRemote<h5vcc_metrics::mojom::MetricsListener> listener_remote;

  EXPECT_CALL(*(client_->mock_log_uploader()), SetMetricsListener(_));

  client_->SetMetricsListener(std::move(listener_remote));
}

}  // namespace cobalt
