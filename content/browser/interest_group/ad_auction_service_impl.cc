// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_service_impl.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/aggregation_service/features.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/ad_auction_document_data.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/browser/interest_group/ad_auction_result_metrics.h"
#include "content/browser/interest_group/auction_runner.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_client.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace content {

namespace {

constexpr base::TimeDelta kMaxExpiry = base::Days(30);

bool IsAdRequestValid(const blink::mojom::AdRequestConfig& config) {
  // The ad_request_url origin has to be HTTPS.
  if (config.ad_request_url.scheme() != url::kHttpsScheme) {
    return false;
  }

  // At least one adProperties is required to request potential ads.
  if (config.ad_properties.size() <= 0) {
    return false;
  }

  // If a fallback source is specified it must be HTTPS.
  if (config.fallback_source &&
      (config.fallback_source->scheme() != url::kHttpsScheme)) {
    return false;
  }

  return true;
}

// This function is used as a callback to verify
// `InterestGroup::Ad::allowed_reporting_origins` are attested. These origins
// are specified as part of the ads during `joinAdInterestGroup()` and
// `updateAdInterestGroups()`. They receive reporting beacons sent by
// `reportEvent()` when reporting to custom urls.
bool AreAllowedReportingOriginsAttested(
    BrowserContext* browser_context,
    const std::vector<url::Origin>& origins) {
  for (const auto& origin : origins) {
    if (!GetContentClient()
             ->browser()
             ->IsPrivacySandboxReportingDestinationAttested(
                 browser_context, origin,
                 PrivacySandboxInvokingAPI::kProtectedAudience,
                 /*post_impression_reporting=*/true)) {
      return false;
    }
  }
  return true;
}

}  // namespace

AdAuctionServiceImpl::BiddingAndAuctionDataConstructionState::
    BiddingAndAuctionDataConstructionState()
    : start_time(base::TimeTicks::Now()),
      request_id(base::Uuid::GenerateRandomV4()) {}
AdAuctionServiceImpl::BiddingAndAuctionDataConstructionState::
    BiddingAndAuctionDataConstructionState(
        BiddingAndAuctionDataConstructionState&& other) = default;
AdAuctionServiceImpl::BiddingAndAuctionDataConstructionState::
    ~BiddingAndAuctionDataConstructionState() = default;

// static
void AdAuctionServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver) {
  CHECK(render_frame_host);

  // The object is bound to the lifetime of `render_frame_host` and the mojo
  // connection. See DocumentService for details.
  new AdAuctionServiceImpl(*render_frame_host, std::move(receiver));
}

void AdAuctionServiceImpl::JoinInterestGroup(
    const blink::InterestGroup& group,
    JoinInterestGroupCallback callback) {
  if (!JoinOrLeaveApiAllowedFromRenderer(group.owner)) {
    return;
  }

  // If the interest group API is not allowed for this origin, report the result
  // of the permissions check, but don't actually join the interest group.
  // The return value of IsInterestGroupAPIAllowed() is potentially affected by
  // a user's browser configuration, which shouldn't be leaked to sites to
  // protect against fingerprinting.
  bool report_result_only = !IsInterestGroupAPIAllowed(
      ContentBrowserClient::InterestGroupApiOperation::kJoin, group.owner);

  blink::InterestGroup updated_group = group;
  base::Time max_expiry = base::Time::Now() + kMaxExpiry;
  if (updated_group.expiry > max_expiry) {
    updated_group.expiry = max_expiry;
  }

  if (!base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiMultipleCloudProviders) ||
      !base::FeatureList::IsEnabled(
          aggregation_service::kAggregationServiceMultipleCloudProviders)) {
    // Override with the default if a non-default coordinator is specified when
    // the feature is disabled.
    updated_group.aggregation_coordinator_origin = absl::nullopt;
  }

  if (updated_group.aggregation_coordinator_origin &&
      !aggregation_service::IsAggregationCoordinatorOriginAllowed(
          updated_group.aggregation_coordinator_origin.value())) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: aggregationCoordinatorOrigin is not supported.");
    return;
  }

  // `base::Unretained` is safe here since the `BrowserContext` owns the
  // `StoragePartition` that owns the interest group manager.
  GetInterestGroupManager().CheckPermissionsAndJoinInterestGroup(
      std::move(updated_group), main_frame_url_, origin(),
      GetFrame()->GetNetworkIsolationKey(), report_result_only,
      *GetFrameURLLoaderFactory(),
      base::BindRepeating(
          &AreAllowedReportingOriginsAttested,
          base::Unretained(render_frame_host().GetBrowserContext())),
      std::move(callback));
}

