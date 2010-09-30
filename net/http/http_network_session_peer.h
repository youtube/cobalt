// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_
#pragma once

#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket_pool.h"

namespace net {

class HttpNetworkSessionPeer {
 public:
  explicit HttpNetworkSessionPeer(
      const scoped_refptr<HttpNetworkSession>& session)
      : session_(session) {}

  void SetTCPSocketPool(TCPClientSocketPool* pool) {
    session_->socket_pool_manager_.tcp_socket_pool_.reset(pool);
  }

  void SetSocketPoolForSOCKSProxy(
      const HostPortPair& socks_proxy,
      SOCKSClientSocketPool* pool) {
    ClientSocketPoolManager* socket_pool_manager =
        &session_->socket_pool_manager_;

    // Call through the public interface to force initialization of the
    // wrapped socket pools.
    delete socket_pool_manager->GetSocketPoolForSOCKSProxy(socks_proxy);
    socket_pool_manager->socks_socket_pools_[socks_proxy] = pool;
  }

  void SetSocketPoolForHTTPProxy(
      const HostPortPair& http_proxy,
      HttpProxyClientSocketPool* pool) {
    ClientSocketPoolManager* socket_pool_manager =
        &session_->socket_pool_manager_;

    // Call through the public interface to force initialization of the
    // wrapped socket pools.
    delete socket_pool_manager->GetSocketPoolForHTTPProxy(http_proxy);
    socket_pool_manager->http_proxy_socket_pools_[http_proxy] = pool;
  }

  void SetSSLSocketPool(SSLClientSocketPool* pool) {
    session_->socket_pool_manager_.ssl_socket_pool_.reset(pool);
  }

  void SetSocketPoolForSSLWithProxy(
      const HostPortPair& proxy_host,
      SSLClientSocketPool* pool) {
    ClientSocketPoolManager* socket_pool_manager =
        &session_->socket_pool_manager_;

    // Call through the public interface to force initialization of the
    // wrapped socket pools.
    delete socket_pool_manager->GetSocketPoolForSSLWithProxy(proxy_host);
    socket_pool_manager->ssl_socket_pools_for_proxies_[proxy_host] = pool;
  }

  void SetProxyService(ProxyService* proxy_service) {
    session_->proxy_service_ = proxy_service;
  }

 private:
  const scoped_refptr<HttpNetworkSession> session_;

  DISALLOW_COPY_AND_ASSIGN(HttpNetworkSessionPeer);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_
