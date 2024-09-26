// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_instrumentation.h"

#include "base/containers/adapters.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/traced_value.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_item.h"
#include "content/browser/devtools/browser_devtools_agent_host.h"
#include "content/browser/devtools/devtools_issue_storage.h"
#include "content/browser/devtools/devtools_url_loader_interceptor.h"
#include "content/browser/devtools/protocol/audits.h"
#include "content/browser/devtools/protocol/audits_handler.h"
#include "content/browser/devtools/protocol/browser_handler.h"
#include "content/browser/devtools/protocol/device_access_handler.h"
#include "content/browser/devtools/protocol/emulation_handler.h"
#include "content/browser/devtools/protocol/fedcm_handler.h"
#include "content/browser/devtools/protocol/fetch_handler.h"
#include "content/browser/devtools/protocol/input_handler.h"
#include "content/browser/devtools/protocol/log_handler.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/protocol/preload_handler.h"
#include "content/browser/devtools/protocol/security_handler.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/web_contents_devtools_agent_host.h"
#include "content/browser/devtools/worker_devtools_agent_host.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/portal/portal.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/public/browser/browser_context.h"
#include "devtools_instrumentation.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/load_flags.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/http/http_request_headers.h"
#include "net/quic/web_transport_error.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"

namespace content {
namespace devtools_instrumentation {

namespace {

const char kPrivacySandboxExtensionsAPI[] = "PrivacySandboxExtensionsAPI";

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToAgents(DevToolsAgentHostImpl* host,
                      void (Handler::*method)(MethodArgs...),
                      Args&&... args) {
  if (!host)
    return;
  for (auto* h : Handler::ForAgentHost(host))
    (h->*method)(std::forward<Args>(args)...);
}

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToAgents(FrameTreeNode* frame_tree_node,
                      void (Handler::*method)(MethodArgs...),
                      Args&&... args) {
  DevToolsAgentHostImpl* agent_host =
      RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
  DispatchToAgents(agent_host, method, std::forward<Args>(args)...);
}

template <typename Handler, typename... MethodArgs, typename... Args>
void DispatchToAgents(int frame_tree_node_id,
                      void (Handler::*method)(MethodArgs...),
                      Args&&... args) {
  FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (ftn)
    DispatchToAgents(ftn, method, std::forward<Args>(args)...);
}

std::unique_ptr<protocol::Audits::InspectorIssue> BuildHeavyAdIssue(
    const blink::mojom::HeavyAdIssueDetailsPtr& issue_details) {
  protocol::String status =
      (issue_details->resolution ==
       blink::mojom::HeavyAdResolutionStatus::kHeavyAdBlocked)
          ? protocol::Audits::HeavyAdResolutionStatusEnum::HeavyAdBlocked
          : protocol::Audits::HeavyAdResolutionStatusEnum::HeavyAdWarning;
  protocol::String reason_string;
  switch (issue_details->reason) {
    case blink::mojom::HeavyAdReason::kNetworkTotalLimit:
      reason_string = protocol::Audits::HeavyAdReasonEnum::NetworkTotalLimit;
      break;
    case blink::mojom::HeavyAdReason::kCpuTotalLimit:
      reason_string = protocol::Audits::HeavyAdReasonEnum::CpuTotalLimit;
      break;
    case blink::mojom::HeavyAdReason::kCpuPeakLimit:
      reason_string = protocol::Audits::HeavyAdReasonEnum::CpuPeakLimit;
      break;
  }
  auto heavy_ad_details =
      protocol::Audits::HeavyAdIssueDetails::Create()
          .SetReason(reason_string)
          .SetResolution(status)
          .SetFrame(protocol::Audits::AffectedFrame::Create()
                        .SetFrameId(issue_details->frame->frame_id)
                        .Build())
          .Build();

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetHeavyAdIssueDetails(std::move(heavy_ad_details))
          .Build();
  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(protocol::Audits::InspectorIssueCodeEnum::HeavyAdIssue)
          .SetDetails(std::move(protocol_issue_details))
          .Build();
  return issue;
}

std::unique_ptr<protocol::Audits::InspectorIssue> BuildTWAQualityIssue(
    const blink::mojom::TrustedWebActivityIssueDetailsPtr& issue_details) {
  protocol::String type_string;
  switch (issue_details->violation_type) {
    case blink::mojom::TwaQualityEnforcementViolationType::kHttpError:
      type_string =
          protocol::Audits::TwaQualityEnforcementViolationTypeEnum::KHttpError;
      break;
    case blink::mojom::TwaQualityEnforcementViolationType::kUnavailableOffline:
      type_string = protocol::Audits::TwaQualityEnforcementViolationTypeEnum::
          KUnavailableOffline;
      break;
    case blink::mojom::TwaQualityEnforcementViolationType::kDigitalAssetLinks:
      type_string = protocol::Audits::TwaQualityEnforcementViolationTypeEnum::
          KDigitalAssetLinks;
      break;
  }

  auto twa_details = protocol::Audits::TrustedWebActivityIssueDetails::Create()
                         .SetUrl(issue_details->url.spec())
                         .SetViolationType(type_string)
                         .Build();
  if (issue_details->http_error_code)
    twa_details->SetHttpStatusCode(issue_details->http_error_code);
  if (issue_details->package_name)
    twa_details->SetPackageName(*issue_details->package_name);
  if (issue_details->signature)
    twa_details->SetSignature(*issue_details->signature);

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetTwaQualityEnforcementDetails(std::move(twa_details))
          .Build();
  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(
              protocol::Audits::InspectorIssueCodeEnum::TrustedWebActivityIssue)
          .SetDetails(std::move(protocol_issue_details))
          .Build();
  return issue;
}

std::string FederatedAuthRequestResultToProtocol(
    blink::mojom::FederatedAuthRequestResult result) {
  using blink::mojom::FederatedAuthRequestResult;
  namespace FederatedAuthRequestIssueReasonEnum =
      protocol::Audits::FederatedAuthRequestIssueReasonEnum;
  switch (result) {
    case FederatedAuthRequestResult::kShouldEmbargo: {
      return FederatedAuthRequestIssueReasonEnum::ShouldEmbargo;
    }
    case FederatedAuthRequestResult::kErrorDisabledInSettings: {
      return FederatedAuthRequestIssueReasonEnum::DisabledInSettings;
    }
    case FederatedAuthRequestResult::kErrorTooManyRequests: {
      return FederatedAuthRequestIssueReasonEnum::TooManyRequests;
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownHttpNotFound;
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownNoResponse;
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownInvalidResponse;
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownListEmpty: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownListEmpty;
    }
    case FederatedAuthRequestResult::
        kErrorFetchingWellKnownInvalidContentType: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownInvalidContentType;
    }
    case FederatedAuthRequestResult::kErrorConfigNotInWellKnown: {
      return FederatedAuthRequestIssueReasonEnum::ConfigNotInWellKnown;
    }
    case FederatedAuthRequestResult::kErrorWellKnownTooBig: {
      return FederatedAuthRequestIssueReasonEnum::WellKnownTooBig;
    }
    case FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound: {
      return FederatedAuthRequestIssueReasonEnum::ConfigHttpNotFound;
    }
    case FederatedAuthRequestResult::kErrorFetchingConfigNoResponse: {
      return FederatedAuthRequestIssueReasonEnum::ConfigNoResponse;
    }
    case FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse: {
      return FederatedAuthRequestIssueReasonEnum::ConfigInvalidResponse;
    }
    case FederatedAuthRequestResult::kErrorFetchingConfigInvalidContentType: {
      return FederatedAuthRequestIssueReasonEnum::ConfigInvalidContentType;
    }
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound: {
      return FederatedAuthRequestIssueReasonEnum::ClientMetadataHttpNotFound;
    }
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse: {
      return FederatedAuthRequestIssueReasonEnum::ClientMetadataNoResponse;
    }
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse: {
      return FederatedAuthRequestIssueReasonEnum::ClientMetadataInvalidResponse;
    }
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidContentType: {
      return FederatedAuthRequestIssueReasonEnum::
          ClientMetadataInvalidContentType;
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound: {
      return FederatedAuthRequestIssueReasonEnum::AccountsHttpNotFound;
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse: {
      return FederatedAuthRequestIssueReasonEnum::AccountsNoResponse;
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse: {
      return FederatedAuthRequestIssueReasonEnum::AccountsInvalidResponse;
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty: {
      return FederatedAuthRequestIssueReasonEnum::AccountsListEmpty;
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidContentType: {
      return FederatedAuthRequestIssueReasonEnum::AccountsInvalidContentType;
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound: {
      return FederatedAuthRequestIssueReasonEnum::IdTokenHttpNotFound;
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse: {
      return FederatedAuthRequestIssueReasonEnum::IdTokenNoResponse;
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse: {
      return FederatedAuthRequestIssueReasonEnum::IdTokenInvalidResponse;
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidContentType: {
      return FederatedAuthRequestIssueReasonEnum::IdTokenInvalidContentType;
    }
    case FederatedAuthRequestResult::kErrorCanceled: {
      return FederatedAuthRequestIssueReasonEnum::Canceled;
    }
    case FederatedAuthRequestResult::kErrorRpPageNotVisible:
      return FederatedAuthRequestIssueReasonEnum::RpPageNotVisible;
    case FederatedAuthRequestResult::kError: {
      return FederatedAuthRequestIssueReasonEnum::ErrorIdToken;
    }
    case FederatedAuthRequestResult::kSuccess: {
      DCHECK(false);
      return "";
    }
  }
}

std::unique_ptr<protocol::Audits::InspectorIssue>
BuildFederatedAuthRequestIssue(
    const blink::mojom::FederatedAuthRequestIssueDetailsPtr& issue_details) {
  protocol::String type_string =
      FederatedAuthRequestResultToProtocol(issue_details->status);

  auto federated_auth_request_details =
      protocol::Audits::FederatedAuthRequestIssueDetails::Create()
          .SetFederatedAuthRequestIssueReason(type_string)
          .Build();

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetFederatedAuthRequestIssueDetails(
              std::move(federated_auth_request_details))
          .Build();

  auto issue = protocol::Audits::InspectorIssue::Create()
                   .SetCode(protocol::Audits::InspectorIssueCodeEnum::
                                FederatedAuthRequestIssue)
                   .SetDetails(std::move(protocol_issue_details))
                   .Build();
  return issue;
}

const char* DeprecationIssueTypeToProtocol(
    blink::mojom::DeprecationIssueType error_type) {
  switch (error_type) {
    case blink::mojom::DeprecationIssueType::kPrivacySandboxExtensionsAPI:
      return kPrivacySandboxExtensionsAPI;
  }
}

std::unique_ptr<protocol::Audits::InspectorIssue> BuildDeprecationIssue(
    const blink::mojom::DeprecationIssueDetailsPtr& issue_details) {
  std::unique_ptr<protocol::Audits::SourceCodeLocation> source_code_location =
      protocol::Audits::SourceCodeLocation::Create()
          .SetUrl(issue_details->affected_location->url.value())
          .SetLineNumber(issue_details->affected_location->line)
          .SetColumnNumber(issue_details->affected_location->column)
          .Build();

  if (issue_details->affected_location->script_id.has_value()) {
    source_code_location->SetScriptId(
        issue_details->affected_location->script_id.value());
  }

  auto deprecation_issue_details =
      protocol::Audits::DeprecationIssueDetails::Create()
          .SetSourceCodeLocation(std::move(source_code_location))
          .SetType(DeprecationIssueTypeToProtocol(issue_details->type))
          .Build();

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetDeprecationIssueDetails(std::move(deprecation_issue_details))
          .Build();

  auto deprecation_issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(protocol::Audits::InspectorIssueCodeEnum::DeprecationIssue)
          .SetDetails(std::move(protocol_issue_details))
          .Build();

  return deprecation_issue;
}

std::unique_ptr<protocol::Audits::InspectorIssue> BuildBounceTrackingIssue(
    const blink::mojom::BounceTrackingIssueDetailsPtr& issue_details) {
  auto bounce_tracking_issue_details =
      protocol::Audits::BounceTrackingIssueDetails::Create()
          .SetTrackingSites(std::make_unique<protocol::Array<protocol::String>>(
              issue_details->tracking_sites))
          .Build();

  auto protocol_issue_details =
      protocol::Audits::InspectorIssueDetails::Create()
          .SetBounceTrackingIssueDetails(
              std::move(bounce_tracking_issue_details))
          .Build();

  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(
              protocol::Audits::InspectorIssueCodeEnum::BounceTrackingIssue)
          .SetDetails(std::move(protocol_issue_details))
          .Build();

  return issue;
}

void UpdateChildFrameTrees(FrameTreeNode* ftn, bool update_target_info) {
  if (auto* agent_host = WebContentsDevToolsAgentHost::GetFor(
          WebContentsImpl::FromFrameTreeNode(ftn))) {
    agent_host->UpdateChildFrameTrees(update_target_info);
  }
}

}  // namespace

void OnResetNavigationRequest(NavigationRequest* navigation_request) {
  // Traverse frame chain all the way to the top and report to all
  // page handlers that the navigation completed.
  for (FrameTreeNode* node = navigation_request->frame_tree_node(); node;
       node = FrameTreeNode::From(node->parent())) {
    DispatchToAgents(node, &protocol::PageHandler::NavigationReset,
                     navigation_request);
  }
}

void OnNavigationResponseReceived(const NavigationRequest& nav_request,
                                  const network::mojom::URLResponseHead& head) {
  // This response is artificial (see CachedNavigationURLLoader), so we don't
  // want to report it.
  if (nav_request.IsPageActivation())
    return;

  FrameTreeNode* ftn = nav_request.frame_tree_node();
  std::string id = nav_request.devtools_navigation_token().ToString();
  std::string frame_id =
      ftn->current_frame_host()->devtools_frame_token().ToString();
  GURL url = nav_request.common_params().url;

  network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
      network::ExtractDevToolsInfo(head);
  DispatchToAgents(ftn, &protocol::NetworkHandler::ResponseReceived, id, id,
                   url, protocol::Network::ResourceTypeEnum::Document,
                   *head_info, frame_id);
}

void BackForwardCacheNotUsed(
    const NavigationRequest* nav_request,
    const BackForwardCacheCanStoreDocumentResult* result,
    const BackForwardCacheCanStoreTreeResult* tree_result) {
  DCHECK(nav_request);
  FrameTreeNode* ftn = nav_request->frame_tree_node();
  DispatchToAgents(ftn, &protocol::PageHandler::BackForwardCacheNotUsed,
                   nav_request, result, tree_result);
}

void WillSwapFrameTreeNode(FrameTreeNode& old_node, FrameTreeNode& new_node) {
  auto* host = static_cast<RenderFrameDevToolsAgentHost*>(
      RenderFrameDevToolsAgentHost::GetFor(&old_node));
  if (!host || host->HasSessionsWithoutTabTargetSupport())
    return;
  // The new node may have a previous host associated, disconnect it first.
  scoped_refptr<RenderFrameDevToolsAgentHost> previous_host =
      static_cast<RenderFrameDevToolsAgentHost*>(
          RenderFrameDevToolsAgentHost::GetFor(&new_node));
  previous_host->SetFrameTreeNode(nullptr);
  host->SetFrameTreeNode(&new_node);
}

void OnFrameTreeNodeDestroyed(FrameTreeNode& frame_tree_node) {
  // If the child frame is an OOPIF, we emit Page.frameDetached event which
  // otherwise might be lost because the OOPIF target is being destroyed.
  content::RenderFrameHostImpl* parent = frame_tree_node.parent();
  if (!parent)
    return;
  if (RenderFrameDevToolsAgentHost::GetFor(&frame_tree_node) !=
      RenderFrameDevToolsAgentHost::GetFor(parent)) {
    DispatchToAgents(
        RenderFrameDevToolsAgentHost::GetFor(parent),
        &protocol::PageHandler::OnFrameDetached,
        frame_tree_node.current_frame_host()->devtools_frame_token());
  }
}

void WillInitiatePrerender(FrameTree& frame_tree) {
  DCHECK(frame_tree.is_prerendering());
  auto* wc = WebContentsImpl::FromFrameTreeNode(frame_tree.root());
  if (auto* host = WebContentsDevToolsAgentHost::GetFor(wc))
    host->WillInitiatePrerender(frame_tree.root());
}

void DidActivatePrerender(
    const NavigationRequest& nav_request,
    const base::UnguessableToken& initiator_devtools_navigation_token) {
  FrameTreeNode* ftn = nav_request.frame_tree_node();
  WebContentsImpl* web_contents = WebContentsImpl::FromFrameTreeNode(ftn);
  // Record prerender activation here because users don't necessarily open
  // DevTools when the activation is triggered. If the DevTools is not opened at
  // the moment, recording the activation here will still preserve the signal.
  web_contents->set_last_navigation_was_prerender_activation_for_devtools();
  DispatchToAgents(ftn, &protocol::PreloadHandler::DidActivatePrerender,
                   initiator_devtools_navigation_token, nav_request);
  UpdateChildFrameTrees(ftn, /* update_target_info= */ true);
}

void DidCancelPrerender(
    FrameTreeNode* ftn,
    const GURL& prerendering_url,
    const base::UnguessableToken& initiator_devtools_navigation_token,
    PrerenderFinalStatus status,
    const std::string& disallowed_api_method) {
  if (!ftn) {
    return;
  }

  std::string initiating_frame_id =
      ftn->current_frame_host()->devtools_frame_token().ToString();
  DispatchToAgents(ftn, &protocol::PreloadHandler::DidCancelPrerender,
                   prerendering_url, initiator_devtools_navigation_token,
                   initiating_frame_id, status, disallowed_api_method);
}

void DidUpdatePrefetchStatus(
    FrameTreeNode* ftn,
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const GURL& prefetch_url,
    PreloadingTriggeringOutcome status) {
  if (!ftn) {
    return;
  }

  std::string initiating_frame_id =
      ftn->current_frame_host()->devtools_frame_token().ToString();
  DispatchToAgents(ftn, &protocol::PreloadHandler::DidUpdatePrefetchStatus,
                   initiator_devtools_navigation_token, initiating_frame_id,
                   prefetch_url, status);
}

void DidUpdatePrerenderStatus(
    int initiator_frame_tree_node_id,
    const base::UnguessableToken& initiator_devtools_navigation_token,
    const GURL& prerender_url,
    PreloadingTriggeringOutcome status) {
  auto* ftn = FrameTreeNode::GloballyFindByID(initiator_frame_tree_node_id);
  // ftn will be null if this is browser-initiated, which has no initiator.
  if (ftn) {
    std::string initiating_frame_id =
        ftn->current_frame_host()->devtools_frame_token().ToString();
    DispatchToAgents(ftn, &protocol::PreloadHandler::DidUpdatePrerenderStatus,
                     initiator_devtools_navigation_token, initiating_frame_id,
                     prerender_url, status);
  }
}

namespace {

protocol::String BuildBlockedByResponseReason(
    network::mojom::BlockedByResponseReason reason) {
  switch (reason) {
    case network::mojom::BlockedByResponseReason::
        kCoepFrameResourceNeedsCoepHeader:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CoepFrameResourceNeedsCoepHeader;
    case network::mojom::BlockedByResponseReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CoopSandboxedIFrameCannotNavigateToCoopPage;
    case network::mojom::BlockedByResponseReason::kCorpNotSameOrigin:
      return protocol::Audits::BlockedByResponseReasonEnum::CorpNotSameOrigin;
    case network::mojom::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoep:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByCoep;
    case network::mojom::BlockedByResponseReason::kCorpNotSameSite:
      return protocol::Audits::BlockedByResponseReasonEnum::CorpNotSameSite;
  }
}

void ReportBlockedByResponseIssue(
    const GURL& url,
    std::string& requestId,
    FrameTreeNode* ftn,
    RenderFrameHostImpl* parent_frame,
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(status.blocked_by_response_reason);

  auto issueDetails = protocol::Audits::InspectorIssueDetails::Create();
  auto request = protocol::Audits::AffectedRequest::Create()
                     .SetRequestId(requestId)
                     .SetUrl(url.spec())
                     .Build();
  auto blockedByResponseDetails =
      protocol::Audits::BlockedByResponseIssueDetails::Create()
          .SetRequest(std::move(request))
          .SetReason(
              BuildBlockedByResponseReason(*status.blocked_by_response_reason))
          .Build();

  blockedByResponseDetails->SetBlockedFrame(
      protocol::Audits::AffectedFrame::Create()
          .SetFrameId(
              ftn->current_frame_host()->devtools_frame_token().ToString())
          .Build());
  if (parent_frame) {
    blockedByResponseDetails->SetParentFrame(
        protocol::Audits::AffectedFrame::Create()
            .SetFrameId(parent_frame->devtools_frame_token().ToString())
            .Build());
  }

  issueDetails.SetBlockedByResponseIssueDetails(
      std::move(blockedByResponseDetails));

  auto inspector_issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(
              protocol::Audits::InspectorIssueCodeEnum::BlockedByResponseIssue)
          .SetDetails(issueDetails.Build())
          .Build();

  ReportBrowserInitiatedIssue(ftn->current_frame_host(), inspector_issue.get());
}

}  // namespace

void OnNavigationRequestFailed(
    const NavigationRequest& nav_request,
    const network::URLLoaderCompletionStatus& status) {
  FrameTreeNode* ftn = nav_request.frame_tree_node();
  std::string id = nav_request.devtools_navigation_token().ToString();

  if (status.blocked_by_response_reason) {
    ReportBlockedByResponseIssue(
        const_cast<NavigationRequest&>(nav_request).GetURL(), id, ftn,
        ftn->parent(), status);
  }

  // If a BFCache navigation fails, it will be restarted as a regular
  // navigation, so we don't want to report this failure.
  if (nav_request.IsServedFromBackForwardCache())
    return;

  // Activation of a prerender page is synchronous with its own activation flow
  // (crrev.com/c/2992411); if the prerender is cancelled (e.g. speculation rule
  // removed), the flow will fallback to a normal navigation, which is no longer
  // considered as a page activation.
  DCHECK(!nav_request.IsPageActivation());

  DispatchToAgents(ftn, &protocol::NetworkHandler::LoadingComplete, id,
                   protocol::Network::ResourceTypeEnum::Document, status);
}

bool ShouldBypassCSP(const NavigationRequest& nav_request) {
  DevToolsAgentHostImpl* agent_host =
      RenderFrameDevToolsAgentHost::GetFor(nav_request.frame_tree_node());
  if (!agent_host)
    return false;

  for (auto* page : protocol::PageHandler::ForAgentHost(agent_host)) {
    if (page->ShouldBypassCSP())
      return true;
  }
  return false;
}

void WillBeginDownload(download::DownloadCreateInfo* info,
                       download::DownloadItem* item) {
  if (!item)
    return;
  auto* rfh = static_cast<RenderFrameHostImpl*>(
      RenderFrameHost::FromID(info->render_process_id, info->render_frame_id));
  FrameTreeNode* ftn =
      rfh ? FrameTreeNode::GloballyFindByID(rfh->GetFrameTreeNodeId())
          : nullptr;
  if (!ftn)
    return;
  DispatchToAgents(ftn, &protocol::BrowserHandler::DownloadWillBegin, ftn,
                   item);
  DispatchToAgents(ftn, &protocol::PageHandler::DownloadWillBegin, ftn, item);

  for (auto* agent_host : BrowserDevToolsAgentHost::Instances()) {
    for (auto* browser_handler :
         protocol::BrowserHandler::ForAgentHost(agent_host)) {
      browser_handler->DownloadWillBegin(ftn, item);
    }
  }
}

void OnSignedExchangeReceived(
    FrameTreeNode* frame_tree_node,
    absl::optional<const base::UnguessableToken> devtools_navigation_token,
    const GURL& outer_request_url,
    const network::mojom::URLResponseHead& outer_response,
    const absl::optional<SignedExchangeEnvelope>& envelope,
    const scoped_refptr<net::X509Certificate>& certificate,
    const absl::optional<net::SSLInfo>& ssl_info,
    const std::vector<SignedExchangeError>& errors) {
  DispatchToAgents(frame_tree_node,
                   &protocol::NetworkHandler::OnSignedExchangeReceived,
                   devtools_navigation_token, outer_request_url, outer_response,
                   envelope, certificate, ssl_info, errors);
}

namespace inspector_will_send_navigation_request_event {
std::unique_ptr<base::trace_event::TracedValue> Data(
    const base::UnguessableToken& request_id) {
  auto value = std::make_unique<base::trace_event::TracedValue>();
  value->SetString("requestId", request_id.ToString());
  return value;
}
}  // namespace inspector_will_send_navigation_request_event

void OnSignedExchangeCertificateRequestSent(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const network::ResourceRequest& request,
    const GURL& signed_exchange_url) {
  // Make sure both back-ends yield the same timestamp.
  auto timestamp = base::TimeTicks::Now();
  network::mojom::URLRequestDevToolsInfoPtr request_info =
      network::ExtractDevToolsInfo(request);
  DispatchToAgents(
      frame_tree_node, &protocol::NetworkHandler::RequestSent,
      request_id.ToString(), loader_id.ToString(), request.headers,
      *request_info, protocol::Network::Initiator::TypeEnum::SignedExchange,
      signed_exchange_url, /*initiator_devtools_request_id=*/"", timestamp);

  auto value = std::make_unique<base::trace_event::TracedValue>();
  value->SetString("requestId", request_id.ToString());
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "devtools.timeline", "ResourceWillSendRequest", TRACE_EVENT_SCOPE_PROCESS,
      timestamp, "data",
      inspector_will_send_navigation_request_event::Data(request_id));
}

void OnSignedExchangeCertificateResponseReceived(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const base::UnguessableToken& loader_id,
    const GURL& url,
    const network::mojom::URLResponseHead& head) {
  network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
      network::ExtractDevToolsInfo(head);
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::ResponseReceived,
                   request_id.ToString(), loader_id.ToString(), url,
                   protocol::Network::ResourceTypeEnum::Other, *head_info,
                   protocol::Maybe<std::string>());
}

void OnSignedExchangeCertificateRequestCompleted(
    FrameTreeNode* frame_tree_node,
    const base::UnguessableToken& request_id,
    const network::URLLoaderCompletionStatus& status) {
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::LoadingComplete,
                   request_id.ToString(),
                   protocol::Network::ResourceTypeEnum::Other, status);
}

void ThrottleForServiceWorkerAgentHost(
    ServiceWorkerDevToolsAgentHost* agent_host,
    DevToolsAgentHostImpl* requesting_agent_host,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle) {
  for (auto* target_handler :
       protocol::TargetHandler::ForAgentHost(requesting_agent_host)) {
    target_handler->AddWorkerThrottle(agent_host, throttle_handle);
  }
}

std::vector<std::unique_ptr<NavigationThrottle>> CreateNavigationThrottles(
    NavigationHandle* navigation_handle) {
  FrameTreeNode* frame_tree_node =
      NavigationRequest::From(navigation_handle)->frame_tree_node();
  FrameTreeNode* parent = FrameTreeNode::From(frame_tree_node->parent());

  std::vector<std::unique_ptr<NavigationThrottle>> result;

  if (!parent) {
    FrameTreeNode* outer_delegate_node =
        frame_tree_node->render_manager()->GetOuterDelegateNode();
    if (outer_delegate_node &&
        (WebContentsImpl::FromFrameTreeNode(frame_tree_node)->IsPortal() ||
         frame_tree_node->IsFencedFrameRoot())) {
      parent = outer_delegate_node->parent()->frame_tree_node();
    } else if (frame_tree_node->GetFrameType() ==
                   FrameType::kPrerenderMainFrame &&
               !frame_tree_node->current_frame_host()
                    ->has_committed_any_navigation()) {
      if (auto* agent_host = WebContentsDevToolsAgentHost::GetFor(
              WebContentsImpl::FromFrameTreeNode(frame_tree_node))) {
        // For prerender, perform auto-attach to tab target at the point of
        // initial navigation.
        agent_host->auto_attacher()->AppendNavigationThrottles(
            navigation_handle, &result);
        return result;
      }
    }
  }

  if (parent) {
    if (auto* agent_host = RenderFrameDevToolsAgentHost::GetFor(parent)) {
      agent_host->auto_attacher()->AppendNavigationThrottles(navigation_handle,
                                                             &result);
    }
  } else {
    for (DevToolsAgentHostImpl* host : BrowserDevToolsAgentHost::Instances()) {
      host->auto_attacher()->AppendNavigationThrottles(navigation_handle,
                                                       &result);
    }
  }

  return result;
}

void ThrottleServiceWorkerMainScriptFetch(
    ServiceWorkerContextWrapper* wrapper,
    int64_t version_id,
    const GlobalRenderFrameHostId& requesting_frame_id,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle) {
  ServiceWorkerDevToolsAgentHost* agent_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForNewInstallingWorker(wrapper, version_id);
  DCHECK(agent_host);

  // TODO(ahemery): We should probably also add the possibility for Browser wide
  // agents to throttle the request.

  // If we have a requesting_frame_id, we should have a frame and a frame tree
  // node. However since the lifetime of these objects can be complex, we check
  // at each step that we indeed can go reach all the way to the FrameTreeNode.
  if (!requesting_frame_id)
    return;

  RenderFrameHostImpl* requesting_frame =
      RenderFrameHostImpl::FromID(requesting_frame_id);
  if (!requesting_frame)
    return;

  FrameTreeNode* ftn = requesting_frame->frame_tree_node();
  DCHECK(ftn);

  DevToolsAgentHostImpl* requesting_agent_host =
      RenderFrameDevToolsAgentHost::GetFor(ftn);
  if (!requesting_agent_host)
    return;

  ThrottleForServiceWorkerAgentHost(agent_host, requesting_agent_host,
                                    throttle_handle);
}

void ThrottleWorkerMainScriptFetch(
    const base::UnguessableToken& devtools_worker_token,
    const GlobalRenderFrameHostId& ancestor_render_frame_host_id,
    scoped_refptr<DevToolsThrottleHandle> throttle_handle) {
  WorkerDevToolsAgentHost* agent_host =
      WorkerDevToolsManager::GetInstance().GetDevToolsHostFromToken(
          devtools_worker_token);
  if (!agent_host)
    return;

  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(ancestor_render_frame_host_id);
  if (!rfh)
    return;

  FrameTreeNode* ftn = rfh->frame_tree_node();
  DispatchToAgents(ftn, &protocol::TargetHandler::AddWorkerThrottle, agent_host,
                   std::move(throttle_handle));
}

bool ShouldWaitForDebuggerInWindowOpen() {
  for (auto* browser_agent_host : BrowserDevToolsAgentHost::Instances()) {
    for (auto* target_handler :
         protocol::TargetHandler::ForAgentHost(browser_agent_host)) {
      if (target_handler->ShouldThrottlePopups())
        return true;
    }
  }
  return false;
}

void ApplyNetworkRequestOverrides(
    FrameTreeNode* frame_tree_node,
    blink::mojom::BeginNavigationParams* begin_params,
    bool* report_raw_headers,
    absl::optional<std::vector<net::SourceStream::SourceType>>*
        devtools_accepted_stream_types,
    bool* devtools_user_agent_overridden,
    bool* devtools_accept_language_overridden) {
  *devtools_user_agent_overridden = false;
  *devtools_accept_language_overridden = false;
  bool disable_cache = false;
  DevToolsAgentHostImpl* agent_host =
      RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
  // Prerendered pages will only have have DevTools attached if the client opted
  // into supporting the tab target. For legacy clients, we will apply relevant
  // network override from the associated main frame target.
  if (frame_tree_node->frame_tree().is_prerendering()) {
    if (!agent_host) {
      agent_host = RenderFrameDevToolsAgentHost::GetFor(
          WebContentsImpl::FromFrameTreeNode(frame_tree_node)
              ->GetPrimaryMainFrame()
              ->frame_tree_node());
    }
  }
  if (!agent_host)
    return;
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params->headers);
  for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host)) {
    if (!network->enabled())
      continue;
    *report_raw_headers = true;
    network->ApplyOverrides(&headers, &begin_params->skip_service_worker,
                            &disable_cache, devtools_accepted_stream_types);
  }

  for (auto* emulation : protocol::EmulationHandler::ForAgentHost(agent_host)) {
    bool ua_overridden = false;
    bool accept_language_overridden = false;
    emulation->ApplyOverrides(&headers, &ua_overridden,
                              &accept_language_overridden);
    *devtools_user_agent_overridden |= ua_overridden;
    *devtools_accept_language_overridden |= accept_language_overridden;
  }

  if (disable_cache) {
    begin_params->load_flags &=
        ~(net::LOAD_VALIDATE_CACHE | net::LOAD_SKIP_CACHE_VALIDATION |
          net::LOAD_ONLY_FROM_CACHE | net::LOAD_DISABLE_CACHE);
    begin_params->load_flags |= net::LOAD_BYPASS_CACHE;
  }

  begin_params->headers = headers.ToString();
}

bool ApplyUserAgentMetadataOverrides(
    FrameTreeNode* frame_tree_node,
    absl::optional<blink::UserAgentMetadata>* override_out) {
  DevToolsAgentHostImpl* agent_host =
      RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
  // If DevToolsTabTarget is not enabled, Prerendered pages do not have DevTools
  // attached but it's important for developers that they get the UA override of
  // the visible DevTools for testing mobile sites. Use the DevTools agent of
  // the primary main frame of the WebContents.
  // TODO(https://crbug.com/1221419): The real fix may be to make a separate
  // target for the prerendered page.
  if (frame_tree_node->frame_tree().is_prerendering() && !agent_host) {
    agent_host = RenderFrameDevToolsAgentHost::GetFor(
        WebContentsImpl::FromFrameTreeNode(frame_tree_node)
            ->GetPrimaryMainFrame()
            ->frame_tree_node());
  }
  if (!agent_host)
    return false;

  bool result = false;
  for (auto* emulation : protocol::EmulationHandler::ForAgentHost(agent_host))
    result = emulation->ApplyUserAgentMetadataOverrides(override_out) || result;

  return result;
}

namespace {
template <typename HandlerType>
bool MaybeCreateProxyForInterception(
    DevToolsAgentHostImpl* agent_host,
    int process_id,
    StoragePartition* storage_partition,
    const base::UnguessableToken& frame_token,
    bool is_navigation,
    bool is_download,
    network::mojom::URLLoaderFactoryOverride* agent_override) {
  if (!agent_host)
    return false;
  bool had_interceptors = false;
  const auto& handlers = HandlerType::ForAgentHost(agent_host);
  for (const auto& handler : base::Reversed(handlers)) {
    had_interceptors = handler->MaybeCreateProxyForInterception(
                           process_id, storage_partition, frame_token,
                           is_navigation, is_download, agent_override) ||
                       had_interceptors;
  }
  return had_interceptors;
}

}  // namespace

bool WillCreateURLLoaderFactory(
    RenderFrameHostImpl* rfh,
    bool is_navigation,
    bool is_download,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
        target_factory_receiver,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  DCHECK(!is_download || is_navigation);

  RenderProcessHost* rph = rfh->GetProcess();
  DCHECK(rph);

  DevToolsAgentHostImpl* frame_agent_host =
      RenderFrameDevToolsAgentHost::GetFor(rfh);

  return WillCreateURLLoaderFactoryInternal(
      frame_agent_host, rfh->GetDevToolsFrameToken(), rph->GetID(),
      rph->GetStoragePartition(), is_navigation, is_download,
      target_factory_receiver, factory_override);
}

bool WillCreateURLLoaderFactoryInternal(
    DevToolsAgentHostImpl* agent_host,
    const base::UnguessableToken& devtools_token,
    int process_id,
    StoragePartition* storage_partition,
    bool is_navigation,
    bool is_download,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
        target_factory_receiver,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  DCHECK(!is_download || is_navigation);

  network::mojom::URLLoaderFactoryOverride devtools_override;
  // If caller passed some existing overrides, use those.
  // Otherwise, use our local var, then if handlers actually
  // decide to intercept, move it to |factory_override|.
  network::mojom::URLLoaderFactoryOverride* handler_override =
      factory_override && *factory_override ? factory_override->get()
                                            : &devtools_override;

  // Order of targets and sessions matters -- the latter proxy is created,
  // the closer it is to the network. So start with frame's NetworkHandler,
  // then process frame's FetchHandler and then browser's FetchHandler.
  // Within the target, the agents added earlier are closer to network.
  bool had_interceptors =
      MaybeCreateProxyForInterception<protocol::NetworkHandler>(
          agent_host, process_id, storage_partition, devtools_token,
          is_navigation, is_download, handler_override);

  had_interceptors =
      MaybeCreateProxyForInterception<protocol::FetchHandler>(
          agent_host, process_id, storage_partition, devtools_token,
          is_navigation, is_download, handler_override) ||
      had_interceptors;

  // TODO(caseq): assure deterministic order of browser agents (or sessions).
  for (auto* browser_agent_host : BrowserDevToolsAgentHost::Instances()) {
    had_interceptors =
        MaybeCreateProxyForInterception<protocol::FetchHandler>(
            browser_agent_host, process_id, storage_partition, devtools_token,
            is_navigation, is_download, handler_override) ||
        had_interceptors;
  }
  if (!had_interceptors)
    return false;
  DCHECK(handler_override->overriding_factory);
  DCHECK(handler_override->overridden_factory_receiver);
  if (!factory_override) {
    // Not a subresource navigation, so just override the target receiver.
    mojo::FusePipes(std::move(*target_factory_receiver),
                    std::move(devtools_override.overriding_factory));
    *target_factory_receiver =
        std::move(devtools_override.overridden_factory_receiver);
  } else if (!*factory_override) {
    // No other overrides, so just returns ours as is.
    *factory_override = network::mojom::URLLoaderFactoryOverride::New(
        std::move(devtools_override.overriding_factory),
        std::move(devtools_override.overridden_factory_receiver), false);
  }
  // ... else things are already taken care of, as handler_override was pointing
  // to factory override and we've done all magic in-place.
  DCHECK(!devtools_override.overriding_factory);
  DCHECK(!devtools_override.overridden_factory_receiver);

  return true;
}

bool WillCreateURLLoaderFactoryForServiceWorker(
    RenderProcessHost* rph,
    int routing_id,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  DCHECK(rph);
  DCHECK(factory_override);

  ServiceWorkerDevToolsAgentHost* worker_agent_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(rph->GetID(), routing_id);
  DCHECK(worker_agent_host);

  return WillCreateURLLoaderFactoryInternal(
      worker_agent_host, worker_agent_host->devtools_worker_token(),
      rph->GetID(), rph->GetStoragePartition(),
      /*is_navigation=*/false, /*is_download=*/false,
      /*target_factory_receiver=*/nullptr, factory_override);
}

bool WillCreateURLLoaderFactoryForServiceWorkerMainScript(
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
        target_factory_receiver) {
  ServiceWorkerDevToolsAgentHost* worker_agent_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForNewInstallingWorker(context_wrapper,
                                                       version_id);
  DCHECK(worker_agent_host);

  return WillCreateURLLoaderFactoryInternal(
      worker_agent_host, worker_agent_host->devtools_worker_token(),
      ChildProcessHost::kInvalidUniqueID, context_wrapper->storage_partition(),
      /*is_navigation=*/true,
      /*is_download=*/false, target_factory_receiver,
      /*factory_override=*/nullptr);
}

bool WillCreateURLLoaderFactoryForSharedWorker(
    SharedWorkerHost* host,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  auto* worker_agent_host = SharedWorkerDevToolsAgentHost::GetFor(host);
  if (!worker_agent_host)
    return false;

  RenderProcessHost* rph = worker_agent_host->GetProcessHost();
  DCHECK(rph);

  return WillCreateURLLoaderFactoryInternal(
      worker_agent_host, worker_agent_host->devtools_worker_token(),
      rph->GetID(), rph->GetStoragePartition(),
      /*is_navigation=*/false, /*is_download=*/false,
      /*target_factory_receiver=*/nullptr, factory_override);
}

bool WillCreateURLLoaderFactoryForWorkerMainScript(
    DevToolsAgentHostImpl* host,
    const base::UnguessableToken& worker_token,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  RenderProcessHost* rph = host->GetProcessHost();
  DCHECK(rph);

  return WillCreateURLLoaderFactoryInternal(
      host, worker_token, rph->GetID(), rph->GetStoragePartition(),
      /*is_navigation=*/false, /*is_download=*/false,
      /*target_factory_receiver=*/nullptr, factory_override);
}

bool WillCreateURLLoaderFactory(
    RenderFrameHostImpl* rfh,
    bool is_navigation,
    bool is_download,
    std::unique_ptr<network::mojom::URLLoaderFactory>* factory) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> proxied_factory;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
      proxied_factory.InitWithNewPipeAndPassReceiver();
  if (!WillCreateURLLoaderFactory(rfh, is_navigation, is_download, &receiver,
                                  nullptr)) {
    return false;
  }
  mojo::MakeSelfOwnedReceiver(std::move(*factory), std::move(receiver));
  *factory = std::make_unique<DevToolsURLLoaderFactoryAdapter>(
      std::move(proxied_factory));
  return true;
}

void OnPrefetchRequestWillBeSent(FrameTreeNode* frame_tree_node,
                                 const std::string& request_id,
                                 const GURL& initiator,
                                 const network::ResourceRequest& request) {
  auto timestamp = base::TimeTicks::Now();
  std::string frame_token =
      frame_tree_node->current_frame_host()->devtools_frame_token().ToString();
  DispatchToAgents(frame_tree_node,
                   &protocol::NetworkHandler::PrefetchRequestWillBeSent,
                   request_id, request, initiator, frame_token, timestamp);
}

void OnPrefetchResponseReceived(FrameTreeNode* frame_tree_node,
                                const std::string& request_id,
                                const GURL& url,
                                const network::mojom::URLResponseHead& head) {
  std::string frame_token =
      frame_tree_node->current_frame_host()->devtools_frame_token().ToString();

  network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
      network::ExtractDevToolsInfo(head);
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::ResponseReceived,
                   request_id, request_id, url,
                   protocol::Network::ResourceTypeEnum::Prefetch, *head_info,
                   frame_token);
}
void OnPrefetchRequestComplete(
    FrameTreeNode* frame_tree_node,
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::LoadingComplete,
                   request_id, protocol::Network::ResourceTypeEnum::Prefetch,
                   status);
}
void OnPrefetchBodyDataReceived(FrameTreeNode* frame_tree_node,
                                const std::string& request_id,
                                const std::string& body,
                                bool is_base64_encoded) {
  DispatchToAgents(frame_tree_node, &protocol::NetworkHandler::BodyDataReceived,
                   request_id, body, is_base64_encoded);
}

void OnNavigationRequestWillBeSent(
    const NavigationRequest& navigation_request) {
  // Note this intentionally deviates from the usual instrumentation signal
  // logic and dispatches to all agents upwards from the frame, to make sure
  // the security checks are properly applied even if no DevTools session is
  // established for the navigated frame itself. This is because the page
  // agent may navigate all of its subframes currently.
  for (RenderFrameHostImpl* rfh =
           navigation_request.frame_tree_node()->current_frame_host();
       rfh; rfh = rfh->GetParentOrOuterDocument()) {
    // Only check frames that qualify as DevTools targets, i.e. (local)? roots.
    if (!RenderFrameDevToolsAgentHost::ShouldCreateDevToolsForHost(rfh))
      continue;
    auto* agent_host = static_cast<RenderFrameDevToolsAgentHost*>(
        RenderFrameDevToolsAgentHost::GetFor(rfh));
    if (!agent_host)
      continue;
    agent_host->OnNavigationRequestWillBeSent(navigation_request);
  }

  // We use CachedNavigationURLLoader for page activation (BFCache navigations
  // and Prerender activations) and don't actually send a network request, so we
  // don't report this request to DevTools.
  if (navigation_request.IsPageActivation())
    return;

  // Make sure both back-ends yield the same timestamp.
  auto timestamp = base::TimeTicks::Now();
  DispatchToAgents(navigation_request.frame_tree_node(),
                   &protocol::NetworkHandler::NavigationRequestWillBeSent,
                   navigation_request, timestamp);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "devtools.timeline", "ResourceWillSendRequest", TRACE_EVENT_SCOPE_PROCESS,
      timestamp, "data",
      inspector_will_send_navigation_request_event::Data(
          navigation_request.devtools_navigation_token()));
}

