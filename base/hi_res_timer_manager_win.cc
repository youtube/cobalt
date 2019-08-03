// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hi_res_timer_manager.h"

#include "base/time.h"

HighResolutionTimerManager::HighResolutionTimerManager()
    : hi_res_clock_available_(false) {
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  system_monitor->AddPowerObserver(this);
  UseHiResClock(!system_monitor->BatteryPower());
}

HighResolutionTimerManager::~HighResolutionTimerManager() {
  base::SystemMonitor::Get()->RemovePowerObserver(this);
  UseHiResClock(false);
}

void HighResolutionTimerManager::OnPowerStateChange(bool on_battery_power) {
  UseHiResClock(!on_battery_power);
}

void HighResolutionTimerManager::UseHiResClock(bool use) {
  if (use == hi_res_clock_available_)
    return;
  hi_res_clock_available_ = use;
  base::Time::EnableHighResolutionTimer(use);
}
