// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/battery_discharge_reporter.h"

#include <memory>

#include "base/power_monitor/battery_level_provider.h"
#include "base/power_monitor/battery_state_sampler.h"
#include "base/power_monitor/sampling_event_source.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/metrics/power/power_metrics.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr const char* kBatteryDischargeModeHistogramName =
    "Power.BatteryDischargeMode5";
constexpr const char* kBatteryDischargeRateMilliwattsHistogramName =
    "Power.BatteryDischargeRateMilliwatts6";
constexpr const char* kBatteryDischargeRateRelativeHistogramName =
    "Power.BatteryDischargeRateRelative5";

constexpr base::TimeDelta kTolerableDrift = base::Seconds(1);
constexpr int kFullBatteryChargeLevel = 10000;
constexpr int kHalfBatteryChargeLevel = 5000;

absl::optional<base::BatteryLevelProvider::BatteryState> MakeBatteryState(
    int current_capacity) {
  return base::BatteryLevelProvider::BatteryState{
      .battery_count = 1,
      .is_external_power_connected = false,
      .current_capacity = current_capacity,
      .full_charged_capacity = kFullBatteryChargeLevel,
      .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh};
}

struct HistogramSampleExpectation {
  std::string histogram_name_prefix;
  base::Histogram::Sample sample;
};

// For each histogram named after the combination of prefixes from
// `expectations` and suffixes from `suffixes`, verifies that there is a unique
// sample `expectation.sample`.
void ExpectHistogramSamples(
    base::HistogramTester* histogram_tester,
    const std::vector<const char*>& suffixes,
    const std::vector<HistogramSampleExpectation>& expectations) {
  for (const char* suffix : suffixes) {
    for (const auto& expectation : expectations) {
      std::string histogram_name =
          base::StrCat({expectation.histogram_name_prefix, suffix});
      SCOPED_TRACE(histogram_name);
      histogram_tester->ExpectUniqueSample(histogram_name, expectation.sample,
                                           1);
    }
  }
}

class NoopSamplingEventSource : public base::SamplingEventSource {
 public:
  NoopSamplingEventSource() = default;
  ~NoopSamplingEventSource() override = default;

  bool Start(SamplingEventCallback callback) override { return true; }
};

class NoopBatteryLevelProvider : public base::BatteryLevelProvider {
 public:
  NoopBatteryLevelProvider() = default;
  ~NoopBatteryLevelProvider() override = default;

  void GetBatteryState(
      base::OnceCallback<void(const absl::optional<BatteryState>&)> callback)
      override {}
};

class TestUsageScenarioDataStoreImpl : public UsageScenarioDataStoreImpl {
 public:
  TestUsageScenarioDataStoreImpl() = default;
  TestUsageScenarioDataStoreImpl(const TestUsageScenarioDataStoreImpl& rhs) =
      delete;
  TestUsageScenarioDataStoreImpl& operator=(
      const TestUsageScenarioDataStoreImpl& rhs) = delete;
  ~TestUsageScenarioDataStoreImpl() override = default;

  IntervalData ResetIntervalData() override { return fake_data_; }

 private:
  IntervalData fake_data_;
};

class BatteryDischargeReporterTest : public testing::Test {
 public:
  BatteryDischargeReporterTest() = default;
  BatteryDischargeReporterTest(const BatteryDischargeReporterTest& rhs) =
      delete;
  BatteryDischargeReporterTest& operator=(
      const BatteryDischargeReporterTest& rhs) = delete;
  ~BatteryDischargeReporterTest() override = default;

