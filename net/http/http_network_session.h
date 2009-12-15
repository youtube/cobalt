// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_H_

#include "base/ref_counted.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_client_auth_cache.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_auth_cache.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/tcp_client_socket_pool.h"

namespace net {

class ClientSocketFactory;
class FlipSessionPool;

// This class holds session objects used by HttpNetworkTransaction objects.
class HttpNetworkSession : public base::RefCounted<HttpNetworkSession> {
 public:
  HttpNetworkSession(HostResolver* host_resolver, ProxyService* proxy_service,
                     ClientSocketFactory* client_socket_factory,
                     SSLConfigService* ssl_config_service,
                     FlipSessionPool* flip_session_pool);

  HttpAuthCache* auth_cache() { return &auth_cache_; }
  SSLClientAuthCache* ssl_client_auth_cache() {
    return &ssl_client_auth_cache_;
  }

  // TCP sockets come from the tcp_socket_pool().
  TCPClientSocketPool* tcp_socket_pool() { return tcp_socket_pool_; }
  // SSL sockets come frmo the socket_factory().
  ClientSocketFactory* socket_factory() { return socket_factory_; }
  HostResolver* host_resolver() { return host_resolver_; }
  ProxyService* proxy_service() { return proxy_service_; }
  SSLConfigService* ssl_config_service() { return ssl_config_service_; }
  const scoped_refptr<FlipSessionPool>& flip_session_pool() {
    return flip_session_pool_;
  }

  static void set_max_sockets_per_group(int socket_count);

  static uint16 fixed_http_port() { return g_fixed_http_port; }
  static void set_fixed_http_port(uint16 port) { g_fixed_http_port = port; }

  static uint16 fixed_https_port() { return g_fixed_https_port; }
  static void set_fixed_https_port(uint16 port) { g_fixed_https_port = port; }

 private:
  friend class base::RefCounted<HttpNetworkSession>;
  FRIEND_TEST(HttpNetworkTransactionTest, GroupNameForProxyConnections);

  ~HttpNetworkSession();

  // Total limit of sockets. Not a constant to allow experiments.
  static int max_sockets_;

  // Default to allow up to 6 connections per host. Experiment and tuning may
  // try other values (greater than 0).  Too large may cause many problems, such
  // as home routers blocking the connections!?!?
  static int max_sockets_per_group_;

  static uint16 g_fixed_http_port;
  static uint16 g_fixed_https_port;

  HttpAuthCache auth_cache_;
  SSLClientAuthCache ssl_client_auth_cache_;
  scoped_refptr<TCPClientSocketPool> tcp_socket_pool_;
  ClientSocketFactory* socket_factory_;
  scoped_refptr<HostResolver> host_resolver_;
  scoped_refptr<ProxyService> proxy_service_;
  scoped_refptr<SSLConfigService> ssl_config_service_;
  scoped_refptr<FlipSessionPool> flip_session_pool_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_H_