// Notify the provided agent host of a certificate error. Returns true if one of
// the host's handlers will handle the certificate error.
bool NotifyCertificateError(DevToolsAgentHost* host,
                            int cert_error,
                            const GURL& request_url,
                            const CertErrorCallback& callback) {
  DevToolsAgentHostImpl* host_impl = static_cast<DevToolsAgentHostImpl*>(host);
  for (auto* security_handler :
       protocol::SecurityHandler::ForAgentHost(host_impl)) {
    if (security_handler->NotifyCertificateError(cert_error, request_url,
                                                 callback)) {
      return true;
    }
  }
  return false;
}

bool HandleCertificateError(WebContents* web_contents,
                            int cert_error,
                            const GURL& request_url,
                            CertErrorCallback callback) {
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetOrCreateFor(web_contents).get();
  if (NotifyCertificateError(agent_host.get(), cert_error, request_url,
                             callback)) {
    // Only allow a single agent host to handle the error.
    callback.Reset();
  }

  for (auto* browser_agent_host : BrowserDevToolsAgentHost::Instances()) {
    if (NotifyCertificateError(browser_agent_host, cert_error, request_url,
                               callback)) {
      // Only allow a single agent host to handle the error.
      callback.Reset();
    }
  }
  return !callback;
}

namespace {
void UpdatePortals(RenderFrameHostImpl* render_frame_host_impl) {
  if (auto* agent_host = static_cast<RenderFrameDevToolsAgentHost*>(
          RenderFrameDevToolsAgentHost::GetFor(
              render_frame_host_impl->frame_tree_node()))) {
    agent_host->UpdatePortals();
  }
  UpdateChildFrameTrees(render_frame_host_impl->frame_tree_node(),
                        /* update_target_info= */ false);
}
}  // namespace

