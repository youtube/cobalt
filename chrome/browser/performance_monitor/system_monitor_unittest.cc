// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/system_monitor.h"

#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {

namespace {

using SamplingFrequency = SystemMonitor::SamplingFrequency;
using SystemObserver = SystemMonitor::SystemObserver;
using MetricsRefreshFrequencies = SystemObserver::MetricRefreshFrequencies;

const int kFakeFreePhysMemoryMb = 42;

class MockMetricsMonitorObserver : public SystemObserver {
 public:
  ~MockMetricsMonitorObserver() override {}
  MOCK_METHOD1(OnFreePhysicalMemoryMbSample, void(int free_phys_memory_mb));
  MOCK_METHOD1(OnSystemMetricsStruct,
               void(const base::SystemMetrics& system_metrics));
};

// Test version of a MetricEvaluatorsHelper that returns constant values.
class TestMetricEvaluatorsHelper : public MetricEvaluatorsHelper {
 public:
  TestMetricEvaluatorsHelper() = default;

  TestMetricEvaluatorsHelper(const TestMetricEvaluatorsHelper&) = delete;
  TestMetricEvaluatorsHelper& operator=(const TestMetricEvaluatorsHelper&) =
      delete;

  ~TestMetricEvaluatorsHelper() override = default;

  absl::optional<int> GetFreePhysicalMemoryMb() override {
    return kFakeFreePhysMemoryMb;
  }
};

}  // namespace

class SystemMonitorTest : public testing::Test {
 protected:
  using MetricEvaluator = SystemMonitor::MetricEvaluator;
  using MetricMetadata = SystemMonitor::MetricMetadata;

  SystemMonitorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  SystemMonitorTest(const SystemMonitorTest&) = delete;
  SystemMonitorTest& operator=(const SystemMonitorTest&) = delete;

  void SetUp() override {
    EXPECT_EQ(nullptr, SystemMonitor::Get());
    system_monitor_ = base::WrapUnique(
        new SystemMonitor(std::make_unique<TestMetricEvaluatorsHelper>()));
  }

  void TearDown() override { system_monitor_.reset(); }

  void EnsureMetricsAreObservedAtExpectedFrequency(
      SamplingFrequency expected_free_memory_mb_freq =
          SamplingFrequency::kNoSampling,
      SamplingFrequency expected_system_metrics_struct_freq =
          SamplingFrequency::kNoSampling) {
    const auto& observed_metrics_and_frequencies =
        system_monitor_->GetMetricSamplingFrequencyArrayForTesting();

    EXPECT_EQ(2U, observed_metrics_and_frequencies.size());
    EXPECT_EQ(expected_free_memory_mb_freq,
              observed_metrics_and_frequencies[static_cast<size_t>(
                  SystemMonitor::MetricEvaluator::Type::kFreeMemoryMb)]);
    EXPECT_EQ(expected_system_metrics_struct_freq,
              observed_metrics_and_frequencies[static_cast<size_t>(
                  SystemMonitor::MetricEvaluator::Type::kSystemMetricsStruct)]);
  }

  std::unique_ptr<SystemMonitor> system_monitor_;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SystemMonitorTest, GetReturnsSingleInstance) {
  EXPECT_EQ(system_monitor_.get(), SystemMonitor::Get());
  system_monitor_.reset();
  EXPECT_EQ(nullptr, SystemMonitor::Get());
}

TEST_F(SystemMonitorTest, AddAndUpdateObservers) {
  SystemObserver obs1;
  SystemObserver obs2;
  SystemObserver obs3;

  // The first observer doesn't observe anything yet.
  MetricsRefreshFrequencies obs1_metrics_frequencies = {};
  system_monitor_->AddOrUpdateObserver(&obs1, obs1_metrics_frequencies);
  EnsureMetricsAreObservedAtExpectedFrequency();

  // Add a second observer that observes the amount of free memory at the
  // default frequency.
  MetricsRefreshFrequencies obs2_metrics_frequencies =
      MetricsRefreshFrequencies::Builder()
          .SetFreePhysMemoryMbFrequency(SamplingFrequency::kDefaultFrequency)
          .Build();

  system_monitor_->AddOrUpdateObserver(&obs2, obs2_metrics_frequencies);
  EnsureMetricsAreObservedAtExpectedFrequency(
      SamplingFrequency::kDefaultFrequency);

  // Add a third observer that observes the amount of free memory and the system
  // metrics at the default frequency.
  MetricsRefreshFrequencies obs3_metrics_frequencies =
      MetricsRefreshFrequencies::Builder()
          .SetFreePhysMemoryMbFrequency(SamplingFrequency::kDefaultFrequency)
          .SetSystemMetricsSamplingFrequency(
              SamplingFrequency::kDefaultFrequency)
          .Build();
  system_monitor_->AddOrUpdateObserver(&obs3, obs3_metrics_frequencies);
  EnsureMetricsAreObservedAtExpectedFrequency(
      SamplingFrequency::kDefaultFrequency,
      SamplingFrequency::kDefaultFrequency);

  // Stop observing any metric with the second observer.
  obs2_metrics_frequencies.free_phys_memory_mb_frequency =
      SamplingFrequency::kNoSampling;
  obs2_metrics_frequencies.system_metrics_sampling_frequency =
      SamplingFrequency::kNoSampling;
  system_monitor_->AddOrUpdateObserver(&obs2, obs2_metrics_frequencies);
  EnsureMetricsAreObservedAtExpectedFrequency(
      SamplingFrequency::kDefaultFrequency,
      SamplingFrequency::kDefaultFrequency);

  // Remove the third observer, ensure that no metrics are observed anymore.
  system_monitor_->RemoveObserver(&obs3);
  EnsureMetricsAreObservedAtExpectedFrequency();
}

TEST_F(SystemMonitorTest, ObserverGetsCalled) {
  ::testing::StrictMock<MockMetricsMonitorObserver> mock_observer_1;
  system_monitor_->AddOrUpdateObserver(
      &mock_observer_1,
      MetricsRefreshFrequencies::Builder()
          .SetFreePhysMemoryMbFrequency(SamplingFrequency::kDefaultFrequency)
          .Build());

  ::testing::StrictMock<MockMetricsMonitorObserver> mock_observer_2;
  system_monitor_->AddOrUpdateObserver(
      &mock_observer_2,
      MetricsRefreshFrequencies::Builder()
          .SetSystemMetricsSamplingFrequency(
              SamplingFrequency::kDefaultFrequency)
          .Build());

  EnsureMetricsAreObservedAtExpectedFrequency(
      SamplingFrequency::kDefaultFrequency,
      SamplingFrequency::kDefaultFrequency);

  // Ensure that we get several samples to verify that the timer logic works.
  EXPECT_CALL(mock_observer_1,
              OnFreePhysicalMemoryMbSample(kFakeFreePhysMemoryMb))
      .Times(2);

  EXPECT_CALL(mock_observer_2, OnSystemMetricsStruct(::testing::_)).Times(2);

  // Fast forward by enough time to get multiple samples and wait for the tasks
  // to complete.
  task_environment_.FastForwardBy(
      2 * system_monitor_->refresh_timer_for_testing().GetCurrentDelay());
  task_environment_.RunUntilIdle();

  ::testing::Mock::VerifyAndClear(&mock_observer_1);
  ::testing::Mock::VerifyAndClear(&mock_observer_2);
}

}  // namespace performance_monitor
