// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/values.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_stream_factory_impl.h"
#include "net/http/url_security_manager.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_pool_manager_impl.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

// TODO(mbelshe): Move the socket factories into HttpStreamFactory.
HttpNetworkSession::HttpNetworkSession(const Params& params)
    : net_log_(params.net_log),
      network_delegate_(params.network_delegate),
      http_server_properties_(params.http_server_properties),
      cert_verifier_(params.cert_verifier),
      http_auth_handler_factory_(params.http_auth_handler_factory),
      proxy_service_(params.proxy_service),
      ssl_config_service_(params.ssl_config_service),
      socket_pool_manager_(
          new ClientSocketPoolManagerImpl(
              params.net_log,
              params.client_socket_factory ?
              params.client_socket_factory :
              ClientSocketFactory::GetDefaultFactory(),
              params.host_resolver,
              params.cert_verifier,
              params.origin_bound_cert_service,
              params.dnsrr_resolver,
              params.dns_cert_checker,
              params.ssl_host_info_factory,
              params.proxy_service,
              params.ssl_config_service)),
      spdy_session_pool_(params.host_resolver,
                         params.ssl_config_service,
                         params.http_server_properties),
      ALLOW_THIS_IN_INITIALIZER_LIST(http_stream_factory_(
          new HttpStreamFactoryImpl(this))) {
  DCHECK(proxy_service_);
  DCHECK(ssl_config_service_);
  CHECK(http_server_properties_);
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

SOCKSClientSocketPool* HttpNetworkSession::GetSocketPoolForSOCKSProxy(
    const HostPortPair& socks_proxy) {
  return socket_pool_manager_->GetSocketPoolForSOCKSProxy(socks_proxy);
}

HttpProxyClientSocketPool* HttpNetworkSession::GetSocketPoolForHTTPProxy(
    const HostPortPair& http_proxy) {
  return socket_pool_manager_->GetSocketPoolForHTTPProxy(http_proxy);
}

SSLClientSocketPool* HttpNetworkSession::GetSocketPoolForSSLWithProxy(
    const HostPortPair& proxy_server) {
  return socket_pool_manager_->GetSocketPoolForSSLWithProxy(proxy_server);
}

Value* HttpNetworkSession::SocketPoolInfoToValue() const {
  return socket_pool_manager_->SocketPoolInfoToValue();
}

Value* HttpNetworkSession::SpdySessionPoolInfoToValue() const {
  return spdy_session_pool_.SpdySessionPoolInfoToValue();
}

void HttpNetworkSession::CloseAllConnections() {
  socket_pool_manager_->FlushSocketPools();
  spdy_session_pool_.CloseCurrentSessions();
}

void HttpNetworkSession::CloseIdleConnections() {
  socket_pool_manager_->CloseIdleSockets();
  spdy_session_pool_.CloseIdleSessions();
}

}  //  namespace net