void PortalAttached(RenderFrameHostImpl* render_frame_host_impl) {
  UpdatePortals(render_frame_host_impl);
}

void PortalDetached(RenderFrameHostImpl* render_frame_host_impl) {
  UpdatePortals(render_frame_host_impl);
}

void PortalActivated(Portal& portal) {
  WebContents* host_contents = portal.GetPortalHostContents();
  UpdatePortals(reinterpret_cast<RenderFrameHostImpl*>(
      host_contents->GetPrimaryMainFrame()));
  if (auto* host = WebContentsDevToolsAgentHost::GetFor(host_contents))
    host->PortalActivated(portal);
}

void FencedFrameCreated(
    base::SafeRef<RenderFrameHostImpl> owner_render_frame_host,
    FencedFrame* fenced_frame) {
  auto* agent_host = static_cast<RenderFrameDevToolsAgentHost*>(
      RenderFrameDevToolsAgentHost::GetFor(
          owner_render_frame_host->frame_tree_node()));
  if (!agent_host)
    return;
  agent_host->DidCreateFencedFrame(fenced_frame);
}

void DidCreateProcessForAuctionWorklet(RenderFrameHostImpl* owner,
                                       base::ProcessId pid) {
  // TracingHandler lives on the very root, not local root.
  // TODO(morlovich): This may not be right for fenced frames, though
  // that should not currently matter.
  FrameTreeNode* node = owner->GetMainFrame()->frame_tree_node();
  if (!node)
    return;
  DispatchToAgents(node, &protocol::TracingHandler::AddProcess, pid);
}

