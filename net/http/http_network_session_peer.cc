// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session_peer.h"

#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/tcp_client_socket_pool.h"

namespace net {

HttpNetworkSessionPeer::HttpNetworkSessionPeer(
    const scoped_refptr<HttpNetworkSession>& session)
    : session_(session) {}

HttpNetworkSessionPeer::~HttpNetworkSessionPeer() {}

void HttpNetworkSessionPeer::SetTCPSocketPool(TCPClientSocketPool* pool) {
  session_->socket_pool_manager_.tcp_socket_pool_.reset(pool);
}

void HttpNetworkSessionPeer::SetSocketPoolForSOCKSProxy(
    const HostPortPair& socks_proxy,
    SOCKSClientSocketPool* pool) {
  ClientSocketPoolManager* socket_pool_manager =
      &session_->socket_pool_manager_;

  // Call through the public interface to force initialization of the
  // wrapped socket pools.
  delete socket_pool_manager->GetSocketPoolForSOCKSProxy(socks_proxy);
  socket_pool_manager->socks_socket_pools_[socks_proxy] = pool;
}

void HttpNetworkSessionPeer::SetSocketPoolForHTTPProxy(
    const HostPortPair& http_proxy,
    HttpProxyClientSocketPool* pool) {
  ClientSocketPoolManager* socket_pool_manager =
      &session_->socket_pool_manager_;

  // Call through the public interface to force initialization of the
  // wrapped socket pools.
  delete socket_pool_manager->GetSocketPoolForHTTPProxy(http_proxy);
  socket_pool_manager->http_proxy_socket_pools_[http_proxy] = pool;
}

void HttpNetworkSessionPeer::SetSSLSocketPool(SSLClientSocketPool* pool) {
  session_->socket_pool_manager_.ssl_socket_pool_.reset(pool);
}

void HttpNetworkSessionPeer::SetSocketPoolForSSLWithProxy(
    const HostPortPair& proxy_host,
    SSLClientSocketPool* pool) {
  ClientSocketPoolManager* socket_pool_manager =
      &session_->socket_pool_manager_;

  // Call through the public interface to force initialization of the
  // wrapped socket pools.
  delete socket_pool_manager->GetSocketPoolForSSLWithProxy(proxy_host);
  socket_pool_manager->ssl_socket_pools_for_proxies_[proxy_host] = pool;
}

void HttpNetworkSessionPeer::SetProxyService(ProxyService* proxy_service) {
  session_->proxy_service_ = proxy_service;
}

}  // namespace net
