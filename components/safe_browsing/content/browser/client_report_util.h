// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_REPORT_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_REPORT_UTIL_H_

#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/unsafe_resource.h"

// These are utils for ClientSafeBrowsing reports.
namespace safe_browsing::client_report_utils {

// Helper function that converts SBThreatType to
// ClientSafeBrowsingReportRequest::ReportType.
ClientSafeBrowsingReportRequest::ReportType GetReportTypeFromSBThreatType(
    SBThreatType threat_type);

// Helper function that converts mojom::RequestDestination to
// ClientSafeBrowsingReportRequest::UrlRequestDestination.
ClientSafeBrowsingReportRequest::UrlRequestDestination
GetUrlRequestDestinationFromMojomRequestDestination(
    network::mojom::RequestDestination request_destination);

// Helper function that converts SecurityInterstitialCommand to CSBRR
// SecurityInterstitialInteraction.
ClientSafeBrowsingReportRequest::InterstitialInteraction::
    SecurityInterstitialInteraction
    GetSecurityInterstitialInteractionFromCommand(
        security_interstitials::SecurityInterstitialCommand command);

// Helper function that returns true if we can send reports for the url.
bool IsReportableUrl(const GURL& url);

// Return page url from the resource.
GURL GetPageUrl(const security_interstitials::UnsafeResource& resource);

// Return referrer url from the resource.
GURL GetReferrerUrl(const security_interstitials::UnsafeResource& resource);

// Set url, type, and url_request_destination fields in
// `report` from `resource`.
void FillReportBasicResourceDetails(
    ClientSafeBrowsingReportRequest* report,
    const security_interstitials::UnsafeResource& resource);

// Helper that creates new InterstitialInteraction objects and adds them to the
// report.
void FillInterstitialInteractionsHelper(
    ClientSafeBrowsingReportRequest* report,
    security_interstitials::InterstitialInteractionMap*
        interstitial_interactions);

}  // namespace safe_browsing::client_report_utils

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_REPORT_UTIL_H_
