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
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "cobalt/browser/h5vcc_metrics/public/mojom/h5vcc_metrics.mojom.h"
#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"
#include "cobalt/browser/metrics/cobalt_metrics_log_uploader.h"
#include "cobalt/shell/common/shell_paths.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
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
               const metrics::LogMetadata& log_metadata,
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
      std::unique_ptr<variations::SyntheticTrialRegistry>
          synthetic_trial_registry,
      PrefService* local_state)
      : CobaltMetricsServiceClient(state_manager,
                                   std::move(synthetic_trial_registry),
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

  void ResetOnApplicationNotIdleInternalCalled() {
    on_application_not_idle_internal_called_ = false;
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

  // Expose the timer for inspection in tests.
  const base::RepeatingTimer& idle_refresh_timer() const {
    return idle_refresh_timer_;
  }

  void ClearMockUploaderPtr() { mock_log_uploader_ = nullptr; }

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
        base::DIR_CACHE, temp_dir_.GetPath());

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

    auto synthetic_trial_registry =
        std::make_unique<variations::SyntheticTrialRegistry>();
    synthetic_trial_registry_ = synthetic_trial_registry.get();

    // Instantiate the test client and call Initialize to trigger mock creation.
    // This simulates the two-phase initialization of the static Create().
    client_ = std::make_unique<TestCobaltMetricsServiceClient>(
        metrics_state_manager_.get(), std::move(synthetic_trial_registry),
        &prefs_);
    client_->CallInitialize();  // This will use the overridden factory methods.

    ASSERT_THAT(client_->mock_metrics_service(), NotNull());
    ASSERT_THAT(client_->mock_log_uploader(), NotNull());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI};
  TestingPrefServiceSimple prefs_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  std::unique_ptr<CobaltEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<TestCobaltMetricsServiceClient> client_;
  base::raw_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;
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

TEST_F(CobaltMetricsServiceClientTest, RecordMemoryMetricsRecordsHistogram) {
  base::HistogramTester histogram_tester;

  // Prepare a dummy GlobalMemoryDump.
  memory_instrumentation::mojom::GlobalMemoryDumpPtr dump_ptr =
      memory_instrumentation::mojom::GlobalMemoryDump::New();

  // Browser process dump
  auto browser_dump = memory_instrumentation::mojom::ProcessMemoryDump::New();
  browser_dump->process_type =
      memory_instrumentation::mojom::ProcessType::BROWSER;
  browser_dump->os_dump = memory_instrumentation::mojom::OSMemDump::New();
  browser_dump->os_dump->private_footprint_kb = 10240;  // 10 MB
  browser_dump->os_dump->resident_set_kb = 20480;       // 20 MB
  dump_ptr->process_dumps.push_back(std::move(browser_dump));

  auto global_dump =
      memory_instrumentation::GlobalMemoryDump::MoveFrom(std::move(dump_ptr));

  CobaltMetricsServiceClient::RecordMemoryMetrics(global_dump.get());

  histogram_tester.ExpectUniqueSample("Memory.Total.PrivateMemoryFootprint", 10,
                                      1);
  histogram_tester.ExpectUniqueSample("Memory.Total.Resident", 20, 1);
  // TODO(482357006): Re-add process-specific resident memory metrics (Browser,
  // Renderer, GPU) when moving to multi-process architecture.
}

