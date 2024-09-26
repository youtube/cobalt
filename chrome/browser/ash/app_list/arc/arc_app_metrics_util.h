// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_METRICS_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_METRICS_UTIL_H_

#include <map>
#include <string>

#include "base/time/time.h"

namespace arc {

// Helper class to record metrics for app installs.
class ArcAppMetricsUtil {
 public:
  ArcAppMetricsUtil();
  ~ArcAppMetricsUtil();

  // Records the install start time for a specific app.
  void recordAppInstallStartTime(const std::string& app_name);

  // Reports install time delta for an app to UMA.
  void maybeReportInstallTimeDelta(const std::string& app_name);

  // Reports the number of incomplete app installs to UMA.
  void reportIncompleteInstalls();

 private:
  bool installs_requested_ = false;
  std::map<std::string, base::TimeTicks> install_start_time_map_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_METRICS_UTIL_H_