  // Tests that the right BatteryDischargeMode histogram sample is emitted given
  // the battery states before and after an interval.
  void TestBatteryDischargeMode(
      const absl::optional<base::BatteryLevelProvider::BatteryState>&
          previous_battery_state,
      const absl::optional<base::BatteryLevelProvider::BatteryState>&
          new_battery_state,
      BatteryDischargeMode expected_mode) {
    TestUsageScenarioDataStoreImpl usage_scenario_data_store;

    base::BatteryStateSampler battery_state_sampler(
        std::make_unique<NoopSamplingEventSource>(),
        std::make_unique<NoopBatteryLevelProvider>());
    BatteryDischargeReporter battery_discharge_reporter(
        &battery_state_sampler, &usage_scenario_data_store);

    battery_discharge_reporter.OnBatteryStateSampled(previous_battery_state);
    task_environment_.FastForwardBy(base::Minutes(1));
    battery_discharge_reporter.OnBatteryStateSampled(new_battery_state);

    const std::vector<const char*> suffixes(
        {"", ".Initial", ".ZeroWindow", ".ZeroWindow.Initial"});
    ExpectHistogramSamples(&histogram_tester_, suffixes,
                           {{kBatteryDischargeModeHistogramName,
                             static_cast<int>(expected_mode)}});
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::HistogramTester histogram_tester_;
};

}  // namespace

TEST_F(BatteryDischargeReporterTest, Simple) {
  TestUsageScenarioDataStoreImpl usage_scenario_data_store;

  base::BatteryStateSampler battery_state_sampler(
      std::make_unique<NoopSamplingEventSource>(),
      std::make_unique<NoopBatteryLevelProvider>());
  BatteryDischargeReporter battery_discharge_reporter(
      &battery_state_sampler, &usage_scenario_data_store);

  battery_discharge_reporter.OnBatteryStateSampled(
      MakeBatteryState(kHalfBatteryChargeLevel));
  task_environment_.FastForwardBy(base::Minutes(1));
  battery_discharge_reporter.OnBatteryStateSampled(
      MakeBatteryState(kHalfBatteryChargeLevel - 10));

  // 10 mWh discharge over 1 minute equals 600 mW.
  const int64_t kExpectedDischargeRate = 600;
  // 10 mWh discharge when capacity is 10000 mWh is 10 hundredth of a percent.
  const int64_t kExpectedDischargeRateRelative = 10;

  const std::vector<const char*> suffixes(
      {"", ".Initial", ".ZeroWindow", ".ZeroWindow.Initial"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{kBatteryDischargeModeHistogramName,
        static_cast<int64_t>(BatteryDischargeMode::kDischarging)}});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{kBatteryDischargeRateMilliwattsHistogramName, kExpectedDischargeRate}});
  ExpectHistogramSamples(&histogram_tester_, suffixes,
                         {{kBatteryDischargeRateRelativeHistogramName,
                           kExpectedDischargeRateRelative}});
}

TEST_F(BatteryDischargeReporterTest, BatteryDischargeCaptureIsTooLate) {
  TestUsageScenarioDataStoreImpl usage_scenario_data_store;

  base::BatteryStateSampler battery_state_sampler(
      std::make_unique<NoopSamplingEventSource>(),
      std::make_unique<NoopBatteryLevelProvider>());
  BatteryDischargeReporter battery_discharge_reporter(
      &battery_state_sampler, &usage_scenario_data_store);

  battery_discharge_reporter.OnBatteryStateSampled(MakeBatteryState(5000));

  const base::TimeDelta kTooLate = base::Minutes(1) + kTolerableDrift;
  task_environment_.FastForwardBy(kTooLate);
  battery_discharge_reporter.OnBatteryStateSampled(
      MakeBatteryState(kHalfBatteryChargeLevel - 10));

  // No rate because the interval is invalid.
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kInvalidInterval,
                                       1);
  histogram_tester_.ExpectTotalCount(
      kBatteryDischargeRateMilliwattsHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateRelativeHistogramName,
                                     0);
}

TEST_F(BatteryDischargeReporterTest, BatteryDischargeCaptureIsLate) {
  TestUsageScenarioDataStoreImpl usage_scenario_data_store;

  base::BatteryStateSampler battery_state_sampler(
      std::make_unique<NoopSamplingEventSource>(),
      std::make_unique<NoopBatteryLevelProvider>());
  BatteryDischargeReporter battery_discharge_reporter(
      &battery_state_sampler, &usage_scenario_data_store);

  battery_discharge_reporter.OnBatteryStateSampled(
      MakeBatteryState(kHalfBatteryChargeLevel));

  const base::TimeDelta kLate =
      base::Minutes(1) + kTolerableDrift - base::Microseconds(1);
  task_environment_.FastForwardBy(kLate);
  battery_discharge_reporter.OnBatteryStateSampled(
      MakeBatteryState(kHalfBatteryChargeLevel - 10));

  // No rate because the interval is invalid.
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectTotalCount(
      kBatteryDischargeRateMilliwattsHistogramName, 1);
  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateRelativeHistogramName,
                                     1);
}

