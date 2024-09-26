// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/reduce_accept_language/reduce_accept_language_throttle.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "content/browser/reduce_accept_language/reduce_accept_language_utils.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/variants_header.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

namespace {

using ::network::mojom::VariantsHeaderPtr;

// Metrics on the count of requests restarted or the reason why not restarted
// when reducing accept-language HTTP header. These values are persisted to
// logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class AcceptLanguageNegotiationRestart {
  kNavigationStarted = 0,
  kVariantsAndContentLanguageHeaderPresent = 1,
  kServiceWorkerPreloadRequest = 2,
  kNavigationRestarted = 3,
  kMaxValue = kNavigationRestarted,
};

// Logging the metric related to reduce accept language header and
// corresponding restart metric when language negotiation happens.
void LogAcceptLanguageStatus(AcceptLanguageNegotiationRestart status) {
  base::UmaHistogramEnumeration(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart", status);
}

}  // namespace

ReduceAcceptLanguageThrottle::ReduceAcceptLanguageThrottle(
    ReduceAcceptLanguageControllerDelegate& accept_language_delegate)
    : accept_language_delegate_(accept_language_delegate) {
  DCHECK(
      base::FeatureList::IsEnabled(network::features::kReduceAcceptLanguage) ||
      base::FeatureList::IsEnabled(
          network::features::kReduceAcceptLanguageOriginTrial));
  LogAcceptLanguageStatus(AcceptLanguageNegotiationRestart::kNavigationStarted);
}

ReduceAcceptLanguageThrottle::~ReduceAcceptLanguageThrottle() = default;

void ReduceAcceptLanguageThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  last_request_url_ = request->url;
  initial_request_headers_ = request->headers;
}

void ReduceAcceptLanguageThrottle::BeforeWillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  // For redirect case, checking if a redirect response should result in a
  // restart of the last requested URL with a better negotiation language,
  // rather than following the redirect.
  //
  // Suppose origin A returns a response redirecting to origin B,
  // `last_request_url_` will be A, and `response_head` contains
  // Content-Language and Variants headers suggesting whether we should follow
  // the redirect response. If the response shows it supports one of user's
  // other preferred language and needs to restart. We will restart the request
  // to A with the new preferred language and expect A responses a different
  // redirect. For detail example, see
  // https://github.com/Tanych/accept-language/issues/3.
  MaybeRestartWithLanguageNegotiation(response_head);
  // Update the url with the redirect new url to make sure last_request_url_
  // with be the response_url.
  last_request_url_ = redirect_info->new_url;
}

void ReduceAcceptLanguageThrottle::BeforeWillProcessResponse(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head,
    bool* defer) {
  DCHECK_EQ(response_url, last_request_url_);
  MaybeRestartWithLanguageNegotiation(response_head);
}

void ReduceAcceptLanguageThrottle::MaybeRestartWithLanguageNegotiation(
    const network::mojom::URLResponseHead& response_head) {
  // For responses that don't contains content-language and variants header, we
  // skip language negotiation for them since we don't know whether we can get a
  // better representation.
  if (!response_head.parsed_headers ||
      !response_head.parsed_headers->content_language ||
      !response_head.parsed_headers->variants_headers) {
    return;
  }

  LogAcceptLanguageStatus(AcceptLanguageNegotiationRestart::
                              kVariantsAndContentLanguageHeaderPresent);

  // Skip restart when it's a service worker navigation preload request,
  // otherwise request for the same origin can't guarantee restart at most once.
  // All URLLoaderThrottles instances get recreated and `restarted_origins_` get
  // reset when requests are the service worker preload requests. This could
  // lead to browsers resending too many requests if
  // `ParseAndPersistAcceptLanguageForNavigation` returns need to restart.
  if (response_head.did_service_worker_navigation_preload) {
    LogAcceptLanguageStatus(
        AcceptLanguageNegotiationRestart::kServiceWorkerPreloadRequest);
    return;
  }

  url::Origin last_request_origin = url::Origin::Create(last_request_url_);
  if (!ReduceAcceptLanguageUtils::OriginCanReduceAcceptLanguage(
          last_request_origin)) {
    return;
  }

  ReduceAcceptLanguageUtils reduce_language_utils(*accept_language_delegate_);
  // Skip if kReduceAcceptLanguage feature is disabled and response header has
  // no valid origin trial token.
  const bool is_origin_trial_enabled =
      ReduceAcceptLanguageUtils::IsReduceAcceptLanguageEnabledForOrigin(
          url::Origin::Create(last_request_url_), response_head.headers.get());
  if (!base::FeatureList::IsEnabled(network::features::kReduceAcceptLanguage) &&
      !is_origin_trial_enabled) {
    return;
  }

  // Only restart once per-Origin (per navigation).
  if (!restarted_origins_.insert(last_request_origin).second)
    return;

  bool need_restart =
      reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
          last_request_origin, initial_request_headers_,
          response_head.parsed_headers, is_origin_trial_enabled);

  // Only restart if the initial accept language doesn't match content language.
  if (need_restart) {
    LogAcceptLanguageStatus(
        AcceptLanguageNegotiationRestart::kNavigationRestarted);
    // RestartWithURLResetAndFlags will restart from the original requested URL.
    // Ideally, we expect only restart last requested URL to avoid unnecessary
    // restarts starting from the original request URL. However, for cross
    // origin redirects, it won't pass the SiteForCookies equivalent check on
    // URLLoader when using RestartWithFlags.
    delegate_->RestartWithURLResetAndFlags(/*additional_load_flags=*/0);
    return;
  }
}

}  // namespace content