void WillStartDragging(FrameTreeNode* main_frame_tree_node,
                       const blink::mojom::DragDataPtr drag_data,
                       blink::DragOperationsMask drag_operations_mask,
                       bool* intercepted) {
  DCHECK(main_frame_tree_node->frame_tree().root() == main_frame_tree_node);
  DispatchToAgents(main_frame_tree_node, &protocol::InputHandler::StartDragging,
                   *drag_data, drag_operations_mask, intercepted);
}

void DragEnded(FrameTreeNode& node) {
  DCHECK(node.frame_tree().root() == &node);
  DispatchToAgents(&node, &protocol::InputHandler::DragEnded);
}

namespace {
std::unique_ptr<protocol::Array<protocol::String>> BuildExclusionReasons(
    net::CookieInclusionStatus status) {
  auto exclusion_reasons =
      std::make_unique<protocol::Array<protocol::String>>();
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::
              EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX)) {
    exclusion_reasons->push_back(protocol::Audits::CookieExclusionReasonEnum::
                                     ExcludeSameSiteUnspecifiedTreatedAsLax);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE)) {
    exclusion_reasons->push_back(protocol::Audits::CookieExclusionReasonEnum::
                                     ExcludeSameSiteNoneInsecure);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)) {
    exclusion_reasons->push_back(
        protocol::Audits::CookieExclusionReasonEnum::ExcludeSameSiteLax);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)) {
    exclusion_reasons->push_back(
        protocol::Audits::CookieExclusionReasonEnum::ExcludeSameSiteStrict);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_INVALID_SAMEPARTY)) {
    exclusion_reasons->push_back(
        protocol::Audits::CookieExclusionReasonEnum::ExcludeInvalidSameParty);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT)) {
    exclusion_reasons->push_back(protocol::Audits::CookieExclusionReasonEnum::
                                     ExcludeSamePartyCrossPartyContext);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_DOMAIN_NON_ASCII)) {
    exclusion_reasons->push_back(
        protocol::Audits::CookieExclusionReasonEnum::ExcludeDomainNonASCII);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::
              EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET)) {
    exclusion_reasons->push_back(
        protocol::Audits::CookieExclusionReasonEnum::
            ExcludeThirdPartyCookieBlockedInFirstPartySet);
  }

  return exclusion_reasons;
}

