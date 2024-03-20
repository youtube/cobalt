// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/network/network_delegate.h"

#include <set>
#include <utility>

#include "base/logging.h"
#include "cobalt/network/local_network.h"
#include "cobalt/network/socket_address_parser.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"

namespace cobalt {
namespace network {

NetworkDelegate::NetworkDelegate(net::StaticCookiePolicy::Type cookie_policy,
                                 network::HTTPSRequirement https_requirement,
                                 network::CORSPolicy cors_policy)
    : cookie_policy_(cookie_policy),
      cors_policy_(cors_policy),
      cookies_enabled_(true),
      https_requirement_(https_requirement) {}

NetworkDelegate::~NetworkDelegate() {}

int NetworkDelegate::OnBeforeURLRequest(net::URLRequest* request,
                                        net::CompletionOnceCallback callback,
                                        GURL* new_url) {
  const GURL& url = request->url();
  if (url.SchemeIsCryptographic() || url.SchemeIsFileSystem() ||
      url.SchemeIs("data")) {
    return net::OK;
  } else if (https_requirement_ == kHTTPSOptional) {
    DLOG(WARNING)
        << "Pages must be served over secure scheme, otherwise it will fail to "
           "load in production builds of Cobalt.";
    return net::OK;
  }

  if (!url.is_valid() || url.is_empty()) {
    return net::ERR_INVALID_ARGUMENT;
  }

  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();

  if (!url.has_host() || !parsed.host.is_valid() ||
      !parsed.host.is_nonempty()) {
    return net::ERR_INVALID_ARGUMENT;
  }

  const std::string& valid_spec = url.possibly_invalid_spec();
  const char* valid_spec_cstr = valid_spec.c_str();

  std::string host;
  // This will be our host string if we are not using IPV6.
  host.append(valid_spec_cstr + parsed.host.begin,
              valid_spec_cstr + parsed.host.begin + parsed.host.len);
  if (SbSocketIsIpv6Supported()) host = url.HostNoBrackets();
  if (net::HostStringIsLocalhost(host)) {
    return net::OK;
  }

  SbSocketAddress destination;
  // Note that ParseSocketAddress will only pass if host is a numeric IP.
  if (!cobalt::network::ParseSocketAddress(valid_spec_cstr, parsed.host,
                                           &destination)) {
    return net::ERR_INVALID_ARGUMENT;
  }

  if (IsIPInPrivateRange(destination) || IsIPInLocalNetwork(destination)) {
    return net::OK;
  }

  return net::ERR_DISALLOWED_URL_SCHEME;
}

int NetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request, const net::HttpRequestHeaders& headers,
    OnBeforeStartTransactionCallback callback) {
  return net::OK;
}

int NetworkDelegate::OnHeadersReceived(
    net::URLRequest* request, net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    const net::IPEndPoint& remote_endpoint,
    absl::optional<GURL>* preserve_fragment_on_redirect_url) {
  return net::OK;
}

void NetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                       const GURL& new_location) {}

void NetworkDelegate::OnResponseStarted(net::URLRequest* request,
                                        int net_error) {}

void NetworkDelegate::OnCompleted(net::URLRequest* request, bool started,
                                  int net_error) {}

void NetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {}

void NetworkDelegate::OnPACScriptError(int line_number,
                                       const std::u16string& error) {}


bool NetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                     const net::CanonicalCookie& cookie,
                                     net::CookieOptions* options) {
  net::StaticCookiePolicy policy(ComputeCookiePolicy());
  int rv = policy.CanAccessCookies(request.url(), request.site_for_cookies());
  return rv == net::OK;
}

bool NetworkDelegate::OnCancelURLRequestWithPolicyViolatingReferrerHeader(
    const net::URLRequest& request, const GURL& target_url,
    const GURL& referrer_url) const {
  return true;
}

bool NetworkDelegate::OnCanQueueReportingReport(
    const url::Origin& origin) const {
  return true;
}

void NetworkDelegate::OnCanSendReportingReports(
    std::set<url::Origin> origins,
    base::OnceCallback<void(std::set<url::Origin>)> result_callback) const {
  std::move(result_callback).Run(std::move(origins));
}

bool NetworkDelegate::OnCanSetReportingClient(const url::Origin& origin,
                                              const GURL& endpoint) const {
  return true;
}

bool NetworkDelegate::OnCanUseReportingClient(const url::Origin& origin,
                                              const GURL& endpoint) const {
  return true;
}

net::StaticCookiePolicy::Type NetworkDelegate::ComputeCookiePolicy() const {
  if (cookies_enabled_) {
    return cookie_policy_;
  } else {
    return net::StaticCookiePolicy::BLOCK_ALL_COOKIES;
  }
}
}  // namespace network
}  // namespace cobalt
