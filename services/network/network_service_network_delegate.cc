// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_network_delegate.h"

#include <string>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "components/domain_reliability/monitor.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/first_party_sets/same_party_context.h"
#include "net/url_request/referrer_policy.h"
#include "net/url_request/url_request.h"
#include "services/network/cookie_manager.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/network_service_proxy_delegate.h"
#include "services/network/pending_callback_chain.h"
#include "services/network/url_loader.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_IOS)
#include "services/network/websocket.h"
#endif

namespace network {

namespace {

const char kClearSiteDataHeader[] = "Clear-Site-Data";

}  // anonymous namespace

NetworkServiceNetworkDelegate::NetworkServiceNetworkDelegate(
    bool enable_referrers,
    bool validate_referrer_policy_on_initial_request,
    mojo::PendingRemote<mojom::ProxyErrorClient> proxy_error_client_remote,
    NetworkContext* network_context)
    : enable_referrers_(enable_referrers),
      validate_referrer_policy_on_initial_request_(
          validate_referrer_policy_on_initial_request),
      network_context_(network_context) {
  if (proxy_error_client_remote)
    proxy_error_client_.Bind(std::move(proxy_error_client_remote));
}

NetworkServiceNetworkDelegate::~NetworkServiceNetworkDelegate() = default;

void NetworkServiceNetworkDelegate::MaybeTruncateReferrer(
    net::URLRequest* const request,
    const GURL& effective_url) {
  if (!enable_referrers_) {
    request->SetReferrer(std::string());
    request->set_referrer_policy(net::ReferrerPolicy::NO_REFERRER);
    return;
  }

  if (base::FeatureList::IsEnabled(
          net::features::kCapReferrerToOriginOnCrossOrigin)) {
    if (!url::IsSameOriginWith(effective_url, GURL(request->referrer()))) {
      auto capped_referrer = url::Origin::Create(GURL(request->referrer()));
      request->SetReferrer(capped_referrer.GetURL().spec());
    }
  }
}

int NetworkServiceNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    GURL* new_url) {
  DCHECK(request);

  auto* const loader = URLLoader::ForRequest(*request);
  const GURL* effective_url = nullptr;
  if (loader && loader->new_redirect_url()) {
    DCHECK(new_url);
    *new_url = loader->new_redirect_url().value();
    effective_url = new_url;
  } else {
    effective_url = &request->url();
  }

  MaybeTruncateReferrer(request, *effective_url);

  if (!loader)
    return net::OK;

  NetworkService* network_service = network_context_->network_service();
  if (network_service) {
    loader->SetEnableReportingRawHeaders(network_service->HasRawHeadersAccess(
        loader->GetProcessId(), *effective_url));
  }
  return net::OK;
}

int NetworkServiceNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers,
    OnBeforeStartTransactionCallback callback) {
  URLLoader* url_loader = URLLoader::ForRequest(*request);
  if (url_loader)
    return url_loader->OnBeforeStartTransaction(headers, std::move(callback));

#if !BUILDFLAG(IS_IOS)
  WebSocket* web_socket = WebSocket::ForRequest(*request);
  if (web_socket)
    return web_socket->OnBeforeStartTransaction(headers, std::move(callback));
#endif  // !BUILDFLAG(IS_IOS)

  return net::OK;
}

int NetworkServiceNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    const net::IPEndPoint& endpoint,
    absl::optional<GURL>* preserve_fragment_on_redirect_url) {
  auto chain = base::MakeRefCounted<PendingCallbackChain>(std::move(callback));
  URLLoader* url_loader = URLLoader::ForRequest(*request);
  if (url_loader) {
    chain->AddResult(url_loader->OnHeadersReceived(
        chain->CreateCallback(), original_response_headers,
        override_response_headers, endpoint,
        preserve_fragment_on_redirect_url));
  }

#if !BUILDFLAG(IS_IOS)
  WebSocket* web_socket = WebSocket::ForRequest(*request);
  if (web_socket) {
    chain->AddResult(web_socket->OnHeadersReceived(
        chain->CreateCallback(), original_response_headers,
        override_response_headers, preserve_fragment_on_redirect_url));
  }
#endif  // !BUILDFLAG(IS_IOS)

  chain->AddResult(HandleClearSiteDataHeader(request, chain->CreateCallback(),
                                             original_response_headers));

  return chain->GetResult();
}

void NetworkServiceNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                                     const GURL& new_location) {
  if (network_context_->domain_reliability_monitor())
    network_context_->domain_reliability_monitor()->OnBeforeRedirect(request);
}

void NetworkServiceNetworkDelegate::OnResponseStarted(net::URLRequest* request,
                                                      int net_error) {
  ForwardProxyErrors(net_error);
}

