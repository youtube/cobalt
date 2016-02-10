/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_NETWORK_NETWORK_DELEGATE_H_
#define COBALT_NETWORK_NETWORK_DELEGATE_H_

#include <string>

#include "googleurl/src/gurl.h"
#include "net/base/static_cookie_policy.h"
#include "net/url_request/url_request.h"

namespace cobalt {
namespace network {
// A NetworkDelegate receives callbacks when network events occur.
// Each override can specify custom behavior or just add additional logging.
// We do nothing for most events, but our network delegate
// is used to specify a cookie policy.
class NetworkDelegate : public net::NetworkDelegate {
 public:
  NetworkDelegate(net::StaticCookiePolicy::Type cookie_policy,
                  bool require_https);
  ~NetworkDelegate() OVERRIDE;

  // For debugging, we allow blocking all cookies.
  void set_cookies_enabled(bool enabled) { cookies_enabled_ = enabled; }
  bool cookies_enabled() const { return cookies_enabled_; }

 protected:
  // net::NetworkDelegate implementation.
  int OnBeforeURLRequest(net::URLRequest* request,
                         const net::CompletionCallback& callback,
                         GURL* new_url) OVERRIDE;
  int OnBeforeSendHeaders(net::URLRequest* request,
                          const net::CompletionCallback& callback,
                          net::HttpRequestHeaders* headers) OVERRIDE;
  void OnSendHeaders(net::URLRequest* request,
                     const net::HttpRequestHeaders& headers) OVERRIDE;
  int OnHeadersReceived(
      net::URLRequest* request, const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers)
      OVERRIDE;
  void OnBeforeRedirect(net::URLRequest* request,
                        const GURL& new_location) OVERRIDE;
  void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  void OnRawBytesRead(const net::URLRequest& request, int bytes_read) OVERRIDE;
  void OnCompleted(net::URLRequest* request, bool started) OVERRIDE;
  void OnURLRequestDestroyed(net::URLRequest* request) OVERRIDE;

  void OnPACScriptError(int line_number, const string16& error) OVERRIDE;
  AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request, const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback, net::AuthCredentials* credentials) OVERRIDE;
  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list) OVERRIDE;
  bool OnCanSetCookie(const net::URLRequest& request,
                      const std::string& cookie_line,
                      net::CookieOptions* options) OVERRIDE;
  bool OnCanAccessFile(const net::URLRequest& request,
                       const FilePath& path) const OVERRIDE;
  bool OnCanThrottleRequest(const net::URLRequest& request) const OVERRIDE;

  int OnBeforeSocketStreamConnect(
      net::SocketStream* stream,
      const net::CompletionCallback& callback) OVERRIDE;

  void OnRequestWaitStateChange(const net::URLRequest& request,
                                RequestWaitState state) OVERRIDE;

  net::StaticCookiePolicy::Type ComputeCookiePolicy() const;

 private:
  net::StaticCookiePolicy::Type cookie_policy_;
  bool cookies_enabled_;
  bool require_https_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDelegate);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_NETWORK_DELEGATE_H_
