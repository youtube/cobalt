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

#include "cobalt/network/network_delegate.h"

#include "net/base/net_errors.h"

namespace cobalt {
namespace network {

NetworkDelegate::NetworkDelegate(const NetworkDelegate::Options& options)
    : cookie_policy_(options.cookie_policy) {}

NetworkDelegate::~NetworkDelegate() {}

int NetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {

  return net::OK;
}

int NetworkDelegate::OnBeforeSendHeaders(
    net::URLRequest* request, const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  return net::OK;
}

void NetworkDelegate::OnSendHeaders(net::URLRequest* request,
                                    const net::HttpRequestHeaders& headers) {}

int NetworkDelegate::OnHeadersReceived(
    net::URLRequest* request, const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers) {
  return net::OK;
}

void NetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                       const GURL& new_location) {}

void NetworkDelegate::OnResponseStarted(net::URLRequest* request) {}

void NetworkDelegate::OnRawBytesRead(const net::URLRequest& request,
                                     int bytes_read) {}

void NetworkDelegate::OnCompleted(net::URLRequest* request, bool started) {}

void NetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {}

void NetworkDelegate::OnPACScriptError(int line_number, const string16& error) {
}

NetworkDelegate::AuthRequiredResponse NetworkDelegate::OnAuthRequired(
    net::URLRequest* request, const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback, net::AuthCredentials* credentials) {
  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool NetworkDelegate::OnCanGetCookies(const net::URLRequest& request,
                                      const net::CookieList& cookie_list) {
  net::StaticCookiePolicy policy(ComputeCookiePolicy());
  int rv =
      policy.CanGetCookies(request.url(), request.first_party_for_cookies());
  return rv == net::OK;
}

bool NetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                     const std::string& cookie_line,
                                     net::CookieOptions* options) {
  net::StaticCookiePolicy policy(ComputeCookiePolicy());
  int rv =
      policy.CanSetCookie(request.url(), request.first_party_for_cookies());
  return rv == net::OK;
}

bool NetworkDelegate::OnCanAccessFile(const net::URLRequest& request,
                                      const FilePath& path) const {
  return true;
}

bool NetworkDelegate::OnCanThrottleRequest(
    const net::URLRequest& request) const {
  return false;
}

int NetworkDelegate::OnBeforeSocketStreamConnect(
    net::SocketStream* stream, const net::CompletionCallback& callback) {
  return net::OK;
}

void NetworkDelegate::OnRequestWaitStateChange(const net::URLRequest& request,
                                               RequestWaitState state) {}

net::StaticCookiePolicy::Type NetworkDelegate::ComputeCookiePolicy() const {
  // TODO(***REMOVED***): Allow dynamically overriding this for debugging.
  return cookie_policy_;
}

}  // namespace network
}  // namespace cobalt
