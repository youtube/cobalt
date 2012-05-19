// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor/system_monitor.h"

namespace base {

void SystemMonitor::BeginPowerRequirement(PowerRequirement requirement,
                                          const std::string& reason) {
  if (requirement == CPU_REQUIRED)
    return;

  NOTIMPLEMENTED();
}

void SystemMonitor::EndPowerRequirement(PowerRequirement requirement,
                                        const std::string& reason) {
  if (requirement == CPU_REQUIRED)
    return;  // Nothing to do.

  NOTIMPLEMENTED();
}

bool SystemMonitor::IsBatteryPower() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace base
