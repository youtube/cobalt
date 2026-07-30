// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_TYPE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_TYPE_H_

#include <string_view>

namespace enterprise_reporting {

enum class ReportType {
  kBrowser = 0,
  kBrowserVersion = 1,
  kProfileReport = 2,
};

inline std::string_view GetReportTypeMetricSuffix(ReportType report_type) {
  switch (report_type) {
    case ReportType::kBrowser:
    case ReportType::kBrowserVersion:
      return "Browser";
    case ReportType::kProfileReport:
      return "Profile";
  }
}

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_TYPE_H_
