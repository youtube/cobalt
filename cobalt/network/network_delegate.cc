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

NetworkDelegate::NetworkDelegate(net::StaticCookiePolicy::Type cookie_policy,
                                 bool require_https)
    : cookie_policy_(cookie_policy),
      cookies_enabled_(true),
      require_https_(require_https) {}

NetworkDelegate::~NetworkDelegate() {}

int NetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  UNREFERENCED_PARAMETER(callback);
  UNREFERENCED_PARAMETER(new_url);

#if defined(COBALT_FORCE_HTTPS)
  const bool require_https = true;
#else
  bool require_https = require_https_;
#endif

  if (!require_https) {
    return net::OK;
  } else if (request->url().SchemeIsSecure() ||
             request->url().SchemeIs("data")) {
    return net::OK;
  } else {
    return net::ERR_DISALLOWED_URL_SCHEME;
  }
}

int NetworkDelegate::OnBeforeSendHeaders(
    net::URLRequest* request, const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(callback);
  UNREFERENCED_PARAMETER(headers);
  return net::OK;
}

void NetworkDelegate::OnSendHeaders(net::URLRequest* request,
                                    const net::HttpRequestHeaders& headers) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(headers);
}

int NetworkDelegate::OnHeadersReceived(
    net::URLRequest* request, const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(callback);
  UNREFERENCED_PARAMETER(original_response_headers);
  UNREFERENCED_PARAMETER(override_response_headers);
  return net::OK;
}

void NetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                       const GURL& new_location) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(new_location);
}

void NetworkDelegate::OnResponseStarted(net::URLRequest* request) {
  UNREFERENCED_PARAMETER(request);
}

void NetworkDelegate::OnRawBytesRead(const net::URLRequest& request,
                                     int bytes_read) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(bytes_read);
}

void NetworkDelegate::OnCompleted(net::URLRequest* request, bool started) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(started);
}

void NetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
  UNREFERENCED_PARAMETER(request);
}

void NetworkDelegate::OnPACScriptError(int line_number, const string16& error) {
  UNREFERENCED_PARAMETER(line_number);
  UNREFERENCED_PARAMETER(error);
}

NetworkDelegate::AuthRequiredResponse NetworkDelegate::OnAuthRequired(
    net::URLRequest* request, const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback, net::AuthCredentials* credentials) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(auth_info);
  UNREFERENCED_PARAMETER(callback);
  UNREFERENCED_PARAMETER(credentials);
  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool NetworkDelegate::OnCanGetCookies(const net::URLRequest& request,
                                      const net::CookieList& cookie_list) {
  UNREFERENCED_PARAMETER(cookie_list);
  net::StaticCookiePolicy policy(ComputeCookiePolicy());
  int rv =
      policy.CanGetCookies(request.url(), request.first_party_for_cookies());
  return rv == net::OK;
}

bool NetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                     const std::string& cookie_line,
                                     net::CookieOptions* options) {
  UNREFERENCED_PARAMETER(cookie_line);
  UNREFERENCED_PARAMETER(options);
  net::StaticCookiePolicy policy(ComputeCookiePolicy());
  int rv =
      policy.CanSetCookie(request.url(), request.first_party_for_cookies());
  return rv == net::OK;
}

bool NetworkDelegate::OnCanAccessFile(const net::URLRequest& request,
                                      const FilePath& path) const {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(path);
  return true;
}

bool NetworkDelegate::OnCanThrottleRequest(
    const net::URLRequest& request) const {
  UNREFERENCED_PARAMETER(request);
  return false;
}

int NetworkDelegate::OnBeforeSocketStreamConnect(
    net::SocketStream* stream, const net::CompletionCallback& callback) {
  UNREFERENCED_PARAMETER(stream);
  UNREFERENCED_PARAMETER(callback);
  return net::OK;
}

void NetworkDelegate::OnRequestWaitStateChange(const net::URLRequest& request,
                                               RequestWaitState state) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(state);
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