void NetworkServiceNetworkDelegate::OnCompleted(net::URLRequest* request,
                                                bool started,
                                                int net_error) {
  // TODO(mmenke): Once the network service ships on all platforms, can move
  // this logic into URLLoader's completion method.
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (network_context_->domain_reliability_monitor()) {
    network_context_->domain_reliability_monitor()->OnCompleted(
        request, started, net_error);
  }

  ForwardProxyErrors(net_error);
}

void NetworkServiceNetworkDelegate::OnPACScriptError(
    int line_number,
    const std::u16string& error) {
  if (!proxy_error_client_)
    return;

  proxy_error_client_->OnPACScriptError(line_number, base::UTF16ToUTF8(error));
}

bool NetworkServiceNetworkDelegate::OnAnnotateAndMoveUserBlockedCookies(
    const net::URLRequest& request,
    const net::FirstPartySetMetadata& first_party_set_metadata,
    net::CookieAccessResultList& maybe_included_cookies,
    net::CookieAccessResultList& excluded_cookies) {
  if (!network_context_->cookie_manager()
           ->cookie_settings()
           .AnnotateAndMoveUserBlockedCookies(
               request.url(), request.site_for_cookies(),
               base::OptionalToPtr(request.isolation_info().top_frame_origin()),
               first_party_set_metadata, request.cookie_setting_overrides(),
               maybe_included_cookies, excluded_cookies)) {
    // CookieSettings has already moved and annotated the cookies.
    return false;
  }

  bool allowed = true;
  URLLoader* url_loader = URLLoader::ForRequest(request);
  if (url_loader) {
    allowed =
        url_loader->AllowCookies(request.url(), request.site_for_cookies());
#if !BUILDFLAG(IS_IOS)
  } else {
    WebSocket* web_socket = WebSocket::ForRequest(request);
    if (web_socket) {
      allowed = web_socket->AllowCookies(request.url());
    }
#endif  // !BUILDFLAG(IS_IOS)
  }
  if (!allowed)
    ExcludeAllCookies(net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES,
                      maybe_included_cookies, excluded_cookies);

  return allowed;
}

bool NetworkServiceNetworkDelegate::OnCanSetCookie(
    const net::URLRequest& request,
    const net::CanonicalCookie& cookie,
    net::CookieOptions* options) {
  bool allowed =
      network_context_->cookie_manager()->cookie_settings().IsCookieAccessible(
          cookie, request.url(), request.site_for_cookies(),
          request.isolation_info().top_frame_origin(),
          request.cookie_setting_overrides());
  if (!allowed)
    return false;
  // The remaining checks do not consider setting overrides since they enforce
  // explicit disablement via Android Webview APIs.
  URLLoader* url_loader = URLLoader::ForRequest(request);
  if (url_loader)
    return url_loader->AllowCookies(request.url(), request.site_for_cookies());
#if !BUILDFLAG(IS_IOS)
  WebSocket* web_socket = WebSocket::ForRequest(request);
  if (web_socket)
    return web_socket->AllowCookies(request.url());
#endif  // !BUILDFLAG(IS_IOS)
  return true;
}

net::NetworkDelegate::PrivacySetting
NetworkServiceNetworkDelegate::OnForcePrivacyMode(
    const net::URLRequest& request) const {
  return network_context_->cookie_manager()
      ->cookie_settings()
      .IsPrivacyModeEnabled(request.url(), request.site_for_cookies(),
                            request.isolation_info().top_frame_origin(),
                            request.cookie_setting_overrides());
}

bool NetworkServiceNetworkDelegate::
    OnCancelURLRequestWithPolicyViolatingReferrerHeader(
        const net::URLRequest& request,
        const GURL& target_url,
        const GURL& referrer_url) const {
  // TODO(mmenke): Once the network service has shipped on all platforms,
  // consider moving this logic into URLLoader, and removing this method from
  // NetworkDelegate. Can just have a DCHECK in URLRequest instead.
  if (!validate_referrer_policy_on_initial_request_)
    return false;

  LOG(ERROR) << "Cancelling request to " << target_url
             << " with invalid referrer " << referrer_url;
  // Record information to help debug issues like http://crbug.com/422871.
  if (target_url.SchemeIsHTTPOrHTTPS()) {
    auto referrer_policy = request.referrer_policy();
    base::debug::Alias(&referrer_policy);
    DEBUG_ALIAS_FOR_GURL(target_buf, target_url);
    DEBUG_ALIAS_FOR_GURL(referrer_buf, referrer_url);
    base::debug::DumpWithoutCrashing();
  }
  return true;
}

bool NetworkServiceNetworkDelegate::OnCanQueueReportingReport(
    const url::Origin& origin) const {
  return network_context_->cookie_manager()
      ->cookie_settings()
      .IsFullCookieAccessAllowed(origin.GetURL(),
                                 net::SiteForCookies::FromOrigin(origin),
                                 origin, net::CookieSettingOverrides());
}

