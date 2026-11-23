// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_launch/browser_launch_event_uploader_desktop.h"

#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"
#include "components/policy/core/common/policy_logger.h"

namespace enterprise_reporting {

BrowserLaunchEventUploaderDesktop::BrowserLaunchEventUploaderDesktop()
    : helper_("browser", "browser launch", nullptr), profile_(nullptr) {}

BrowserLaunchEventUploaderDesktop::BrowserLaunchEventUploaderDesktop(
    Profile* profile)
    : helper_("profile", "browser launch", profile), profile_(profile) {
  CHECK(profile);
}

BrowserLaunchEventUploaderDesktop::~BrowserLaunchEventUploaderDesktop() = default;

std::string_view BrowserLaunchEventUploaderDesktop::GetMetricSuffix() const {
  return IsProfileReporting() ? "Profile" : "Browser";
}

void BrowserLaunchEventUploaderDesktop::UploadEvent(
    const ::chrome::cros::reporting::proto::BrowserLaunchEvent& event,
    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
        upload_callback) {
  auto context = helper_.PrepareUpload(IsProfileReporting());
  if (!context) {
    std::move(upload_callback)
        .Run(policy::CloudPolicyClient::Result(
            policy::CloudPolicyClient::NotRegistered()));
    return;
  }

  VLOG_POLICY(1, REPORTING) << "Sending " << helper_.reporting_scope() << " "
                            << helper_.event_name() << " report.";

  ::chrome::cros::reporting::proto::Event wrapper;
  *wrapper.mutable_browser_launch_event() = event;

  context->client->ReportBrowserLaunchEvent(wrapper, context->per_profile,
                                            context->dm_token,
                                            std::move(upload_callback));
}

}  // namespace enterprise_reporting