void AdAuctionServiceImpl::LeaveInterestGroup(
    const url::Origin& owner,
    const std::string& name,
    LeaveInterestGroupCallback callback) {
  if (!JoinOrLeaveApiAllowedFromRenderer(owner)) {
    return;
  }

  // If the interest group API is not allowed for this origin, report the result
  // of the permissions check, but don't actually join the interest group.
  // The return value of IsInterestGroupAPIAllowed() is potentially affected by
  // a user's browser configuration, which shouldn't be leaked to sites to
  // protect against fingerprinting.
  bool report_result_only = !IsInterestGroupAPIAllowed(
      ContentBrowserClient::InterestGroupApiOperation::kLeave, owner);

  GetInterestGroupManager().CheckPermissionsAndLeaveInterestGroup(
      blink::InterestGroupKey(owner, name), main_frame_origin_, origin(),
      GetFrame()->GetNetworkIsolationKey(), report_result_only,
      *GetFrameURLLoaderFactory(), std::move(callback));
}

void AdAuctionServiceImpl::LeaveInterestGroupForDocument() {
  // Based on the spec, permission policy is bypassed for leaving implicit
  // interest groups.

  // If the interest group API is not allowed for this origin do nothing.
  if (!IsInterestGroupAPIAllowed(
          ContentBrowserClient::InterestGroupApiOperation::kLeave, origin())) {
    return;
  }

  if (origin().scheme() != url::kHttpsScheme) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: LeaveInterestGroupForDocument only supported for "
        "secure origins");
    return;
  }

  // Get interest group owner and name from the ad auction data, which is part
  // of the fenced frame properties. Here the fenced frame properties are
  // obtained from the closest ancestor that has valid fenced frame properties.
  // This is because both top-level ads and ad components may have ad auction
  // data.
  const absl::optional<FencedFrameProperties>& fenced_frame_properties =
      GetFrame()->frame_tree_node()->GetFencedFrameProperties(
          FencedFramePropertiesNodeSource::kClosestAncestor);

  // This frame is neither a fenced frame or an urn iframe itself, nor it is
  // nested within a fenced frame or an urn iframe.
  if (!fenced_frame_properties.has_value()) {
    devtools_instrumentation::LogWorkletMessage(
        *GetFrame(), blink::mojom::ConsoleMessageLevel::kError,
        "Owner and name are required to call LeaveAdInterestGroup outside of "
        "a fenced frame or an opaque origin iframe.");
    return;
  }

  if (!fenced_frame_properties->ad_auction_data_.has_value()) {
    return;
  }

  if (fenced_frame_properties->is_ad_component_ &&
      !base::FeatureList::IsEnabled(
          blink::features::kFencedFramesM120FeaturesPart2)) {
    // The ability to leave interest group from an ad component is not supported
    // before M120.
    return;
  }

  const blink::FencedFrame::AdAuctionData& auction_data =
      fenced_frame_properties->ad_auction_data_->GetValueIgnoringVisibility();

  if (auction_data.interest_group_owner != origin()) {
    // The ad page calling LeaveAdInterestGroup is not the owner of the group.
    return;
  }

  GetInterestGroupManager().LeaveInterestGroup(
      blink::InterestGroupKey(auction_data.interest_group_owner,
                              auction_data.interest_group_name),
      main_frame_origin_);
}

void AdAuctionServiceImpl::ClearOriginJoinedInterestGroups(
    const url::Origin& owner,
    const std::vector<std::string>& interest_groups_to_keep,
    ClearOriginJoinedInterestGroupsCallback callback) {
  if (!JoinOrLeaveApiAllowedFromRenderer(owner)) {
    return;
  }

  // If the interest group leave API is not allowed for this origin, report the
  // result of the permissions check, but don't actually join the interest
  // group. The return value of IsInterestGroupAPIAllowed() is potentially
  // affected by a user's browser configuration, which shouldn't be leaked to
  // sites to protect against fingerprinting.
  bool report_result_only = !IsInterestGroupAPIAllowed(
      ContentBrowserClient::InterestGroupApiOperation::kLeave, owner);

  GetInterestGroupManager().CheckPermissionsAndClearOriginJoinedInterestGroups(
      owner, interest_groups_to_keep, main_frame_origin_, origin(),
      GetFrame()->GetNetworkIsolationKey(), report_result_only,
      *GetFrameURLLoaderFactory(), std::move(callback));
}

void AdAuctionServiceImpl::UpdateAdInterestGroups() {
  // If the interest group API is not allowed for this context by Permissions
  // Policy, do nothing
  if (!render_frame_host().IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    ReportBadMessageAndDeleteThis("Unexpected request");
    return;
  }
  // If the interest group API is not allowed for this origin do nothing.
  if (!IsInterestGroupAPIAllowed(
          ContentBrowserClient::InterestGroupApiOperation::kUpdate, origin())) {
    return;
  }

  // `base::Unretained` is safe here since the `BrowserContext` owns the
  // `StoragePartition` that owns the interest group manager.
  GetInterestGroupManager().UpdateInterestGroupsOfOwner(
      origin(), GetClientSecurityState(),
      base::BindRepeating(
          &AreAllowedReportingOriginsAttested,
          base::Unretained(render_frame_host().GetBrowserContext())));
}

