// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_UTILS_H_

#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace enterprise_connectors {

// Helper functions that compiles information into event protos. The
// logic is shared across platforms to ensure event consistency.
//
// PasswordBreachEvent could be empty if none of the `identities` matched a
// pattern in the URL filters.
std::optional<chrome::cros::reporting::proto::PasswordBreachEvent>
GetPasswordBreachEvent(
    const std::string& trigger,
    const std::vector<std::pair<GURL, std::u16string>>& identities,
    const enterprise_connectors::ReportingSettings& settings);

chrome::cros::reporting::proto::SafeBrowsingPasswordReuseEvent
GetPasswordReuseEvent(const GURL& url,
                      const std::string& user_name,
                      bool is_phishing_url,
                      bool warning_shown);

chrome::cros::reporting::proto::SafeBrowsingPasswordChangedEvent
GetPasswordChangedEvent(const std::string& user_name);

chrome::cros::reporting::proto::LoginEvent GetLoginEvent(
    const GURL& url,
    bool is_federated,
    const url::SchemeHostPort& federated_origin,
    const std::u16string& username);

chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent
GetInterstitialEvent(const GURL& url,
                     const std::string& reason,
                     int net_error_code,
                     bool clicked_through,
                     EventResult event_result);

chrome::cros::reporting::proto::BrowserCrashEvent GetBrowserCrashEvent(
    const std::string& channel,
    const std::string& version,
    const std::string& report_id,
    const std::string& platform);

// Returns a list of the local IPv4 and IPv6 addresses of the device.
std::vector<std::string> GetLocalIpAddresses();

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_UTILS_H_