std::unique_ptr<protocol::Array<protocol::String>> BuildWarningReasons(
    net::CookieInclusionStatus status) {
  auto warning_reasons = std::make_unique<protocol::Array<protocol::String>>();
  if (status.HasWarningReason(
          net::CookieInclusionStatus::WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnAttributeValueExceedsMaxSize);
  }
  if (status.HasWarningReason(
          net::CookieInclusionStatus::
              WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteUnspecifiedCrossSiteContext);
  }
  if (status.HasWarningReason(
          net::CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE)) {
    warning_reasons->push_back(
        protocol::Audits::CookieWarningReasonEnum::WarnSameSiteNoneInsecure);
  }
  if (status.HasWarningReason(net::CookieInclusionStatus::
                                  WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteUnspecifiedLaxAllowUnsafe);
  }

  // There can only be one of the following warnings.
  if (status.HasWarningReason(net::CookieInclusionStatus::
                                  WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteStrictLaxDowngradeStrict);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteStrictCrossDowngradeStrict);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteStrictCrossDowngradeLax);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteLaxCrossDowngradeStrict);
  } else if (status.HasWarningReason(
                 net::CookieInclusionStatus::
                     WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE)) {
    warning_reasons->push_back(protocol::Audits::CookieWarningReasonEnum::
                                   WarnSameSiteLaxCrossDowngradeLax);
  }

  if (status.HasWarningReason(
          net::CookieInclusionStatus::WARN_DOMAIN_NON_ASCII)) {
    warning_reasons->push_back(
        protocol::Audits::CookieWarningReasonEnum::WarnDomainNonASCII);
  }

  return warning_reasons;
}

