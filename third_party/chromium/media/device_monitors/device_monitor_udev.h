// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is used to detect device change and notify base::SystemMonitor
// on Linux.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_DEVICE_MONITORS_DEVICE_MONITOR_UDEV_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_DEVICE_MONITORS_DEVICE_MONITOR_UDEV_H_

#include <memory>

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "third_party/chromium/media/base/media_export.h"

namespace media_m96 {

class MEDIA_EXPORT DeviceMonitorLinux {
 public:
  DeviceMonitorLinux();

  DeviceMonitorLinux(const DeviceMonitorLinux&) = delete;
  DeviceMonitorLinux& operator=(const DeviceMonitorLinux&) = delete;

  ~DeviceMonitorLinux();

  // TODO(mcasas): Consider adding a StartMonitoring() method like
  // DeviceMonitorMac to reduce startup impact time.

 private:
  class BlockingTaskRunnerHelper;

  // Task for running udev code that can potentially block.
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Holds the BlockingTaskRunnerHelper which runs tasks on
  // |blocking_task_runner_|.
  std::unique_ptr<BlockingTaskRunnerHelper, base::OnTaskRunnerDeleter>
      blocking_task_helper_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_DEVICE_MONITORS_DEVICE_MONITOR_UDEV_H_