void AdAuctionServiceImpl::CreateAuctionNonce(
    CreateAuctionNonceCallback callback) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kFledgeNegativeTargeting)) {
    ReportBadMessageAndDeleteThis(
        "CreateAuctionNonce with FledgeNegativeTargeting off");
    return;
  }
  std::move(callback).Run(
      static_cast<base::Uuid>(auction_nonce_manager_.CreateAuctionNonce()));
}

void AdAuctionServiceImpl::RunAdAuction(
    const blink::AuctionConfig& config,
    mojo::PendingReceiver<blink::mojom::AbortableAdAuction> abort_receiver,
    RunAdAuctionCallback callback) {
  // Ensure the page is not in prerendering as code belows expect it, i.e.
  // GetPageUkmSourceId() doesn't work with prerendering pages.
  CHECK(!render_frame_host().IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));

  // If the run ad auction API is not allowed for this context by Permissions
  // Policy, do nothing
  if (!render_frame_host().IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction)) {
    ReportBadMessageAndDeleteThis("Unexpected request");
    return;
  }

  auto* auction_result_metrics =
      AdAuctionResultMetrics::GetOrCreateForPage(render_frame_host().GetPage());
  if (!auction_result_metrics->ShouldRunAuction()) {
    std::move(callback).Run(/*aborted_by_script=*/false,
                            /*config=*/absl::nullopt);
    return;
  }

  if (!base::FeatureList::IsEnabled(
          blink::features::kFledgeNegativeTargeting)) {
    if (config.non_shared_params.auction_nonce) {
      ReportBadMessageAndDeleteThis(
          "auction_nonce set with FledgeNegativeTargeting off");
      return;
    }
    for (const auto& component : config.non_shared_params.component_auctions) {
      if (component.non_shared_params.auction_nonce) {
        ReportBadMessageAndDeleteThis(
            "auction_nonce set with FledgeNegativeTargeting off");
        return;
      }
    }
  }

  FencedFrameURLMapping& fenced_frame_urls_map =
      GetFrame()->GetPage().fenced_frame_urls_map();
  auto urn_uuid = fenced_frame_urls_map.GeneratePendingMappedURN();

  // If pending mapped URN cannot be generated due to number of mappings has
  // reached limit, stop the auction.
  if (!urn_uuid.has_value()) {
    std::move(callback).Run(/*aborted_by_script=*/false,
                            /*config=*/absl::nullopt);
    return;
  }

  AdAuctionPageData* ad_auction_page_data =
      PageUserData<AdAuctionPageData>::GetOrCreateForPage(
          render_frame_host().GetPage());

  std::unique_ptr<AuctionRunner> auction = AuctionRunner::CreateAndStart(
      &auction_worklet_manager_, &auction_nonce_manager_,
      &GetInterestGroupManager(), render_frame_host().GetBrowserContext(),
      private_aggregation_manager_, ad_auction_page_data,
      // Unlike other callbacks, this needs to be safe to call after destruction
      // of the AdAuctionServiceImpl, so that the reporter can outlive it.
      base::BindRepeating(
          &AdAuctionServiceImpl::MaybeLogPrivateAggregationFeatures,
          weak_ptr_factory_.GetWeakPtr()),
      config, main_frame_origin_, origin(),
      render_frame_host().GetPageUkmSourceId(), GetClientSecurityState(),
      GetRefCountedTrustedURLLoaderFactory(),
      base::BindRepeating(&AdAuctionServiceImpl::IsInterestGroupAPIAllowed,
                          base::Unretained(this)),
      base::BindRepeating(
          &AreAllowedReportingOriginsAttested,
          base::Unretained(render_frame_host().GetBrowserContext())),
      std::move(abort_receiver),
      base::BindOnce(
          &AdAuctionServiceImpl::OnAuctionComplete, base::Unretained(this),
          std::move(callback), std::move(urn_uuid.value()),
          fenced_frame_urls_map.unique_id(), GetFrame()->GetGlobalId(),
          GetFrame()->GetPage().GetWeakPtrImpl()));
  AuctionRunner* raw_auction = auction.get();
  auctions_.emplace(raw_auction, std::move(auction));
}

