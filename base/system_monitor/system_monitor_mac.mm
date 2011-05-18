// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation based on sample code from
// http://developer.apple.com/library/mac/#qa/qa1340/_index.html.

#include "base/system_monitor/system_monitor.h"

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

namespace base {

namespace {

io_connect_t g_system_power_io_port = 0;

void SystemPowerEventCallback(void* system_monitor,
                              io_service_t service,
                              natural_t message_type,
                              void* message_argument) {
  DCHECK(system_monitor);
  SystemMonitor* sys_monitor = reinterpret_cast<SystemMonitor*>(system_monitor);
  switch (message_type) {
    case kIOMessageSystemWillSleep:
      sys_monitor->ProcessPowerMessage(SystemMonitor::SUSPEND_EVENT);
      IOAllowPowerChange(g_system_power_io_port,
          reinterpret_cast<int>(message_argument));
      break;

    case kIOMessageSystemWillPowerOn:
      sys_monitor->ProcessPowerMessage(SystemMonitor::RESUME_EVENT);
      break;
  }
}

}  // namespace

void SystemMonitor::PlatformInit() {
  DCHECK_EQ(g_system_power_io_port, 0u);

  // Notification port allocated by IORegisterForSystemPower.

  g_system_power_io_port = IORegisterForSystemPower(
      this, &notification_port_ref_, SystemPowerEventCallback,
      &notifier_object_);
  DCHECK_NE(g_system_power_io_port, 0u);

  // Add the notification port to the application runloop
  CFRunLoopAddSource(CFRunLoopGetCurrent(),
                     IONotificationPortGetRunLoopSource(notification_port_ref_),
                     kCFRunLoopCommonModes);
}

void SystemMonitor::PlatformDestroy() {
  DCHECK_NE(g_system_power_io_port, 0u);

  // Remove the sleep notification port from the application runloop
  CFRunLoopRemoveSource(
      CFRunLoopGetCurrent(),
      IONotificationPortGetRunLoopSource(notification_port_ref_),
      kCFRunLoopCommonModes);

  // Deregister for system sleep notifications
  IODeregisterForSystemPower(&notifier_object_);

  // IORegisterForSystemPower implicitly opens the Root Power Domain IOService,
  // so we close it here.
  IOServiceClose(g_system_power_io_port);

  g_system_power_io_port = 0;

  // Destroy the notification port allocated by IORegisterForSystemPower.
  IONotificationPortDestroy(notification_port_ref_);
}

}  // namespace base
