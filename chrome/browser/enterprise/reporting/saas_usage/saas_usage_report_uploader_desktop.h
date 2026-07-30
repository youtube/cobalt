// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_DESKTOP_H_

#include <string_view>

#include "chrome/browser/enterprise/reporting/realtime_event_upload_helper_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"

namespace enterprise_reporting {

// Base class for uploading SaaS usage reports on desktop platforms.
// Encapsulates the logic for wrapping the feature proto into a generic event.
class SaasUsageReportUploaderDesktop final : public SaasUsageReportUploader {
 public:
  // Browser-level uploader constructor.
  SaasUsageReportUploaderDesktop();
  // Profile-level uploader constructor.
  explicit SaasUsageReportUploaderDesktop(Profile* profile);

  SaasUsageReportUploaderDesktop(const SaasUsageReportUploaderDesktop&) =
      delete;
  SaasUsageReportUploaderDesktop& operator=(
      const SaasUsageReportUploaderDesktop&) = delete;

  ~SaasUsageReportUploaderDesktop() override;

  // SaasUsageReportUploader:
  void UploadReport(
      const ::chrome::cros::reporting::proto::SaasUsageReportEvent& report,
      base::OnceCallback<void(policy::CloudPolicyClient::Result)>
          upload_callback) override;

 private:
  bool IsProfileReporting() const;

  RealtimeEventUploadHelper helper_;
  const raw_ptr<Profile> profile_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_UPLOADER_DESKTOP_H_