namespace {

// Helper class to retrieve the URL that a given URN is mapped to.
class FencedFrameURLMappingObserver
    : public FencedFrameURLMapping::MappingResultObserver {
 public:
  // Retrieves the URL that `urn_url` is mapped to, if any. If `send_reports` is
  // true, sends the reports associated with `urn_url`, if there are any.
  static absl::optional<GURL> GetURL(RenderFrameHostImpl& render_frame_host,
                                     const GURL& urn_url,
                                     bool send_reports) {
    absl::optional<GURL> mapped_url;
    FencedFrameURLMappingObserver obs(&mapped_url, send_reports);
    content::FencedFrameURLMapping& mapping =
        render_frame_host.GetPage().fenced_frame_urls_map();
    // FLEDGE URN URLs should already be mapped, so the observer will be called
    // synchronously.
    mapping.ConvertFencedFrameURNToURL(urn_url, &obs);
    if (!obs.called_) {
      mapping.RemoveObserverForURN(urn_url, &obs);
    }
    return mapped_url;
  }

 private:
  FencedFrameURLMappingObserver(absl::optional<GURL>* mapped_url,
                                bool send_reports)
      : mapped_url_(mapped_url), send_reports_(send_reports) {}

  ~FencedFrameURLMappingObserver() override = default;

  void OnFencedFrameURLMappingComplete(
      const absl::optional<FencedFrameProperties>& properties) override {
    if (properties) {
      if (properties->mapped_url_) {
        *mapped_url_ = properties->mapped_url_->GetValueIgnoringVisibility();
      }
      if (send_reports_ && properties->on_navigate_callback_) {
        properties->on_navigate_callback_.Run();
      }
    }
    called_ = true;
  }

  bool called_ = false;
  raw_ptr<absl::optional<GURL>> mapped_url_;
  bool send_reports_;
};

}  // namespace

void AdAuctionServiceImpl::DeprecatedGetURLFromURN(
    const GURL& urn_url,
    bool send_reports,
    DeprecatedGetURLFromURNCallback callback) {
  if (!blink::IsValidUrnUuidURL(urn_url)) {
    ReportBadMessageAndDeleteThis("Unexpected request: invalid URN");
    return;
  }

  std::move(callback).Run(FencedFrameURLMappingObserver::GetURL(
      static_cast<RenderFrameHostImpl&>(render_frame_host()), urn_url,
      send_reports));
}

void AdAuctionServiceImpl::DeprecatedReplaceInURN(
    const GURL& urn_url,
    std::vector<blink::mojom::AdKeywordReplacementPtr> replacements,
    DeprecatedReplaceInURNCallback callback) {
  if (!blink::IsValidUrnUuidURL(urn_url)) {
    ReportBadMessageAndDeleteThis("Unexpected request: invalid URN");
    return;
  }
  std::vector<std::pair<std::string, std::string>> local_replacements;
  for (const auto& replacement : replacements) {
    if (!(base::StartsWith(replacement->match, "${") &&
          base::EndsWith(replacement->match, "}")) &&
        !(base::StartsWith(replacement->match, "%%") &&
          base::EndsWith(replacement->match, "%%"))) {
      ReportBadMessageAndDeleteThis("Unexpected request: bad replacement");
      return;
    }
    local_replacements.emplace_back(std::move(replacement->match),
                                    std::move(replacement->replacement));
  }
  content::FencedFrameURLMapping& mapping =
      static_cast<RenderFrameHostImpl&>(render_frame_host())
          .GetPage()
          .fenced_frame_urls_map();
  mapping.SubstituteMappedURL(urn_url, local_replacements);
  std::move(callback).Run();
}

void AdAuctionServiceImpl::GetInterestGroupAdAuctionData(
    const url::Origin& seller,
    const absl::optional<url::Origin>& coordinator,
    GetInterestGroupAdAuctionDataCallback callback) {
  if (coordinator && coordinator->scheme() != url::kHttpsScheme) {
    ReportBadMessageAndDeleteThis("Invalid Bidding and Auction Coordinator");
    return;
  }

  // If the interest group API is not allowed for this origin do nothing.
  if (!IsInterestGroupAPIAllowed(
          ContentBrowserClient::InterestGroupApiOperation::kSell, seller)) {
    std::move(callback).Run({}, {}, "Attestation Failed");
    return;
  }

  BiddingAndAuctionDataConstructionState state;
  state.callback = std::move(callback);
  state.seller = seller;
  state.coordinator = coordinator;

  GetInterestGroupManager().GetInterestGroupAdAuctionData(
      GetTopWindowOrigin(),
      /* generation_id=*/base::Uuid::GenerateRandomV4(),
      base::BindOnce(&AdAuctionServiceImpl::OnGotAuctionData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state)));
}