TEST_F(CobaltMetricsServiceClientTest,
       CollectFinalMetricsForLogInvokesDoneCallbackAndNotifiesService) {
  base::MockCallback<base::OnceClosure> done_callback_mock;

  EXPECT_CALL(done_callback_mock, Run());
  client_->CollectFinalMetricsForLog(done_callback_mock.Get());
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
  constexpr std::string_view kExpectedGuid = "test-guid-123";

  EXPECT_CALL(on_upload_complete_mock,
              Run(kExpectedStatusCode, kExpectedNetError, kExpectedHttps,
                  kExpectedForceDiscard, kExpectedGuid));

  // Run the callback that was captured from the actual call to
  // setOnUploadComplete.
  captured_callback.Run(kExpectedStatusCode, kExpectedNetError, kExpectedHttps,
                        kExpectedForceDiscard, kExpectedGuid);
  client_->ClearMockUploaderPtr();
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

TEST_F(CobaltMetricsServiceClientTest, IdleTimerStartsOnInitialization) {
  // Initialization is done in SetUp().
  EXPECT_TRUE(client_->idle_refresh_timer().IsRunning());

  base::TimeDelta expected_delay = kStandardUploadIntervalMinutes / 2;
  if (expected_delay < kMinIdleRefreshInterval) {
    expected_delay = kMinIdleRefreshInterval;
  }
  EXPECT_EQ(expected_delay, client_->idle_refresh_timer().GetCurrentDelay());
}

TEST_F(CobaltMetricsServiceClientTest,
       IdleTimerRestartsOnSetUploadIntervalWithCorrectDelay) {
  const base::TimeDelta new_upload_interval = base::Minutes(50);
  client_->SetUploadInterval(new_upload_interval);

  EXPECT_TRUE(client_->idle_refresh_timer().IsRunning());
  base::TimeDelta expected_delay = new_upload_interval / 2;  // 25 minutes
  ASSERT_GT(expected_delay, kMinIdleRefreshInterval);
  EXPECT_EQ(expected_delay, client_->idle_refresh_timer().GetCurrentDelay());
}

TEST_F(CobaltMetricsServiceClientTest,
       IdleTimerDelayIsLimitedByMinIdleRefreshInterval) {
  // New interval is 40 seconds. Half of it is 20 seconds.
  const base::TimeDelta new_upload_interval = base::Seconds(40);
  client_->SetUploadInterval(new_upload_interval);

  EXPECT_TRUE(client_->idle_refresh_timer().IsRunning());
  // Expected delay should be kMinIdleRefreshInterval (30s) because 20s < 30s.
  EXPECT_EQ(kMinIdleRefreshInterval,
            client_->idle_refresh_timer().GetCurrentDelay());
}

TEST_F(CobaltMetricsServiceClientTest,
       IdleTimerCallbackInvokesOnApplicationNotIdleInternal) {
  client_->ResetOnApplicationNotIdleInternalCalled();  // Ensure clean state.

  // Use a known interval that results in a delay > kMinIdleRefreshInterval.
  const base::TimeDelta test_upload_interval = base::Minutes(2);  // 120s
  client_->SetUploadInterval(test_upload_interval);
  base::TimeDelta current_delay =
      client_->idle_refresh_timer().GetCurrentDelay();
  EXPECT_EQ(base::Minutes(1), current_delay);  // Half of 2 minutes.

  EXPECT_FALSE(client_->GetOnApplicationNotIdleInternalCalled());

  task_environment_.FastForwardBy(current_delay);
  EXPECT_TRUE(client_->GetOnApplicationNotIdleInternalCalled());
}

TEST_F(CobaltMetricsServiceClientTest, IdleTimerIsRepeatingAndKeepsFiring) {
  client_->ResetOnApplicationNotIdleInternalCalled();

  const base::TimeDelta test_upload_interval =
      base::Seconds(80);  // Results in 40s delay.
  client_->SetUploadInterval(test_upload_interval);
  base::TimeDelta current_delay =
      client_->idle_refresh_timer().GetCurrentDelay();
  EXPECT_EQ(base::Seconds(40), current_delay);  // 80s / 2 = 40s > 30s.

  EXPECT_FALSE(client_->GetOnApplicationNotIdleInternalCalled());

  // First fire.
  task_environment_.FastForwardBy(current_delay);
  EXPECT_TRUE(client_->GetOnApplicationNotIdleInternalCalled());

  client_->ResetOnApplicationNotIdleInternalCalled();  // Reset for next fire.
  EXPECT_FALSE(client_->GetOnApplicationNotIdleInternalCalled());

  // Second fire.
  task_environment_.FastForwardBy(current_delay);
  EXPECT_TRUE(client_->GetOnApplicationNotIdleInternalCalled());
}

TEST_F(CobaltMetricsServiceClientTest,
       StartIdleRefreshTimerStopsAndRestartsExistingTimer) {
  // Initial timer started with default interval (15 min delay) in SetUp.
  base::TimeDelta initial_delay =
      client_->idle_refresh_timer().GetCurrentDelay();
  EXPECT_EQ(kStandardUploadIntervalMinutes / 2, initial_delay);

  // Change interval to something different.
  const base::TimeDelta new_upload_interval =
      base::Minutes(10);  // New delay = 5 mins.
  client_->SetUploadInterval(new_upload_interval);

  EXPECT_TRUE(client_->idle_refresh_timer().IsRunning());
  base::TimeDelta new_delay = new_upload_interval / 2;
  ASSERT_GT(new_delay, kMinIdleRefreshInterval);
  EXPECT_EQ(new_delay, client_->idle_refresh_timer().GetCurrentDelay());

  client_->ResetOnApplicationNotIdleInternalCalled();
  EXPECT_FALSE(client_->GetOnApplicationNotIdleInternalCalled());

  // Fast forward by less than the initial_delay but more than or equal to
  // new_delay. If the timer wasn't restarted, it wouldn't fire. If it was
  // restarted, it should fire based on new_delay.
  task_environment_.FastForwardBy(new_delay);
  EXPECT_TRUE(client_->GetOnApplicationNotIdleInternalCalled());
}

}  // namespace cobalt
