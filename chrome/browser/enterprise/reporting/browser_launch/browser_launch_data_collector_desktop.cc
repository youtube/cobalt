// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_launch/browser_launch_data_collector_desktop.h"

#include <string>

#include "base/command_line.h"
#include "base/process/process.h"
#include "chrome/browser/enterprise/reporting/browser_launch/scoped_initial_command_line.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"

namespace enterprise_reporting {

BrowserLaunchDataCollectorDesktop::BrowserLaunchDataCollectorDesktop() =
    default;
BrowserLaunchDataCollectorDesktop::~BrowserLaunchDataCollectorDesktop() =
    default;

::chrome::cros::reporting::proto::BrowserLaunchEvent&&
BrowserLaunchDataCollectorDesktop::GetEvent() {
  event_.Clear();

  const base::CommandLine& initial_cli = GetInitialBrowserCommandLine();
  for (const auto& [name, value] : initial_cli.GetSwitches()) {
    event_.add_command_line_switch_keys(name);
  }

  // Capture Process Creation Time.
  event_.set_launch_time_millis(
      base::Process::Current().CreationTime().InMillisecondsSinceUnixEpoch());

  return std::move(event_);
}

}  // namespace enterprise_reporting