void AdAuctionServiceImpl::CreateAdRequest(
    blink::mojom::AdRequestConfigPtr config,
    CreateAdRequestCallback callback) {
  if (!IsAdRequestValid(*config)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // TODO(https://crbug.com/1249186): Actually request Ads and return a guid.
  // For now just act like it failed.
  std::move(callback).Run(absl::nullopt);
}

void AdAuctionServiceImpl::FinalizeAd(const std::string& ads_guid,
                                      const blink::AuctionConfig& config,
                                      FinalizeAdCallback callback) {
  if (ads_guid.empty()) {
    ReportBadMessageAndDeleteThis("GUID empty");
    return;
  }

  // TODO(https://crbug.com/1249186): Actually finalize Ad and return an URL.
  // For now just act like it failed.
  std::move(callback).Run(absl::nullopt);
}

network::mojom::URLLoaderFactory*
AdAuctionServiceImpl::GetFrameURLLoaderFactory() {
  if (!frame_url_loader_factory_ || !frame_url_loader_factory_.is_connected()) {
    frame_url_loader_factory_.reset();
    render_frame_host().CreateNetworkServiceDefaultFactory(
        frame_url_loader_factory_.BindNewPipeAndPassReceiver());
  }
  return frame_url_loader_factory_.get();
}

network::mojom::URLLoaderFactory*
AdAuctionServiceImpl::GetTrustedURLLoaderFactory() {
  // Ensure the page is not in prerendering as code belows expect it, i.e.
  // GetPageUkmSourceId() doesn't work with prerendering pages.
  CHECK(!render_frame_host().IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));

  if (!trusted_url_loader_factory_ ||
      !trusted_url_loader_factory_.is_connected()) {
    trusted_url_loader_factory_.reset();
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver =
        trusted_url_loader_factory_.BindNewPipeAndPassReceiver();

    // TODO(mmenke): Should this have its own URLLoaderFactoryType? FLEDGE
    // requests are very different from subresource requests.
    //
    // TODO(mmenke): Hook up devtools.
    GetContentClient()->browser()->WillCreateURLLoaderFactory(
        render_frame_host().GetSiteInstance()->GetBrowserContext(),
        &render_frame_host(), render_frame_host().GetProcess()->GetID(),
        ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
        url::Origin(), /*navigation_id=*/absl::nullopt,
        ukm::SourceIdObj::FromInt64(render_frame_host().GetPageUkmSourceId()),
        &factory_receiver, /*header_client=*/nullptr,
        /*bypass_redirect_checks=*/nullptr, /*disable_secure_dns=*/nullptr,
        /*factory_override=*/nullptr,
        /*navigation_response_task_runner=*/nullptr);

    render_frame_host()
        .GetStoragePartition()
        ->GetURLLoaderFactoryForBrowserProcess()
        ->Clone(std::move(factory_receiver));

    mojo::Remote<network::mojom::URLLoaderFactory> shared_remote;
    trusted_url_loader_factory_->Clone(
        shared_remote.BindNewPipeAndPassReceiver());
    ref_counted_trusted_url_loader_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(shared_remote));
  }
  return trusted_url_loader_factory_.get();
}

void AdAuctionServiceImpl::PreconnectSocket(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  render_frame_host()
      .GetStoragePartition()
      ->GetNetworkContext()
      ->PreconnectSockets(/*num_streams=*/1, url, /*allow_credentials=*/false,
                          network_anonymization_key);
}

scoped_refptr<network::WrapperSharedURLLoaderFactory>
AdAuctionServiceImpl::GetRefCountedTrustedURLLoaderFactory() {
  GetTrustedURLLoaderFactory();
  return ref_counted_trusted_url_loader_factory_;
}

RenderFrameHostImpl* AdAuctionServiceImpl::GetFrame() {
  return static_cast<RenderFrameHostImpl*>(&render_frame_host());
}

scoped_refptr<SiteInstance> AdAuctionServiceImpl::GetFrameSiteInstance() {
  return render_frame_host().GetSiteInstance();
}

network::mojom::ClientSecurityStatePtr
AdAuctionServiceImpl::GetClientSecurityState() {
  return GetFrame()->BuildClientSecurityState();
}

AdAuctionServiceImpl::AdAuctionServiceImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      main_frame_origin_(
          render_frame_host.GetMainFrame()->GetLastCommittedOrigin()),
      main_frame_url_(render_frame_host.GetMainFrame()->GetLastCommittedURL()),
      auction_worklet_manager_(
          &GetInterestGroupManager().auction_process_manager(),
          GetTopWindowOrigin(),
          origin(),
          this),
      auction_nonce_manager_(GetFrame()),
      private_aggregation_manager_(PrivateAggregationManager::GetManager(
          *render_frame_host.GetBrowserContext())) {}

AdAuctionServiceImpl::~AdAuctionServiceImpl() {
  while (!auctions_.empty()) {
    // Need to fail all auctions rather than just deleting them, to ensure Mojo
    // callbacks from the renderers are invoked. Uninvoked Mojo callbacks may
    // not be destroyed before the Mojo pipe is, and the parent DocumentService
    // class owns the pipe, so it may still be open at this point.
    auctions_.begin()->first->FailAuction(/*aborted_by_script=*/false);
  }
}

bool AdAuctionServiceImpl::JoinOrLeaveApiAllowedFromRenderer(
    const url::Origin& owner) {
  if (origin().scheme() != url::kHttpsScheme) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: Interest groups may only be joined or left from "
        "https origins");
    return false;
  }

  // If the interest group API is not allowed for this context by Permissions
  // Policy, do nothing
  if (!render_frame_host().IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: Interest groups may only be joined or left when "
        "feature join-ad-interest-group is enabled by Permissions Policy");
    return false;
  }

  if (owner.scheme() != url::kHttpsScheme) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: Interest groups may only be owned by https "
        "origins");
    return false;
  }

  return true;
}

