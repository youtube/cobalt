// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/back_forward_cache/back_forward_cache_disable.h"

#include "content/public/browser/back_forward_cache.h"
#include "third_party/blink/public/common/features_generated.h"

namespace back_forward_cache {

std::string ReasonIdToString(DisabledReasonId reason_id) {
  switch (reason_id) {
    case DisabledReasonId::kUnknown:
      return "Unknown";
    case DisabledReasonId::kPopupBlockerTabHelper:
      return "PopupBlockerTabHelper";
    case DisabledReasonId::kSafeBrowsingTriggeredPopupBlocker:
      return "SafeBrowsingTriggeredPopupBlocker";
    case DisabledReasonId::kSafeBrowsingThreatDetails:
      return "safe_browsing::ThreatDetails";
    case DisabledReasonId::kDomDistillerViewerSource:
      return "DomDistillerViewerSource";
    case DisabledReasonId::kDomDistiller_SelfDeletingRequestDelegate:
      return "browser::DomDistiller_SelfDeletingRequestDelegate";
    case DisabledReasonId::kOfflinePage:
      return "OfflinePage";
    case DisabledReasonId::kChromePasswordManagerClient_BindCredentialManager:
      return "ChromePasswordManagerClient::BindCredentialManager";
    case DisabledReasonId::kPermissionRequestManager:
      return "PermissionRequestManager";
    case DisabledReasonId::kModalDialog:
      return "ModalDialog";
    case DisabledReasonId::kExtensionMessaging:
      return "ExtensionMessaging";
    case DisabledReasonId::kExtensionSentMessageToCachedFrame:
      return "ExtensionSentMessageToCachedFrame";
    case DisabledReasonId::kOomInterventionTabHelper:
      return "OomInterventionTabHelper";
    case DisabledReasonId::kRequestedByWebViewClient:
      return "RequestedByWebViewClient";
    case DisabledReasonId::kPostMessageByWebViewClient:
      return "PostMessageByWebViewClient";
  }
}

// Report string used for NotRestoredReasons API. This will be brief and will
// mask all the non-actionable reasons.
std::string ReasonIdToReportString(DisabledReasonId reason_id) {
  switch (reason_id) {
    case DisabledReasonId::kExtensionMessaging:
    case DisabledReasonId::kExtensionSentMessageToCachedFrame:
      return base::FeatureList::IsEnabled(
                 blink::features::kBackForwardCacheUpdateNotRestoredReasonsName)
                 ? "masked"
                 : "extension-messaging";
    case DisabledReasonId::kPermissionRequestManager:
      return base::FeatureList::IsEnabled(
                 blink::features::kBackForwardCacheUpdateNotRestoredReasonsName)
                 ? "masked"
                 : "pending-permission-request";
    case DisabledReasonId::kModalDialog:
      return "modals";
    default:
      return "masked";
  }
}

content::BackForwardCache::DisabledReason DisabledReason(
    DisabledReasonId reason_id,
    const std::string& context) {
  return content::BackForwardCache::DisabledReason(
      content::BackForwardCache::DisabledSource::kEmbedder,
      static_cast<content::BackForwardCache::DisabledReasonType>(reason_id),
      ReasonIdToString(reason_id), context, ReasonIdToReportString(reason_id));
}
}  // namespace back_forward_cache