protocol::String BuildCookieOperation(blink::mojom::CookieOperation operation) {
  switch (operation) {
    case blink::mojom::CookieOperation::kReadCookie:
      return protocol::Audits::CookieOperationEnum::ReadCookie;
    case blink::mojom::CookieOperation::kSetCookie:
      return protocol::Audits::CookieOperationEnum::SetCookie;
  }
}

}  // namespace

void ReportCookieIssue(
    RenderFrameHostImpl* render_frame_host_impl,
    const network::mojom::CookieOrLineWithAccessResultPtr& excluded_cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    blink::mojom::CookieOperation operation,
    const absl::optional<std::string>& devtools_request_id) {
  auto exclusion_reasons =
      BuildExclusionReasons(excluded_cookie->access_result.status);
  auto warning_reasons =
      BuildWarningReasons(excluded_cookie->access_result.status);
  if (exclusion_reasons->empty() && warning_reasons->empty()) {
    // If we don't report any reason, there is no point in informing DevTools.
    return;
  }

  std::unique_ptr<protocol::Audits::AffectedRequest> affected_request;
  if (devtools_request_id) {
    // We can report the url here, because if devtools_request_id is set, the
    // url is the url of the request.
    affected_request = protocol::Audits::AffectedRequest::Create()
                           .SetRequestId(*devtools_request_id)
                           .SetUrl(url.spec())
                           .Build();
  }

  auto cookie_issue_details =
      protocol::Audits::CookieIssueDetails::Create()
          .SetCookieExclusionReasons(std::move(exclusion_reasons))
          .SetCookieWarningReasons(std::move(warning_reasons))
          .SetOperation(BuildCookieOperation(operation))
          .SetCookieUrl(url.spec())
          .SetRequest(std::move(affected_request))
          .Build();

  if (excluded_cookie->cookie_or_line->is_cookie()) {
    const auto& cookie = excluded_cookie->cookie_or_line->get_cookie();
    auto affected_cookie = protocol::Audits::AffectedCookie::Create()
                               .SetName(cookie.Name())
                               .SetPath(cookie.Path())
                               .SetDomain(cookie.Domain())
                               .Build();
    cookie_issue_details->SetCookie(std::move(affected_cookie));
  } else {
    CHECK(excluded_cookie->cookie_or_line->is_cookie_string());
    cookie_issue_details->SetRawCookieLine(
        excluded_cookie->cookie_or_line->get_cookie_string());
  }

  if (!site_for_cookies.IsNull()) {
    cookie_issue_details->SetSiteForCookies(
        site_for_cookies.RepresentativeUrl().spec());
  }

  auto details = protocol::Audits::InspectorIssueDetails::Create()
                     .SetCookieIssueDetails(std::move(cookie_issue_details))
                     .Build();

  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(protocol::Audits::InspectorIssueCodeEnum::CookieIssue)
          .SetDetails(std::move(details))
          .Build();

  ReportBrowserInitiatedIssue(render_frame_host_impl, issue.get());
}

