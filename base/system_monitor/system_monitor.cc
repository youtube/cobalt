// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor/system_monitor.h"

#include <utility>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"

namespace base {

static SystemMonitor* g_system_monitor = NULL;

#if defined(ENABLE_BATTERY_MONITORING)
// The amount of time (in ms) to wait before running the initial
// battery check.
static int kDelayedBatteryCheckMs = 10 * 1000;
#endif  // defined(ENABLE_BATTERY_MONITORING)

SystemMonitor::RemovableStorageInfo::RemovableStorageInfo() {
}

SystemMonitor::RemovableStorageInfo::RemovableStorageInfo(
    const std::string& id,
    const string16& device_name,
    const FilePath::StringType& device_location)
    : device_id(id),
      name(device_name),
      location(device_location) {
}

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

void SystemMonitor::ProcessDevicesChanged(DeviceType device_type) {
  NotifyDevicesChanged(device_type);
}

void SystemMonitor::ProcessRemovableStorageAttached(
    const std::string& id,
    const string16& name,
    const FilePath::StringType& location) {
  {
    base::AutoLock lock(removable_storage_lock_);
    if (ContainsKey(removable_storage_map_, id)) {
      // This can happen if our unique id scheme fails. Ignore the incoming
      // non-unique attachment.
      return;
    }
    RemovableStorageInfo info(id, name, location);
    removable_storage_map_.insert(std::make_pair(id, info));
  }
  NotifyRemovableStorageAttached(id, name, location);
}

void SystemMonitor::ProcessRemovableStorageDetached(const std::string& id) {
  {
    base::AutoLock lock(removable_storage_lock_);
    RemovableStorageMap::iterator it = removable_storage_map_.find(id);
    if (it == removable_storage_map_.end())
      return;
    removable_storage_map_.erase(it);
  }
  NotifyRemovableStorageDetached(id);
}

std::vector<SystemMonitor::RemovableStorageInfo>
SystemMonitor::GetAttachedRemovableStorage() const {
  std::vector<RemovableStorageInfo> results;

  base::AutoLock lock(removable_storage_lock_);
  for (RemovableStorageMap::const_iterator it = removable_storage_map_.begin();
       it != removable_storage_map_.end();
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

void SystemMonitor::NotifyDevicesChanged(DeviceType device_type) {
  DVLOG(1) << "DevicesChanged with device type " << device_type;
  devices_changed_observer_list_->Notify(
      &DevicesChangedObserver::OnDevicesChanged, device_type);
}

void SystemMonitor::NotifyRemovableStorageAttached(
    const std::string& id,
    const string16& name,
    const FilePath::StringType& location) {
  DVLOG(1) << "RemovableStorageAttached with name " << UTF16ToUTF8(name)
           << " and id " << id;
  devices_changed_observer_list_->Notify(
    &DevicesChangedObserver::OnRemovableStorageAttached, id, name, location);
}

void SystemMonitor::NotifyRemovableStorageDetached(const std::string& id) {
  DVLOG(1) << "RemovableStorageDetached for id " << id;
  devices_changed_observer_list_->Notify(
    &DevicesChangedObserver::OnRemovableStorageDetached, id);
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
