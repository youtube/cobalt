// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_BATTERY_DISCHARGE_REPORTER_H_
#define CHROME_BROWSER_METRICS_POWER_BATTERY_DISCHARGE_REPORTER_H_

#include <memory>

#include "base/power_monitor/battery_state_sampler.h"
#include "base/power_monitor/power_monitor_buildflags.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class BatteryDischargeReporter : public base::BatteryStateSampler::Observer {
 public:
  explicit BatteryDischargeReporter(
      base::BatteryStateSampler* battery_state_sampler,
      UsageScenarioDataStore* battery_usage_scenario_data_store = nullptr);

  BatteryDischargeReporter(const BatteryDischargeReporter&) = delete;
  BatteryDischargeReporter& operator=(const BatteryDischargeReporter&) = delete;
  BatteryDischargeReporter(BatteryDischargeReporter&&) = delete;
  BatteryDischargeReporter& operator=(BatteryDischargeReporter&&) = delete;

  ~BatteryDischargeReporter() override;

  // base::BatteryStateSampler::Observer:
  void OnBatteryStateSampled(
      const absl::optional<base::BatteryLevelProvider::BatteryState>&
          battery_state) override;

 private:
#if BUILDFLAG(IS_MAC)
  // Records the time delta between two events received from IOPMPowerSource.
  void RecordIOPMPowerSourceSampleEventDelta(
      base::TimeDelta sampling_event_delta);
#endif  // BUILDFLAG(IS_MAC)

  base::ScopedObservation<base::BatteryStateSampler,
                          base::BatteryStateSampler::Observer>
      scoped_battery_state_sampler_observation_{this};

  // Track usage scenarios for the sampling interval.
  std::unique_ptr<UsageScenarioTracker> battery_usage_scenario_tracker_;
  raw_ptr<UsageScenarioDataStore> battery_usage_scenario_data_store_;

  // The time ticks from when the last event was received from
  // |sampling_event_source_|.
  absl::optional<base::TimeTicks> last_event_time_ticks_;

  absl::optional<base::BatteryLevelProvider::BatteryState> last_battery_state_;

  // The first battery sample is potentially outdated because it is not taken
  // upon receiving a notification from the OS. This is used to differentiate
  // the first battery discharge histogram sample from the rest as that initial
  // one is potentially skewed.
  bool is_initial_interval_ = true;
};

#endif  // CHROME_BROWSER_METRICS_POWER_BATTERY_DISCHARGE_REPORTER_H_