namespace {

void AddIssueToIssueStorage(
    RenderFrameHost* rfh,
    std::unique_ptr<protocol::Audits::InspectorIssue> issue) {
  // We only utilize a central storage on the page. Each issue is still
  // associated with the originating |RenderFrameHost| though.
  DevToolsIssueStorage* issue_storage =
      DevToolsIssueStorage::GetOrCreateForPage(
          rfh->GetOutermostMainFrame()->GetPage());

  issue_storage->AddInspectorIssue(rfh, std::move(issue));
}

}  // namespace

void ReportBrowserInitiatedIssue(RenderFrameHostImpl* frame,
                                 protocol::Audits::InspectorIssue* issue) {
  FrameTreeNode* ftn = frame->frame_tree_node();
  if (!ftn)
    return;

  AddIssueToIssueStorage(frame, issue->Clone());
  DispatchToAgents(ftn, &protocol::AuditsHandler::OnIssueAdded, issue);
}

void BuildAndReportBrowserInitiatedIssue(
    RenderFrameHostImpl* frame,
    blink::mojom::InspectorIssueInfoPtr info) {
  std::unique_ptr<protocol::Audits::InspectorIssue> issue;
  if (info->code ==
      blink::mojom::InspectorIssueCode::kTrustedWebActivityIssue) {
    issue = BuildTWAQualityIssue(info->details->twa_issue_details);
  } else if (info->code == blink::mojom::InspectorIssueCode::kHeavyAdIssue) {
    issue = BuildHeavyAdIssue(info->details->heavy_ad_issue_details);
  } else if (info->code ==
             blink::mojom::InspectorIssueCode::kFederatedAuthRequestIssue) {
    issue = BuildFederatedAuthRequestIssue(
        info->details->federated_auth_request_details);
  } else if (info->code ==
             blink::mojom::InspectorIssueCode::kDeprecationIssue) {
    issue = BuildDeprecationIssue(info->details->deprecation_issue_details);
  } else if (info->code ==
             blink::mojom::InspectorIssueCode::kBounceTrackingIssue) {
    issue =
        BuildBounceTrackingIssue(info->details->bounce_tracking_issue_details);
  } else {
    NOTREACHED() << "Unsupported type of browser-initiated issue";
  }
  ReportBrowserInitiatedIssue(frame, issue.get());
}

void OnWebTransportHandshakeFailed(
    RenderFrameHostImpl* frame,
    const GURL& url,
    const absl::optional<net::WebTransportError>& error) {
  FrameTreeNode* ftn = frame->frame_tree_node();
  if (!ftn)
    return;
  std::string text = base::StringPrintf(
      "Failed to establish a connection to %s", url.spec().c_str());
  if (error) {
    text += ": ";
    text += net::WebTransportErrorToString(*error);
  }
  text += ".";
  auto entry = protocol::Log::LogEntry::Create()
                   .SetSource(protocol::Log::LogEntry::SourceEnum::Network)
                   .SetLevel(protocol::Log::LogEntry::LevelEnum::Error)
                   .SetText(text)
                   .SetTimestamp(base::Time::Now().ToDoubleT() * 1000.0)
                   .Build();
  DispatchToAgents(ftn, &protocol::LogHandler::EntryAdded, entry.get());
}

void OnServiceWorkerMainScriptFetchingFailed(
    const GlobalRenderFrameHostId& requesting_frame_id,
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id,
    const std::string& error,
    const network::URLLoaderCompletionStatus& status,
    const network::mojom::URLResponseHead* response_head,
    const GURL& url) {
  DCHECK(!error.empty());
  DCHECK_NE(net::OK, status.error_code);

  // If we have a requesting_frame_id, we should have a frame and a frame tree
  // node. However since the lifetime of these objects can be complex, we check
  // at each step that we indeed can go reach all the way to the FrameTreeNode.
  if (!requesting_frame_id)
    return;

  RenderFrameHostImpl* requesting_frame =
      RenderFrameHostImpl::FromID(requesting_frame_id);
  if (!requesting_frame)
    return;

  FrameTreeNode* ftn = requesting_frame->frame_tree_node();
  if (!ftn)
    return;

  auto entry = protocol::Log::LogEntry::Create()
                   .SetSource(protocol::Log::LogEntry::SourceEnum::Network)
                   .SetLevel(protocol::Log::LogEntry::LevelEnum::Error)
                   .SetText(error)
                   .SetTimestamp(base::Time::Now().ToDoubleT() * 1000.0)
                   .Build();
  DispatchToAgents(ftn, &protocol::LogHandler::EntryAdded, entry.get());

  ServiceWorkerDevToolsAgentHost* agent_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForNewInstallingWorker(context_wrapper,
                                                       version_id);

  if (response_head) {
    DCHECK(agent_host);
    network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
        network::ExtractDevToolsInfo(*response_head);
    auto worker_token = agent_host->devtools_worker_token().ToString();
    for (auto* network_handler :
         protocol::NetworkHandler::ForAgentHost(agent_host)) {
      network_handler->ResponseReceived(
          worker_token, worker_token, url,
          protocol::Network::ResourceTypeEnum::Other, *head_info,
          requesting_frame->devtools_frame_token().ToString());
      network_handler->frontend()->LoadingFinished(
          worker_token,
          status.completion_time.ToInternalValue() /
              static_cast<double>(base::Time::kMicrosecondsPerSecond),
          status.encoded_data_length);
    }
  } else if (agent_host) {
    for (auto* network_handler :
         protocol::NetworkHandler::ForAgentHost(agent_host)) {
      network_handler->LoadingComplete(
          agent_host->devtools_worker_token().ToString(),
          protocol::Network::ResourceTypeEnum::Other, status);
    }
  }
}

namespace {

// Only assign request id if there's an enabled agent host.
void MaybeAssignResourceRequestId(DevToolsAgentHostImpl* host,
                                  const std::string& id,
                                  network::ResourceRequest& request) {
  DCHECK(!request.devtools_request_id.has_value());
  for (auto* network_handler : protocol::NetworkHandler::ForAgentHost(host)) {
    if (network_handler->enabled()) {
      request.devtools_request_id = id;
      return;
    }
  }
}

}  // namespace

void MaybeAssignResourceRequestId(FrameTreeNode* ftn,
                                  const std::string& id,
                                  network::ResourceRequest& request) {
  if (auto* host = RenderFrameDevToolsAgentHost::GetFor(ftn))
    MaybeAssignResourceRequestId(host, id, request);
}

void OnServiceWorkerMainScriptRequestWillBeSent(
    const GlobalRenderFrameHostId& requesting_frame_id,
    const ServiceWorkerContextWrapper* context_wrapper,
    int64_t version_id,
    network::ResourceRequest& request) {
  // Currently, `requesting_frame_id` is invalid when payment apps and
  // extensions register a service worker. See the callers of
  // ServiceWorkerContextWrapper::RegisterServiceWorker().
  if (!requesting_frame_id)
    return;

  RenderFrameHostImpl* requesting_frame =
      RenderFrameHostImpl::FromID(requesting_frame_id);
  if (!requesting_frame)
    return;

  auto timestamp = base::TimeTicks::Now();
  network::mojom::URLRequestDevToolsInfoPtr request_info =
      network::ExtractDevToolsInfo(request);

  ServiceWorkerDevToolsAgentHost* agent_host =
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForNewInstallingWorker(context_wrapper,
                                                       version_id);
  DCHECK(agent_host);
  const std::string request_id = agent_host->devtools_worker_token().ToString();
  MaybeAssignResourceRequestId(agent_host, request_id, request);
  for (auto* network_handler :
       protocol::NetworkHandler::ForAgentHost(agent_host)) {
    network_handler->RequestSent(
        request_id,
        /*loader_id=*/"", request.headers, *request_info,
        protocol::Network::Initiator::TypeEnum::Other,
        requesting_frame->GetLastCommittedURL(),
        /*initiator_devtools_request_id=*/"", timestamp);
  }
}