bool AdAuctionServiceImpl::IsInterestGroupAPIAllowed(
    ContentBrowserClient::InterestGroupApiOperation
        interest_group_api_operation,
    const url::Origin& origin) const {
  return GetContentClient()->browser()->IsInterestGroupAPIAllowed(
      &render_frame_host(), interest_group_api_operation, main_frame_origin_,
      origin);
}

void AdAuctionServiceImpl::OnAuctionComplete(
    RunAdAuctionCallback callback,
    GURL urn_uuid,
    FencedFrameURLMapping::Id fenced_frame_urls_map_id,
    GlobalRenderFrameHostId render_frame_host_id,
    const base::WeakPtr<PageImpl> page_impl,
    AuctionRunner* auction,
    bool aborted_by_script,
    absl::optional<blink::InterestGroupKey> winning_group_key,
    absl::optional<blink::AdSize> requested_ad_size,
    absl::optional<blink::AdDescriptor> ad_descriptor,
    std::vector<blink::AdDescriptor> ad_component_descriptors,
    std::vector<std::string> errors,
    std::unique_ptr<InterestGroupAuctionReporter> reporter) {
  // Remove `auction` from `auctions_` but temporarily keep it alive - on
  // success, it owns a AuctionWorkletManager::WorkletHandle for the top-level
  // auction, which `reporter` can reuse once started. Fine to delete after
  // starting the reporter.
  auto auction_it = auctions_.find(auction);
  DCHECK(auction_it != auctions_.end());
  std::unique_ptr<AuctionRunner> owned_auction = std::move(auction_it->second);
  auctions_.erase(auction_it);

  // Forward debug information to devtools.
  for (const std::string& error : errors) {
    devtools_instrumentation::LogWorkletMessage(
        *GetFrame(), blink::mojom::ConsoleMessageLevel::kError,
        base::StrCat({"Worklet error: ", error}));
  }

  auto* auction_result_metrics =
      AdAuctionResultMetrics::GetForPage(render_frame_host().GetPage());

  if (!ad_descriptor) {
    DCHECK(!reporter);

    std::move(callback).Run(aborted_by_script, /*config=*/absl::nullopt);
    if (auction_result_metrics) {
      // `auction_result_metrics` can be null since PageUserData like
      // AdAuctionResultMetrics isn't guaranteed to be destroyed after document
      // services like `this`, even though this typically is the case for
      // destruction of the RenderFrameHost (except for renderer crashes).
      //
      // So, we need to guard against this.
      auction_result_metrics->ReportAuctionResult(
          AdAuctionResultMetrics::AuctionResult::kFailed);
    }
    return;
  }

  DCHECK(reporter);
  // Should always be present with an ad_descriptor->url.
  DCHECK(winning_group_key);
  DCHECK(blink::IsValidFencedFrameURL(ad_descriptor->url));
  DCHECK(urn_uuid.is_valid());

  GetContentClient()->browser()->OnAuctionComplete(
      &render_frame_host(),
      InterestGroupManager::InterestGroupDataKey{
          reporter->winning_bid_info()
              .storage_interest_group->interest_group.owner,
          reporter->winning_bid_info().storage_interest_group->joining_origin,
      });

  content::AdAuctionData ad_auction_data{winning_group_key->owner,
                                         winning_group_key->name};
  FencedFrameURLMapping& current_fenced_frame_urls_map =
      GetFrame()->GetPage().fenced_frame_urls_map();
  // TODO(crbug.com/1422301): The auction must operate on the same fenced frame
  // mapping that was used at the beginning of the auction. If not, we fail the
  // auction and dump without crashing the browser. Once the root cause is known
  // and the issue fixed, convert it back to a CHECK.
  //
  // The fenced frame mapping may be changed because:
  // 1. The render frame host has changed.
  // 2. The page owned by the render frame host has changed.
  // 3. The fenced frame mapping of the page has changed.
  if (IsAuctionExpectedToFail(fenced_frame_urls_map_id, render_frame_host_id,
                              page_impl)) {
    // At least one of the RenderFrameHostImpl, PageImpl and the
    // FencedFrameUrlMapping has changed during the auction.
    if (auction_result_metrics) {
      auction_result_metrics->ReportAuctionResult(
          AdAuctionResultMetrics::AuctionResult::kFailed);
    }
    std::move(callback).Run(aborted_by_script, /*config=*/absl::nullopt);
    return;
  }

  // Set up reporting for any fenced frame that's navigated to the winning bid's
  // URL. Use a URLLoaderFactory that will automatically reconnect on network
  // process crashes, and can outlive the frame.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      render_frame_host()
          .GetStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  blink::FencedFrame::RedactedFencedFrameConfig config =
      current_fenced_frame_urls_map.AssignFencedFrameURLAndInterestGroupInfo(
          urn_uuid, requested_ad_size, *ad_descriptor,
          std::move(ad_auction_data),
          reporter->OnNavigateToWinningAdCallback(
              GetFrame()->GetFrameTreeNodeId()),
          ad_component_descriptors, reporter->fenced_frame_reporter());
  std::move(callback).Run(/*aborted_by_script=*/false, std::move(config));

  // Start the InterestGroupAuctionReporter. It will run reporting scripts, but
  // nothing will be reported (nor the reporter deleted) until a fenced frame
  // navigates to the winning ad, which will be signalled by invoking the
  // callback returned by the InterestGroupAuctionReporter's
  // OnNavitationToWinningAdCallback() method (invoked just above).
  reporters_.emplace_front(std::move(reporter));
  reporters_.front()->Start(
      base::BindOnce(&AdAuctionServiceImpl::OnReporterComplete,
                     base::Unretained(this), reporters_.begin()));
  if (auction_result_metrics) {
    auction_result_metrics->ReportAuctionResult(
        AdAuctionResultMetrics::AuctionResult::kSucceeded);
  }
}

