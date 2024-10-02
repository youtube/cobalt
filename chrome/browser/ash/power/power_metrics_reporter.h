// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_POWER_METRICS_REPORTER_H_
#define CHROME_BROWSER_ASH_POWER_POWER_METRICS_REPORTER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/metrics/daily_event.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

// PowerMetricsReporter reports power-management-related metrics.
// Prefs are used to retain metrics across Chrome restarts and system reboots.
class PowerMetricsReporter : public chromeos::PowerManagerClient::Observer {
 public:
  // Histogram names.
  static const char kDailyEventIntervalName[];
  static const char kIdleScreenDimCountName[];
  static const char kIdleScreenOffCountName[];
  static const char kIdleSuspendCountName[];
  static const char kLidClosedSuspendCountName[];

  // Registers prefs used by PowerMetricsReporter in |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // RegisterLocalStatePrefs() must be called before instantiating this class.
  PowerMetricsReporter(chromeos::PowerManagerClient* power_manager_client,
                       PrefService* local_state_pref_service);

  PowerMetricsReporter(const PowerMetricsReporter&) = delete;
  PowerMetricsReporter& operator=(const PowerMetricsReporter&) = delete;

  ~PowerMetricsReporter() override;

  // PowerManagerClient::Observer:
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& state) override;
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta duration) override;

 private:
  friend class PowerMetricsReporterTest;

  class DailyEventObserver;

  // Called by DailyEventObserver whenever a day has elapsed according to
  // |daily_event_|.
  void ReportDailyMetrics(metrics::DailyEvent::IntervalType type);

  // Adds |num| to |pref_name|'s count in |daily_counts_| and updates the
  // corresponding pref.
  void AddToCount(const std::string& pref_name, int num);

  raw_ptr<chromeos::PowerManagerClient, ExperimentalAsh>
      power_manager_client_;                            // Not owned.
  raw_ptr<PrefService, ExperimentalAsh> pref_service_;  // Not owned.

  std::unique_ptr<metrics::DailyEvent> daily_event_;

  // Instructs |daily_event_| to check if a day has passed.
  base::RepeatingTimer timer_;

  // Last-received screen-idle state from powerd.
  power_manager::ScreenIdleState old_screen_idle_state_;

  // Map from local store pref name backing a daily count to the count itself.
  std::map<std::string, int> daily_counts_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_POWER_METRICS_REPORTER_H_
