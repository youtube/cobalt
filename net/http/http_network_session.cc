// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session.h"

#include <utility>

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/values.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/url_security_manager.h"
#include "net/socket/client_socket_factory.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

// TODO(mbelshe): Move the socket factories into HttpStreamFactory.
HttpNetworkSession::HttpNetworkSession(const Params& params)
    : cert_verifier_(NULL),
      proxy_service_(params.proxy_service),
      ssl_config_service_(params.ssl_config_service),
      socket_pool_manager_(params.net_log,
                           params.client_socket_factory ?
                               params.client_socket_factory :
                               ClientSocketFactory::GetDefaultFactory(),
                           params.host_resolver,
                           params.cert_verifier,
                           params.dnsrr_resolver,
                           params.dns_cert_checker,
                           params.ssl_host_info_factory,
                           params.proxy_service,
                           params.ssl_config_service),
      spdy_session_pool_(params.ssl_config_service),
      http_auth_handler_factory_(params.http_auth_handler_factory),
      network_delegate_(params.network_delegate),
      net_log_(params.net_log) {
  DCHECK(params.proxy_service);
  DCHECK(params.ssl_config_service);
}

HttpNetworkSession::~HttpNetworkSession() {
  STLDeleteElements(&response_drainers_);
  spdy_session_pool_.CloseAllSessions();
}

void HttpNetworkSession::AddResponseDrainer(HttpResponseBodyDrainer* drainer) {
  DCHECK(!ContainsKey(response_drainers_, drainer));
  response_drainers_.insert(drainer);
}

void HttpNetworkSession::RemoveResponseDrainer(
    HttpResponseBodyDrainer* drainer) {
  DCHECK(ContainsKey(response_drainers_, drainer));
  response_drainers_.erase(drainer);
}

Value* HttpNetworkSession::SpdySessionPoolInfoToValue() const {
  return spdy_session_pool_.SpdySessionPoolInfoToValue();
}

}  //  namespace net