void NetworkServiceNetworkDelegate::OnCanSendReportingReports(
    std::set<url::Origin> origins,
    base::OnceCallback<void(std::set<url::Origin>)> result_callback) const {
  auto* client = network_context_->client();
  if (!client) {
    origins.clear();
    std::move(result_callback).Run(std::move(origins));
    return;
  }

  if (network_context_->SkipReportingPermissionCheck()) {
    std::move(result_callback).Run(std::move(origins));
    return;
  }

  std::vector<url::Origin> origin_vector;
  base::ranges::copy(origins, std::back_inserter(origin_vector));
  client->OnCanSendReportingReports(
      origin_vector,
      base::BindOnce(
          &NetworkServiceNetworkDelegate::FinishedCanSendReportingReports,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
}

bool NetworkServiceNetworkDelegate::OnCanSetReportingClient(
    const url::Origin& origin,
    const GURL& endpoint) const {
  return network_context_->cookie_manager()
      ->cookie_settings()
      .IsFullCookieAccessAllowed(origin.GetURL(),
                                 net::SiteForCookies::FromOrigin(origin),
                                 origin, net::CookieSettingOverrides());
}

bool NetworkServiceNetworkDelegate::OnCanUseReportingClient(
    const url::Origin& origin,
    const GURL& endpoint) const {
  return network_context_->cookie_manager()
      ->cookie_settings()
      .IsFullCookieAccessAllowed(origin.GetURL(),
                                 net::SiteForCookies::FromOrigin(origin),
                                 origin, net::CookieSettingOverrides());
}

absl::optional<net::FirstPartySetsCacheFilter::MatchInfo>
NetworkServiceNetworkDelegate::
    OnGetFirstPartySetsCacheFilterMatchInfoMaybeAsync(
        const net::SchemefulSite& request_site,
        base::OnceCallback<void(net::FirstPartySetsCacheFilter::MatchInfo)>
            callback) const {
  return network_context_->first_party_sets_access_delegate()
      .GetCacheFilterMatchInfo(request_site, std::move(callback));
}

int NetworkServiceNetworkDelegate::HandleClearSiteDataHeader(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers) {
  DCHECK(request);
  if (!original_response_headers)
    return net::OK;

  URLLoader* url_loader = URLLoader::ForRequest(*request);
  if (!url_loader)
    return net::OK;

  auto* url_loader_network_observer =
      url_loader->GetURLLoaderNetworkServiceObserver();
  if (!url_loader_network_observer)
    return net::OK;

  std::string header_value;
  if (!original_response_headers->GetNormalizedHeader(kClearSiteDataHeader,
                                                      &header_value)) {
    return net::OK;
  }

  auto& cookie_settings = network_context_->cookie_manager()->cookie_settings();
  net::NetworkDelegate::PrivacySetting privacy_settings =
      cookie_settings.IsPrivacyModeEnabled(
          request->url(), request->site_for_cookies(),
          request->isolation_info().top_frame_origin(),
          request->cookie_setting_overrides());
  bool partitioned_state_allowed_only =
      privacy_settings ==
      net::NetworkDelegate::PrivacySetting::kPartitionedStateAllowedOnly;

  url_loader_network_observer->OnClearSiteData(
      request->url(), header_value, request->load_flags(),
      net::CookiePartitionKey::FromNetworkIsolationKey(
          request->isolation_info().network_isolation_key()),
      partitioned_state_allowed_only,
      base::BindOnce(&NetworkServiceNetworkDelegate::FinishedClearSiteData,
                     weak_ptr_factory_.GetWeakPtr(), request->GetWeakPtr(),
                     std::move(callback)));

  return net::ERR_IO_PENDING;
}

void NetworkServiceNetworkDelegate::FinishedClearSiteData(
    base::WeakPtr<net::URLRequest> request,
    net::CompletionOnceCallback callback) {
  if (request)
    std::move(callback).Run(net::OK);
}

void NetworkServiceNetworkDelegate::FinishedCanSendReportingReports(
    base::OnceCallback<void(std::set<url::Origin>)> result_callback,
    const std::vector<url::Origin>& origins) {
  std::set<url::Origin> origin_set(origins.begin(), origins.end());
  std::move(result_callback).Run(origin_set);
}

void NetworkServiceNetworkDelegate::ForwardProxyErrors(int net_error) {
  if (!proxy_error_client_)
    return;

  // TODO(https://crbug.com/876848): Provide justification for the currently
  // enumerated errors.
  switch (net_error) {
    case net::ERR_PROXY_AUTH_UNSUPPORTED:
    case net::ERR_PROXY_CONNECTION_FAILED:
    case net::ERR_TUNNEL_CONNECTION_FAILED:
      proxy_error_client_->OnRequestMaybeFailedDueToProxySettings(net_error);
      break;
  }
}

}  // namespace network