void OnWorkerMainScriptLoadingFailed(
    const GURL& url,
    const base::UnguessableToken& worker_token,
    FrameTreeNode* ftn,
    RenderFrameHostImpl* ancestor_rfh,
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(ftn);

  std::string id = worker_token.ToString();

  if (status.blocked_by_response_reason)
    ReportBlockedByResponseIssue(url, id, ftn, ancestor_rfh, status);

  DispatchToAgents(ftn, &protocol::NetworkHandler::LoadingComplete, id,
                   protocol::Network::ResourceTypeEnum::Other, status);
}

void OnWorkerMainScriptLoadingFinished(
    FrameTreeNode* ftn,
    const base::UnguessableToken& worker_token,
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(ftn);
  DispatchToAgents(ftn, &protocol::NetworkHandler::LoadingComplete,
                   worker_token.ToString(),
                   protocol::Network::ResourceTypeEnum::Other, status);
}

void OnWorkerMainScriptRequestWillBeSent(
    FrameTreeNode* ftn,
    const base::UnguessableToken& worker_token,
    network::ResourceRequest& request) {
  DCHECK(ftn);

  auto timestamp = base::TimeTicks::Now();
  network::mojom::URLRequestDevToolsInfoPtr request_info =
      network::ExtractDevToolsInfo(request);

  auto* owner_host = RenderFrameDevToolsAgentHost::GetFor(ftn);
  if (!owner_host)
    return;
  MaybeAssignResourceRequestId(owner_host, worker_token.ToString(), request);
  DispatchToAgents(
      ftn, &protocol::NetworkHandler::RequestSent, worker_token.ToString(),
      /*loader_id=*/"", request.headers, *request_info,
      protocol::Network::Initiator::TypeEnum::Other, ftn->current_url(),
      /*initiator_devtools_request_id*/ "", timestamp);
}

void LogWorkletMessage(RenderFrameHostImpl& frame_host,
                       blink::mojom::ConsoleMessageLevel log_level,
                       const std::string& message) {
  FrameTreeNode* ftn = frame_host.frame_tree_node();
  if (!ftn)
    return;

  std::string log_level_string;
  switch (log_level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      log_level_string = protocol::Log::LogEntry::LevelEnum::Verbose;
      break;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      log_level_string = protocol::Log::LogEntry::LevelEnum::Info;
      break;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      log_level_string = protocol::Log::LogEntry::LevelEnum::Warning;
      break;
    case blink::mojom::ConsoleMessageLevel::kError:
      log_level_string = protocol::Log::LogEntry::LevelEnum::Error;
      break;
  }

  DCHECK(!log_level_string.empty());

  auto entry = protocol::Log::LogEntry::Create()
                   .SetSource(protocol::Log::LogEntry::SourceEnum::Other)
                   .SetLevel(log_level_string)
                   .SetText(message)
                   .SetTimestamp(base::Time::Now().ToDoubleT() * 1000.0)
                   .Build();
  DispatchToAgents(ftn, &protocol::LogHandler::EntryAdded, entry.get());

  // Manually trigger RenderFrameHostImpl::DidAddMessageToConsole, so that the
  // observer behavior aligns more with the observer behavior for the regular
  // devtools logging path from the renderer.
  frame_host.DidAddMessageToConsole(log_level, base::UTF8ToUTF16(message),
                                    /*line_no=*/0, /*source_id=*/{},
                                    /*untrusted_stack_trace=*/{});
}

void ApplyNetworkContextParamsOverrides(
    BrowserContext* browser_context,
    network::mojom::NetworkContextParams* context_params) {
  for (auto* agent_host : BrowserDevToolsAgentHost::Instances()) {
    for (auto* target_handler :
         protocol::TargetHandler::ForAgentHost(agent_host)) {
      target_handler->ApplyNetworkContextParamsOverrides(browser_context,
                                                         context_params);
    }
  }
}

protocol::Audits::GenericIssueErrorType GenericIssueErrorTypeToProtocol(
    blink::mojom::GenericIssueErrorType error_type) {
  switch (error_type) {
    case blink::mojom::GenericIssueErrorType::
        kCrossOriginPortalPostMessageError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          CrossOriginPortalPostMessageError;
    case blink::mojom::GenericIssueErrorType::kFormLabelForNameError:
      return protocol::Audits::GenericIssueErrorTypeEnum::FormLabelForNameError;
    case blink::mojom::GenericIssueErrorType::kFormDuplicateIdForInputError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormDuplicateIdForInputError;
    case blink::mojom::GenericIssueErrorType::kFormInputWithNoLabelError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormInputWithNoLabelError;
    case blink::mojom::GenericIssueErrorType::
        kFormAutocompleteAttributeEmptyError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormAutocompleteAttributeEmptyError;
    case blink::mojom::GenericIssueErrorType::
        kFormEmptyIdAndNameAttributesForInputError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormEmptyIdAndNameAttributesForInputError;
    case blink::mojom::GenericIssueErrorType::
        kFormAriaLabelledByToNonExistingId:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormAriaLabelledByToNonExistingId;
    case blink::mojom::GenericIssueErrorType::
        kFormInputAssignedAutocompleteValueToIdOrNameAttributeError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormInputAssignedAutocompleteValueToIdOrNameAttributeError;
    case blink::mojom::GenericIssueErrorType::
        kFormLabelHasNeitherForNorNestedInput:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormLabelHasNeitherForNorNestedInput;
    case blink::mojom::GenericIssueErrorType::
        kFormLabelForMatchesNonExistingIdError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormLabelForMatchesNonExistingIdError;
    case blink::mojom::GenericIssueErrorType::
        kFormInputHasWrongButWellIntendedAutocompleteValueError:
      return protocol::Audits::GenericIssueErrorTypeEnum::
          FormInputHasWrongButWellIntendedAutocompleteValueError;
  }
}

namespace {
struct GenericIssueInfo {
  GenericIssueInfo() = default;
  ~GenericIssueInfo() = default;
  GenericIssueInfo(const GenericIssueInfo& info) = default;

  blink::mojom::GenericIssueErrorType error_type;
  absl::optional<std::string> frame_id;
};

void BuildAndReportGenericIssue(RenderFrameHostImpl* render_frame_host_impl,
                                const GenericIssueInfo& issue_info) {
  auto generic_issue_details =
      protocol::Audits::GenericIssueDetails::Create()
          .SetErrorType(GenericIssueErrorTypeToProtocol(issue_info.error_type))
          .Build();

  if (issue_info.frame_id) {
    generic_issue_details->SetFrameId(*issue_info.frame_id);
  }

  auto issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(protocol::Audits::InspectorIssueCodeEnum::GenericIssue)
          .SetDetails(
              protocol::Audits::InspectorIssueDetails::Create()
                  .SetGenericIssueDetails(std::move(generic_issue_details))
                  .Build())
          .Build();

  ReportBrowserInitiatedIssue(render_frame_host_impl, issue.get());
}
}  // namespace

void DidRejectCrossOriginPortalMessage(
    RenderFrameHostImpl* render_frame_host_impl) {
  GenericIssueInfo issue_info;
  issue_info.error_type =
      blink::mojom::GenericIssueErrorType::kCrossOriginPortalPostMessageError;
  issue_info.frame_id =
      render_frame_host_impl->GetDevToolsFrameToken().ToString();

  BuildAndReportGenericIssue(render_frame_host_impl, issue_info);
}

void UpdateDeviceRequestPrompt(RenderFrameHost* render_frame_host,
                               DevtoolsDeviceRequestPromptInfo* prompt_info) {
  FrameTreeNode* ftn = FrameTreeNode::From(render_frame_host);
  if (!ftn)
    return;
  DispatchToAgents(ftn,
                   &protocol::DeviceAccessHandler::UpdateDeviceRequestPrompt,
                   prompt_info);
}

void CleanUpDeviceRequestPrompt(RenderFrameHost* render_frame_host,
                                DevtoolsDeviceRequestPromptInfo* prompt_info) {
  FrameTreeNode* ftn = FrameTreeNode::From(render_frame_host);
  if (!ftn)
    return;
  DispatchToAgents(ftn,
                   &protocol::DeviceAccessHandler::CleanUpDeviceRequestPrompt,
                   prompt_info);
}

void WillSendFedCmRequest(RenderFrameHost* render_frame_host,
                          bool* intercept,
                          bool* disable_delay) {
  FrameTreeNode* ftn = FrameTreeNode::From(render_frame_host);
  if (!ftn) {
    return;
  }
  DispatchToAgents(ftn, &protocol::FedCmHandler::WillSendRequest, intercept,
                   disable_delay);
}

void WillShowFedCmDialog(RenderFrameHost* render_frame_host, bool* intercept) {
  FrameTreeNode* ftn = FrameTreeNode::From(render_frame_host);
  if (!ftn) {
    return;
  }
  DispatchToAgents(ftn, &protocol::FedCmHandler::WillShowDialog, intercept);
}

void OnFedCmAccountsDialogShown(RenderFrameHost* render_frame_host) {
  FrameTreeNode* ftn = FrameTreeNode::From(render_frame_host);
  if (!ftn) {
    return;
  }
  DispatchToAgents(ftn, &protocol::FedCmHandler::OnDialogShown);
}

}  // namespace devtools_instrumentation

}  // namespace content