void AdAuctionServiceImpl::OnReporterComplete(
    ReporterList::iterator reporter_it) {
  // Forward debug information to devtools.
  //
  // TODO(https://crbug.com/1394777): Ideally this will share code with the
  // handling of the errors from the earlier phases of the auction.
  InterestGroupAuctionReporter* reporter = reporter_it->get();
  for (const std::string& error : reporter->errors()) {
    devtools_instrumentation::LogWorkletMessage(
        *GetFrame(), blink::mojom::ConsoleMessageLevel::kError,
        base::StrCat({"Worklet error: ", error}));
  }

  reporters_.erase(reporter_it);
}

void AdAuctionServiceImpl::MaybeLogPrivateAggregationFeatures(
    const std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>&
        private_aggregation_requests) {
  // TODO(crbug.com/1356654): Improve coverage of these use counters, i.e.
  // for API usage that does not result in a successful request.
  if (private_aggregation_requests.empty()) {
    return;
  }

  if (!has_logged_private_aggregation_enable_debug_mode_web_feature_ &&
      base::ranges::any_of(private_aggregation_requests,
                           [](const auto& request) {
                             return request->debug_mode_details->is_enabled;
                           })) {
    has_logged_private_aggregation_enable_debug_mode_web_feature_ = true;
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &render_frame_host(),
        blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode);
  }

  if (!has_logged_extended_private_aggregation_web_feature_ &&
      base::ranges::any_of(
          private_aggregation_requests, [](const auto& request) {
            return request->contribution->is_for_event_contribution();
          })) {
    has_logged_extended_private_aggregation_web_feature_ = true;
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &render_frame_host(),
        blink::mojom::WebFeature::kPrivateAggregationApiFledgeExtensions);
  }

  if (!has_logged_private_aggregation_web_features_) {
    has_logged_private_aggregation_web_features_ = true;
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &render_frame_host(),
        blink::mojom::WebFeature::kPrivateAggregationApiAll);
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &render_frame_host(),
        blink::mojom::WebFeature::kPrivateAggregationApiFledge);
  }
}

void AdAuctionServiceImpl::OnGotAuctionData(
    BiddingAndAuctionDataConstructionState state,
    BiddingAndAuctionData data) {
  if (data.request.empty()) {
    std::move(state.callback).Run({}, {}, "");
    return;
  }

  state.data = std::move(data);
  absl::optional<url::Origin> coordinator = state.coordinator;
  scoped_refptr<network::WrapperSharedURLLoaderFactory> loader =
      GetRefCountedTrustedURLLoaderFactory();
  network::WrapperSharedURLLoaderFactory* loader_ptr = loader.get();
  GetInterestGroupManager().GetBiddingAndAuctionServerKey(
      loader_ptr, std::move(coordinator),
      base::BindOnce(&AdAuctionServiceImpl::OnGotBiddingAndAuctionServerKey,
                     weak_ptr_factory_.GetWeakPtr(), std::move(state),
                     std::move(loader)));
}