TEST_F(BatteryDischargeReporterTest, BatteryDischargeCaptureIsTooEarly) {
  TestUsageScenarioDataStoreImpl usage_scenario_data_store;

  base::BatteryStateSampler battery_state_sampler(
      std::make_unique<NoopSamplingEventSource>(),
      std::make_unique<NoopBatteryLevelProvider>());
  BatteryDischargeReporter battery_discharge_reporter(
      &battery_state_sampler, &usage_scenario_data_store);

  battery_discharge_reporter.OnBatteryStateSampled(
      MakeBatteryState(kHalfBatteryChargeLevel));

  const base::TimeDelta kTooEarly = base::Minutes(1) - kTolerableDrift;
  task_environment_.FastForwardBy(kTooEarly);
  battery_discharge_reporter.OnBatteryStateSampled(
      MakeBatteryState(kHalfBatteryChargeLevel - 10));

  // No rate because the interval is invalid.
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kInvalidInterval,
                                       1);
  histogram_tester_.ExpectTotalCount(
      kBatteryDischargeRateMilliwattsHistogramName, 0);
  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateRelativeHistogramName,
                                     0);
}

TEST_F(BatteryDischargeReporterTest, BatteryDischargeCaptureIsEarly) {
  TestUsageScenarioDataStoreImpl usage_scenario_data_store;

  base::BatteryStateSampler battery_state_sampler(
      std::make_unique<NoopSamplingEventSource>(),
      std::make_unique<NoopBatteryLevelProvider>());
  BatteryDischargeReporter battery_discharge_reporter(
      &battery_state_sampler, &usage_scenario_data_store);

  battery_discharge_reporter.OnBatteryStateSampled(
      MakeBatteryState(kHalfBatteryChargeLevel));

  const base::TimeDelta kEarly =
      base::Minutes(1) - kTolerableDrift + base::Microseconds(1);
  task_environment_.FastForwardBy(kEarly);
  battery_discharge_reporter.OnBatteryStateSampled(
      MakeBatteryState(kHalfBatteryChargeLevel - 10));

  // No rate because the interval is invalid.
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectTotalCount(
      kBatteryDischargeRateMilliwattsHistogramName, 1);
  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateRelativeHistogramName,
                                     1);
}

TEST_F(BatteryDischargeReporterTest, FullChargedCapacityIncreased) {
  TestUsageScenarioDataStoreImpl usage_scenario_data_store;

  base::BatteryStateSampler battery_state_sampler(
      std::make_unique<NoopSamplingEventSource>(),
      std::make_unique<NoopBatteryLevelProvider>());
  BatteryDischargeReporter battery_discharge_reporter(
      &battery_state_sampler, &usage_scenario_data_store);

  battery_discharge_reporter.OnBatteryStateSampled(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 40,
          .full_charged_capacity = 100,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      });
  task_environment_.FastForwardBy(base::Minutes(1));
  battery_discharge_reporter.OnBatteryStateSampled(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 40,
          .full_charged_capacity = 110,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      });

  // Full charged capacity increased. Used capacity went from 60 mWh to 70 mwh,
  // which is interpreted as a 10 mWh discharge. 10 mWh discharge over 1 minute
  // equals 600 mW.
  const int64_t kExpectedDischargeRate = 600;

  const std::vector<const char*> suffixes(
      {"", ".Initial", ".ZeroWindow", ".ZeroWindow.Initial"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{kBatteryDischargeRateMilliwattsHistogramName, kExpectedDischargeRate}});
}

TEST_F(BatteryDischargeReporterTest, RetrievalError) {
  TestBatteryDischargeMode(absl::nullopt, absl::nullopt,
                           BatteryDischargeMode::kRetrievalError);
}

