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

#ifndef COBALT_NETWORK_NETWORK_DELEGATE_H_
#define COBALT_NETWORK_NETWORK_DELEGATE_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "net/base/network_delegate.h"
#include "net/cookies/static_cookie_policy.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace cobalt {
namespace network {

// Used to tell if HTTPS scheme has to be used.
enum HTTPSRequirement {
  kHTTPSRequired,
  kHTTPSOptional,
};

enum CORSPolicy {
  kCORSRequired,
  kCORSOptional,
};

// A NetworkDelegate receives callbacks when network events occur.
// Each override can specify custom behavior or just add additional logging.
// We do nothing for most events, but our network delegate
// is used to specify a cookie policy.
class NetworkDelegate : public net::NetworkDelegate {
 public:
  NetworkDelegate(net::StaticCookiePolicy::Type cookie_policy,
                  network::HTTPSRequirement https_requirement,
                  network::CORSPolicy cors_policy);
  ~NetworkDelegate() override;

  // For debugging, we allow blocking all cookies.
  void set_cookies_enabled(bool enabled) { cookies_enabled_ = enabled; }
  bool cookies_enabled() const { return cookies_enabled_; }

  network::CORSPolicy cors_policy() const { return cors_policy_; }

 protected:
  // net::NetworkDelegate implementation.
  int OnBeforeURLRequest(net::URLRequest* request,
                         net::CompletionOnceCallback callback,
                         GURL* new_url) override;
  int OnBeforeStartTransaction(
      net::URLRequest* request, const net::HttpRequestHeaders& headers,
      OnBeforeStartTransactionCallback callback) override;
#ifndef USE_HACKY_COBALT_CHANGES
  void OnBeforeSendHeaders(net::URLRequest* request,
                           const net::ProxyInfo& proxy_info,
                           const net::ProxyRetryInfoMap& proxy_retry_info,
                           net::HttpRequestHeaders* headers) override;
  void OnStartTransaction(net::URLRequest* request,
                          const net::HttpRequestHeaders& headers) override;
#endif
  int OnHeadersReceived(
      net::URLRequest* request, net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      const net::IPEndPoint& remote_endpoint,
      absl::optional<GURL>* preserve_fragment_on_redirect_url) override;
  void OnBeforeRedirect(net::URLRequest* request,
                        const GURL& new_location) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
#ifndef USE_HACKY_COBALT_CHANGES
  void OnNetworkBytesReceived(net::URLRequest* request,
                              int64_t bytes_received) override;
  void OnNetworkBytesSent(net::URLRequest* request,
                          int64_t bytes_sent) override;
#endif
  void OnCompleted(net::URLRequest* request, bool started,
                   int net_error) override;
  void OnURLRequestDestroyed(net::URLRequest* request) override;

  void OnPACScriptError(int line_number, const std::u16string& error) override;
#ifndef USE_HACKY_COBALT_CHANGES
  net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request, const net::AuthChallengeInfo& auth_info,
      AuthCallback callback, net::AuthCredentials* credentials) override;
  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list,
                       bool allowed_from_caller) override;
#endif
  bool OnCanSetCookie(const net::URLRequest& request,
                      const net::CanonicalCookie& cookie,
                      net::CookieOptions* options) override;
#ifndef USE_HACKY_COBALT_CHANGES
  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override;
  bool OnCanEnablePrivacyMode(const GURL& url,
                              const GURL& site_for_cookies) const override;
  bool OnAreExperimentalCookieFeaturesEnabled() const override;
#endif
  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const net::URLRequest& request, const GURL& target_url,
      const GURL& referrer_url) const override;

  // Background knowledge for the following functions taken from Chromium's
  // README in net/reporting/:
  // Reporting is a central mechanism for sending out-of-band error reports
  // to origins from various other components (e.g. HTTP Public Key Pinning,
  // Interventions, or Content Security Policy could potentially use it).
  bool OnCanQueueReportingReport(const url::Origin& origin) const override;

  void OnCanSendReportingReports(std::set<url::Origin> origins,
                                 base::OnceCallback<void(std::set<url::Origin>)>
                                     result_callback) const override;

  bool OnCanSetReportingClient(const url::Origin& origin,
                               const GURL& endpoint) const override;

  bool OnCanUseReportingClient(const url::Origin& origin,
                               const GURL& endpoint) const override;

  net::StaticCookiePolicy::Type ComputeCookiePolicy() const;

 private:
  net::StaticCookiePolicy::Type cookie_policy_;
  network::CORSPolicy cors_policy_;
  bool cookies_enabled_;
  network::HTTPSRequirement https_requirement_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDelegate);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_NETWORK_DELEGATE_H_
