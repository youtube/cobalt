// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_UPLOADER_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_UPLOADER_DESKTOP_H_

#include <string_view>

#include "chrome/browser/enterprise/reporting/realtime_event_upload_helper_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/reporting/browser_launch/browser_launch_event_uploader.h"

namespace enterprise_reporting {

// Base class for uploading browser launch events on desktop platforms.
// Encapsulates the logic for wrapping the feature proto into a generic event.
class BrowserLaunchEventUploaderDesktop final : public BrowserLaunchEventUploader {
 public:
  // Browser-level uploader constructor.
  BrowserLaunchEventUploaderDesktop();
  // Profile-level uploader constructor.
  explicit BrowserLaunchEventUploaderDesktop(Profile* profile);

  BrowserLaunchEventUploaderDesktop(const BrowserLaunchEventUploaderDesktop&) =
      delete;
  BrowserLaunchEventUploaderDesktop& operator=(
      const BrowserLaunchEventUploaderDesktop&) = delete;

  ~BrowserLaunchEventUploaderDesktop() override;

  // BrowserLaunchEventUploader:
  std::string_view GetMetricSuffix() const override;
  void UploadEvent(
      const ::chrome::cros::reporting::proto::BrowserLaunchEvent& event,
      base::OnceCallback<void(policy::CloudPolicyClient::Result)>
          upload_callback) override;

 private:
  bool IsProfileReporting() const { return profile_ != nullptr; }

  RealtimeEventUploadHelper helper_;
  const raw_ptr<Profile> profile_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_UPLOADER_DESKTOP_H_