TEST_F(BatteryDischargeReporterTest, StateChanged_Battery) {
  TestBatteryDischargeMode(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 0,
      },
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
      },
      BatteryDischargeMode::kStateChanged);
}

TEST_F(BatteryDischargeReporterTest, StateChanged_PluggedIn) {
  TestBatteryDischargeMode(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = true,
      },
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
      },
      BatteryDischargeMode::kStateChanged);
}

TEST_F(BatteryDischargeReporterTest, NoBattery) {
  TestBatteryDischargeMode(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 0,
      },
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 0,
      },
      BatteryDischargeMode::kNoBattery);
}

TEST_F(BatteryDischargeReporterTest, PluggedIn) {
  TestBatteryDischargeMode(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = true,
      },
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = true,
      },
      BatteryDischargeMode::kPluggedIn);
}

TEST_F(BatteryDischargeReporterTest, MultipleBatteries) {
  TestBatteryDischargeMode(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 2,
          .is_external_power_connected = false,
      },
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 2,
          .is_external_power_connected = false,
      },
      BatteryDischargeMode::kMultipleBatteries);
}

TEST_F(BatteryDischargeReporterTest, InsufficientResolution) {
  TestBatteryDischargeMode(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .charge_unit =
              base::BatteryLevelProvider::BatteryLevelUnit::kRelative,
      },
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .charge_unit =
              base::BatteryLevelProvider::BatteryLevelUnit::kRelative,
      },
      BatteryDischargeMode::kInsufficientResolution);
}

#if BUILDFLAG(IS_MAC)
TEST_F(BatteryDischargeReporterTest, MacFullyCharged) {
  TestBatteryDischargeMode(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 100,
          .full_charged_capacity = 100,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      },
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 99,
          .full_charged_capacity = 100,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      },
      BatteryDischargeMode::kMacFullyCharged);
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(BatteryDischargeReporterTest, FullChargedCapacityIsZero) {
  TestBatteryDischargeMode(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 10,
          .full_charged_capacity = 0,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      },
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 10,
          .full_charged_capacity = 0,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      },
      BatteryDischargeMode::kFullChargedCapacityIsZero);
}

TEST_F(BatteryDischargeReporterTest, BatteryLevelIncreased) {
  TestBatteryDischargeMode(
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 40,
          .full_charged_capacity = 100,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      },
      base::BatteryLevelProvider::BatteryState{
          .battery_count = 1,
          .is_external_power_connected = false,
          .current_capacity = 50,
          .full_charged_capacity = 100,
          .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      },
      BatteryDischargeMode::kBatteryLevelIncreased);
}

#if BUILDFLAG(IS_WIN)
TEST_F(BatteryDischargeReporterTest, BatteryDischargeGranularity) {
  TestUsageScenarioDataStoreImpl usage_scenario_data_store;

  base::BatteryStateSampler battery_state_sampler(
      std::make_unique<NoopSamplingEventSource>(),
      std::make_unique<NoopBatteryLevelProvider>());
  BatteryDischargeReporter battery_discharge_reporter(
      &battery_state_sampler, &usage_scenario_data_store);

  const int64_t kGranularityMilliwattHours = 10;
  // Since the full charged capacity is 1000, a granularity of 10 is equal to
  // one percent, or 100 hundredths of a percent.
  const int64_t kGranularityRelative = 100;

  const auto kBatteryState = base::BatteryLevelProvider::BatteryState{
      .battery_count = 1,
      .is_external_power_connected = false,
      .current_capacity = 500,
      .full_charged_capacity = 1000,
      .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      .battery_discharge_granularity = kGranularityMilliwattHours,
  };

  battery_discharge_reporter.OnBatteryStateSampled(kBatteryState);
  task_environment_.FastForwardBy(base::Minutes(1));
  battery_discharge_reporter.OnBatteryStateSampled(kBatteryState);

  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeGranularityMilliwattHours2",
      kGranularityMilliwattHours, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeGranularityRelative2", kGranularityRelative, 1);
}
#endif  // BUILDFLAG(IS_WIN)
