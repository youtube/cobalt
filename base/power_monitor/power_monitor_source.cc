// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor_source.h"

#include "base/notreached.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "build/build_config.h"

namespace base {

PowerMonitorSource::PowerMonitorSource() = default;
PowerMonitorSource::~PowerMonitorSource() = default;

PowerThermalObserver::DeviceThermalState
PowerMonitorSource::GetCurrentThermalState() {
  return PowerThermalObserver::DeviceThermalState::kUnknown;
}

int PowerMonitorSource::GetInitialSpeedLimit() {
  return PowerThermalObserver::kSpeedLimitMax;
}

void PowerMonitorSource::SetCurrentThermalState(
    PowerThermalObserver::DeviceThermalState state) {}

#if BUILDFLAG(IS_ANDROID)
int PowerMonitorSource::GetRemainingBatteryCapacity() {
  return 0;
}
#endif  // BUILDFLAG(IS_ANDROID)

// static
void PowerMonitorSource::ProcessPowerEvent(PowerEvent event_id) {
  if (!PowerMonitor::IsInitialized())
    return;

  switch (event_id) {
    case POWER_STATE_EVENT:
      PowerMonitor::NotifyPowerStateChange(
          PowerMonitor::Source()->IsOnBatteryPower());
      break;
      case RESUME_EVENT:
        PowerMonitor::NotifyResume();
      break;
      case SUSPEND_EVENT:
        PowerMonitor::NotifySuspend();
      break;
  }
}

// static
void PowerMonitorSource::ProcessThermalEvent(
    PowerThermalObserver::DeviceThermalState new_thermal_state) {
  if (!PowerMonitor::IsInitialized())
    return;
  PowerMonitor::NotifyThermalStateChange(new_thermal_state);
}

// static
void PowerMonitorSource::ProcessSpeedLimitEvent(int speed_limit) {
  if (!PowerMonitor::IsInitialized())
    return;
  PowerMonitor::NotifySpeedLimitChange(speed_limit);
}

// static
const char* PowerMonitorSource::DeviceThermalStateToString(
    PowerThermalObserver::DeviceThermalState state) {
  switch (state) {
    case PowerThermalObserver::DeviceThermalState::kUnknown:
      return "Unknown";
    case PowerThermalObserver::DeviceThermalState::kNominal:
      return "Nominal";
    case PowerThermalObserver::DeviceThermalState::kFair:
      return "Fair";
    case PowerThermalObserver::DeviceThermalState::kSerious:
      return "Serious";
    case PowerThermalObserver::DeviceThermalState::kCritical:
      return "Critical";
  }
  NOTREACHED();
  return "Unknown";
}

}  // namespace base
