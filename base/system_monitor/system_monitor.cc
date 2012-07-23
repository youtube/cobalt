// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor/system_monitor.h"

#include <utility>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"

namespace base {

static SystemMonitor* g_system_monitor = NULL;

#if defined(ENABLE_BATTERY_MONITORING)
// The amount of time (in ms) to wait before running the initial
// battery check.
static int kDelayedBatteryCheckMs = 10 * 1000;
#endif  // defined(ENABLE_BATTERY_MONITORING)

SystemMonitor::SystemMonitor()
    : power_observer_list_(new ObserverListThreadSafe<PowerObserver>()),
      devices_changed_observer_list_(
          new ObserverListThreadSafe<DevicesChangedObserver>()),
      battery_in_use_(false),
      suspended_(false) {
  DCHECK(!g_system_monitor);
  g_system_monitor = this;

  DCHECK(MessageLoop::current());
#if defined(ENABLE_BATTERY_MONITORING)
  delayed_battery_check_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kDelayedBatteryCheckMs), this,
      &SystemMonitor::BatteryCheck);
#endif  // defined(ENABLE_BATTERY_MONITORING)
#if defined(OS_MACOSX)
  PlatformInit();
#endif
}

SystemMonitor::~SystemMonitor() {
#if defined(OS_MACOSX)
  PlatformDestroy();
#endif
  DCHECK_EQ(this, g_system_monitor);
  g_system_monitor = NULL;
}

// static
SystemMonitor* SystemMonitor::Get() {
  return g_system_monitor;
}

void SystemMonitor::ProcessPowerMessage(PowerEvent event_id) {
  // Suppress duplicate notifications.  Some platforms may
  // send multiple notifications of the same event.
  switch (event_id) {
    case POWER_STATE_EVENT:
      {
        bool on_battery = IsBatteryPower();
        if (on_battery != battery_in_use_) {
          battery_in_use_ = on_battery;
          NotifyPowerStateChange();
        }
      }
      break;
    case RESUME_EVENT:
      if (suspended_) {
        suspended_ = false;
        NotifyResume();
      }
      break;
    case SUSPEND_EVENT:
      if (!suspended_) {
        suspended_ = true;
        NotifySuspend();
      }
      break;
  }
}

void SystemMonitor::ProcessDevicesChanged() {
  NotifyDevicesChanged();
}

void SystemMonitor::ProcessMediaDeviceAttached(const DeviceIdType& id,
                                               const std::string& name,
                                               const FilePath& path) {
  media_device_map_.insert(std::make_pair(id, MakeTuple(id, name, path)));
  NotifyMediaDeviceAttached(id, name, path);
}

void SystemMonitor::ProcessMediaDeviceDetached(const DeviceIdType& id) {
  MediaDeviceMap::iterator it = media_device_map_.find(id);
  if (it != media_device_map_.end())
    media_device_map_.erase(it);
  NotifyMediaDeviceDetached(id);
}

std::vector<SystemMonitor::MediaDeviceInfo>
SystemMonitor::GetAttachedMediaDevices() const {
  std::vector<MediaDeviceInfo> results;
  for (MediaDeviceMap::const_iterator it = media_device_map_.begin();
       it != media_device_map_.end();
       ++it) {
    results.push_back(it->second);
  }
  return results;
}

void SystemMonitor::AddPowerObserver(PowerObserver* obs) {
  power_observer_list_->AddObserver(obs);
}

void SystemMonitor::RemovePowerObserver(PowerObserver* obs) {
  power_observer_list_->RemoveObserver(obs);
}

void SystemMonitor::AddDevicesChangedObserver(DevicesChangedObserver* obs) {
  devices_changed_observer_list_->AddObserver(obs);
}

void SystemMonitor::RemoveDevicesChangedObserver(DevicesChangedObserver* obs) {
  devices_changed_observer_list_->RemoveObserver(obs);
}

void SystemMonitor::NotifyDevicesChanged() {
  DVLOG(1) << "DevicesChanged";
  devices_changed_observer_list_->Notify(
    &DevicesChangedObserver::OnDevicesChanged);
}

void SystemMonitor::NotifyMediaDeviceAttached(const DeviceIdType& id,
                                              const std::string& name,
                                              const FilePath& path) {
  DVLOG(1) << "MediaDeviceAttached with name " << name << " and id " << id;
  devices_changed_observer_list_->Notify(
    &DevicesChangedObserver::OnMediaDeviceAttached, id, name, path);
}

void SystemMonitor::NotifyMediaDeviceDetached(const DeviceIdType& id) {
  DVLOG(1) << "MediaDeviceDetached for id " << id;
  devices_changed_observer_list_->Notify(
    &DevicesChangedObserver::OnMediaDeviceDetached, id);
}

void SystemMonitor::NotifyPowerStateChange() {
  DVLOG(1) << "PowerStateChange: " << (BatteryPower() ? "On" : "Off")
           << " battery";
  power_observer_list_->Notify(&PowerObserver::OnPowerStateChange,
                               BatteryPower());
}

void SystemMonitor::NotifySuspend() {
  DVLOG(1) << "Power Suspending";
  power_observer_list_->Notify(&PowerObserver::OnSuspend);
}

void SystemMonitor::NotifyResume() {
  DVLOG(1) << "Power Resuming";
  power_observer_list_->Notify(&PowerObserver::OnResume);
}

void SystemMonitor::BatteryCheck() {
  ProcessPowerMessage(SystemMonitor::POWER_STATE_EVENT);
}

}  // namespace base