void AdAuctionServiceImpl::OnGotBiddingAndAuctionServerKey(
    BiddingAndAuctionDataConstructionState state,
    scoped_refptr<network::WrapperSharedURLLoaderFactory> loader,
    base::expected<BiddingAndAuctionServerKey, std::string> maybe_key) {
  if (!maybe_key.has_value()) {
    std::move(state.callback).Run({}, {}, maybe_key.error());
    return;
  }

  auto maybe_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      maybe_key->id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  CHECK(maybe_key_config.ok()) << maybe_key_config.status();

  auto maybe_request =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          std::string(state.data.request.begin(), state.data.request.end()),
          maybe_key->key, maybe_key_config.value(),
          kBiddingAndAuctionEncryptionRequestMediaType.Get());
  if (!maybe_request.ok()) {
    std::move(state.callback).Run({}, {}, "Could not create request");
    return;
  }

  std::string data = maybe_request->EncapsulateAndSerialize();

  AdAuctionPageData* ad_auction_page_data =
      PageUserData<AdAuctionPageData>::GetOrCreateForPage(
          render_frame_host().GetPage());

  AdAuctionRequestContext context(
      std::move(state.seller), std::move(state.data.group_names),
      std::move(*maybe_request).ReleaseContext(), state.start_time);
  ad_auction_page_data->RegisterAdAuctionRequestContext(state.request_id,
                                                        std::move(context));
  // Pre-warm data decoder.
  ad_auction_page_data->GetDecoderFor(state.seller)->GetService();

  size_t start_offset = 0;
  if (base::FeatureList::IsEnabled(kBiddingAndAuctionEncryptionMediaType)) {
    // For the modified request format we need to prepend a version number byte
    // to the request.
    start_offset = 1;
  }
  mojo_base::BigBuffer buf(data.size() + start_offset);

  // Write the version byte. If we are not using a modified request this will
  // be immediately overwritten.
  buf.data()[0] = 0;

  // Write the request starting at `start_offset`
  CHECK_EQ(data.size() + start_offset, buf.size());
  std::memcpy(&buf.data()[start_offset], data.data(), data.size());

  std::move(state.callback).Run(std::move(buf), state.request_id, "");
  // Request sizes only increase by factors of two so we only need to sample
  // the powers of two. The maximum of 1 GB size is much larger than it should
  // ever be.
  base::UmaHistogramCustomCounts(/*name=*/"Ads.InterestGroup.BaDataSize",
                                 /*sample=*/data.size(), /*min=*/1,
                                 /*exclusive_max=*/1 << 30, /*buckets=*/30);
  base::UmaHistogramTimes(/*name=*/"Ads.InterestGroup.BaDataConstructionTime",
                          /*sample=*/base::TimeTicks::Now() - state.start_time);
}

InterestGroupManagerImpl& AdAuctionServiceImpl::GetInterestGroupManager()
    const {
  return *static_cast<InterestGroupManagerImpl*>(
      render_frame_host().GetStoragePartition()->GetInterestGroupManager());
}

url::Origin AdAuctionServiceImpl::GetTopWindowOrigin() const {
  if (!render_frame_host().GetParent()) {
    return origin();
  }
  return render_frame_host().GetMainFrame()->GetLastCommittedOrigin();
}

bool AdAuctionServiceImpl::IsAuctionExpectedToFail(
    FencedFrameURLMapping::Id fenced_frame_urls_map_id,
    GlobalRenderFrameHostId render_frame_host_id,
    const base::WeakPtr<PageImpl> page_impl) {
  bool render_frame_host_impl_mismatch =
      render_frame_host_id != GetFrame()->GetGlobalId();
  bool page_impl_mismatch = page_impl.get() != &(GetFrame()->GetPage());
  bool fenced_frame_url_mapping_mismatch =
      fenced_frame_urls_map_id !=
      GetFrame()->GetPage().fenced_frame_urls_map().unique_id();

  if (!render_frame_host_impl_mismatch && !page_impl_mismatch &&
      !fenced_frame_url_mapping_mismatch) {
    // None of the RenderFrameHostImpl, PageImpl and FencedFrameUrlMapping are
    // different from the ones at the start of the auction. The auction is not
    // expected to fail.
    return false;
  }

  // Record the `LifecycleState` of the main frame. If the auction is from a
  // child frame, also record the `LifecycleState` of the child frame.
  std::string main_frame_cycle;
  std::string child_frame_cycle;

  if (GetFrame()->IsOutermostMainFrame()) {
    main_frame_cycle = RenderFrameHostImpl::LifecycleStateImplToString(
        GetFrame()->lifecycle_state());
    child_frame_cycle = "AuctionIsFromMainFrame";
  } else {
    main_frame_cycle = RenderFrameHostImpl::LifecycleStateImplToString(
        GetFrame()->GetOutermostMainFrame()->lifecycle_state());
    child_frame_cycle = RenderFrameHostImpl::LifecycleStateImplToString(
        GetFrame()->lifecycle_state());
  }

  // Set the crash key with the string describing the state.
  SCOPED_CRASH_KEY_STRING1024(
      "fledge", "on-auction-complete-state",
      base::StrCat({"RenderFrameHostImplMismatch_",
                    render_frame_host_impl_mismatch ? "true" : "false",
                    "_PageImplMismatch_", page_impl_mismatch ? "true" : "false",
                    "_FencedFrameUrlMappingMismatch_",
                    fenced_frame_url_mapping_mismatch ? "true" : "false",
                    "_MainFrame_", main_frame_cycle, "_ChildFrame_",
                    child_frame_cycle}));
  base::debug::DumpWithoutCrashing();

  return true;
}

}  // namespace content
