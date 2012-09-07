// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

namespace {

net::ClientSocketPoolManager* CreateSocketPoolManager(
    net::HttpNetworkSession::SocketPoolType pool_type,
    const net::HttpNetworkSession::Params& params) {
  // TODO(yutak): Differentiate WebSocket pool manager and allow more
  // simultaneous connections for WebSockets.
  return new net::ClientSocketPoolManagerImpl(
      params.net_log,
      params.client_socket_factory ?
      params.client_socket_factory :
      net::ClientSocketFactory::GetDefaultFactory(),
      params.host_resolver,
      params.cert_verifier,
      params.server_bound_cert_service,
      params.transport_security_state,
      params.ssl_session_cache_shard,
      params.proxy_service,
      params.ssl_config_service,
      pool_type);
}

}  // unnamed namespace

namespace net {

HttpNetworkSession::Params::Params()
    : client_socket_factory(NULL),
      host_resolver(NULL),
      cert_verifier(NULL),
      server_bound_cert_service(NULL),
      transport_security_state(NULL),
      proxy_service(NULL),
      ssl_config_service(NULL),
      http_auth_handler_factory(NULL),
      network_delegate(NULL),
      http_server_properties(NULL),
      net_log(NULL),
      host_mapping_rules(NULL),
      force_http_pipelining(false),
      ignore_certificate_errors(false),
      http_pipelining_enabled(false),
      testing_fixed_http_port(0),
      testing_fixed_https_port(0) {}

// TODO(mbelshe): Move the socket factories into HttpStreamFactory.
HttpNetworkSession::HttpNetworkSession(const Params& params)
    : net_log_(params.net_log),
      network_delegate_(params.network_delegate),
      http_server_properties_(params.http_server_properties),
      cert_verifier_(params.cert_verifier),
      http_auth_handler_factory_(params.http_auth_handler_factory),
      force_http_pipelining_(params.force_http_pipelining),
      proxy_service_(params.proxy_service),
      ssl_config_service_(params.ssl_config_service),
      normal_socket_pool_manager_(
          CreateSocketPoolManager(NORMAL_SOCKET_POOL, params)),
      websocket_socket_pool_manager_(
          CreateSocketPoolManager(WEBSOCKET_SOCKET_POOL, params)),
      spdy_session_pool_(params.host_resolver,
                         params.ssl_config_service,
                         params.http_server_properties,
                         params.trusted_spdy_proxy),
      ALLOW_THIS_IN_INITIALIZER_LIST(http_stream_factory_(
          new HttpStreamFactoryImpl(this))),
      params_(params) {
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

TransportClientSocketPool* HttpNetworkSession::GetTransportSocketPool(
    SocketPoolType pool_type) {
  return GetSocketPoolManager(pool_type)->GetTransportSocketPool();
}

SSLClientSocketPool* HttpNetworkSession::GetSSLSocketPool(
    SocketPoolType pool_type) {
  return GetSocketPoolManager(pool_type)->GetSSLSocketPool();
}

SOCKSClientSocketPool* HttpNetworkSession::GetSocketPoolForSOCKSProxy(
    SocketPoolType pool_type,
    const HostPortPair& socks_proxy) {
  return GetSocketPoolManager(pool_type)->GetSocketPoolForSOCKSProxy(
      socks_proxy);
}

HttpProxyClientSocketPool* HttpNetworkSession::GetSocketPoolForHTTPProxy(
    SocketPoolType pool_type,
    const HostPortPair& http_proxy) {
  return GetSocketPoolManager(pool_type)->GetSocketPoolForHTTPProxy(http_proxy);
}

SSLClientSocketPool* HttpNetworkSession::GetSocketPoolForSSLWithProxy(
    SocketPoolType pool_type,
    const HostPortPair& proxy_server) {
  return GetSocketPoolManager(pool_type)->GetSocketPoolForSSLWithProxy(
      proxy_server);
}

Value* HttpNetworkSession::SocketPoolInfoToValue() const {
  // TODO(yutak): Should merge values from normal pools and WebSocket pools.
  return normal_socket_pool_manager_->SocketPoolInfoToValue();
}

Value* HttpNetworkSession::SpdySessionPoolInfoToValue() const {
  return spdy_session_pool_.SpdySessionPoolInfoToValue();
}

void HttpNetworkSession::CloseAllConnections() {
  normal_socket_pool_manager_->FlushSocketPools();
  websocket_socket_pool_manager_->FlushSocketPools();
  spdy_session_pool_.CloseCurrentSessions();
}

void HttpNetworkSession::CloseIdleConnections() {
  normal_socket_pool_manager_->CloseIdleSockets();
  websocket_socket_pool_manager_->CloseIdleSockets();
  spdy_session_pool_.CloseIdleSessions();
}

ClientSocketPoolManager* HttpNetworkSession::GetSocketPoolManager(
    SocketPoolType pool_type) {
  switch (pool_type) {
    case NORMAL_SOCKET_POOL:
      return normal_socket_pool_manager_.get();
    case WEBSOCKET_SOCKET_POOL:
      return websocket_socket_pool_manager_.get();
    default:
      NOTREACHED();
      break;
  }
  return NULL;
}

}  //  namespace net
