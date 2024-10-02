// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"

#include <utility>

#include "chrome/browser/enterprise/reporting/browser_report_generator_android.h"
#include "chrome/browser/enterprise/reporting/profile_report_generator_android.h"
#include "chrome/browser/enterprise/reporting/report_scheduler_android.h"

namespace enterprise_reporting {

std::unique_ptr<BrowserReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetBrowserReportGeneratorDelegate() {
  return std::make_unique<BrowserReportGeneratorAndroid>();
}

std::unique_ptr<ProfileReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetProfileReportGeneratorDelegate() {
  return std::make_unique<ProfileReportGeneratorAndroid>();
}

std::unique_ptr<ReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetReportGeneratorDelegate() {
  return nullptr;
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryAndroid::GetReportSchedulerDelegate() {
  return std::make_unique<ReportSchedulerAndroid>();
}

std::unique_ptr<RealTimeReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetRealTimeReportGeneratorDelegate() {
  // TODO(crbug.com/1228845) Implement RealTimeReportGenerator::Delegate for
  // Android
  return nullptr;
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryAndroid::GetReportSchedulerDelegate(Profile* profile) {
  return std::make_unique<ReportSchedulerAndroid>(profile);
}

}  // namespace enterprise_reporting
