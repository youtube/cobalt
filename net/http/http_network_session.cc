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
#include "net/spdy/spdy_session_pool.h"

namespace net {

// TODO(mbelshe): Move the socket factories into HttpStreamFactory.
HttpNetworkSession::HttpNetworkSession(
    HostResolver* host_resolver,
    DnsRRResolver* dnsrr_resolver,
    DnsCertProvenanceChecker* dns_cert_checker,
    SSLHostInfoFactory* ssl_host_info_factory,
    ProxyService* proxy_service,
    ClientSocketFactory* client_socket_factory,
    SSLConfigService* ssl_config_service,
    SpdySessionPool* spdy_session_pool,
    HttpAuthHandlerFactory* http_auth_handler_factory,
    HttpNetworkDelegate* network_delegate,
    NetLog* net_log)
    : socket_factory_(client_socket_factory),
      host_resolver_(host_resolver),
      dnsrr_resolver_(dnsrr_resolver),
      dns_cert_checker_(dns_cert_checker),
      proxy_service_(proxy_service),
      ssl_config_service_(ssl_config_service),
      socket_pool_manager_(net_log,
                           client_socket_factory,
                           host_resolver,
                           dnsrr_resolver,
                           dns_cert_checker,
                           ssl_host_info_factory,
                           proxy_service,
                           ssl_config_service),
      spdy_session_pool_(spdy_session_pool),
      http_auth_handler_factory_(http_auth_handler_factory),
      network_delegate_(network_delegate),
      net_log_(net_log) {
  DCHECK(proxy_service);
  DCHECK(ssl_config_service);
}

HttpNetworkSession::~HttpNetworkSession() {
  STLDeleteElements(&response_drainers_);
  spdy_session_pool_->CloseAllSessions();
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
  return spdy_session_pool_->SpdySessionPoolInfoToValue();
}

}  //  namespace net
