// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/proxy_client_socket.h"

#include "base/stringprintf.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"

namespace net {

// static
void ProxyClientSocket::BuildTunnelRequest(
    const HttpRequestInfo& request_info,
    const HttpRequestHeaders& auth_headers,
    const HostPortPair& endpoint,
    std::string* request_line,
    HttpRequestHeaders* request_headers) {
  // RFC 2616 Section 9 says the Host request-header field MUST accompany all
  // HTTP/1.1 requests.  Add "Proxy-Connection: keep-alive" for compat with
  // HTTP/1.0 proxies such as Squid (required for NTLM authentication).
  *request_line = StringPrintf(
      "CONNECT %s HTTP/1.1\r\n", endpoint.ToString().c_str());
  request_headers->SetHeader(HttpRequestHeaders::kHost,
                             GetHostAndOptionalPort(request_info.url));
  request_headers->SetHeader(HttpRequestHeaders::kProxyConnection,
                             "keep-alive");

  std::string user_agent;
  if (request_info.extra_headers.GetHeader(HttpRequestHeaders::kUserAgent,
                                            &user_agent))
    request_headers->SetHeader(HttpRequestHeaders::kUserAgent, user_agent);

  request_headers->MergeFrom(auth_headers);
}

// static
int ProxyClientSocket::HandleProxyAuthChallenge(HttpAuthController* auth,
                                                HttpResponseInfo* response,
                                                const BoundNetLog& net_log) {
  DCHECK(response->headers);
  int rv = auth->HandleAuthChallenge(response->headers, false, true, net_log);
  response->auth_challenge = auth->auth_info();
  if (rv == OK)
    return ERR_PROXY_AUTH_REQUESTED;
  return rv;
}

}  // namespace net
