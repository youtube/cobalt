// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_DATA_COLLECTOR_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_DATA_COLLECTOR_DESKTOP_H_

#include "components/enterprise/browser/reporting/browser_launch/browser_launch_event_controller.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"

namespace enterprise_reporting {

// Desktop implementation of BrowserLaunchEventController::LaunchDataCollector.
// Captures unpolluted command-line switches and process creation time on
// Windows, Mac, and Linux.
class BrowserLaunchDataCollectorDesktop
    : public BrowserLaunchEventController::LaunchDataCollector {
 public:
  BrowserLaunchDataCollectorDesktop();
  BrowserLaunchDataCollectorDesktop(const BrowserLaunchDataCollectorDesktop&) =
      delete;
  BrowserLaunchDataCollectorDesktop& operator=(
      const BrowserLaunchDataCollectorDesktop&) = delete;
  ~BrowserLaunchDataCollectorDesktop() override;

  // BrowserLaunchEventController::LaunchDataCollector:
  ::chrome::cros::reporting::proto::BrowserLaunchEvent&& GetEvent() override;

 private:
  ::chrome::cros::reporting::proto::BrowserLaunchEvent event_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_DATA_COLLECTOR_DESKTOP_H_
