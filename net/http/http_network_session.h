// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/host_resolver.h"
#include "net/base/network_change_notifier.h"
#include "net/base/ssl_client_auth_cache.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_alternate_protocols.h"
#include "net/http/http_auth_cache.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/spdy/spdy_settings_storage.h"

namespace net {

class ClientSocketFactory;
class HttpAuthHandlerFactory;
class SpdySessionPool;
class NetworkChangeNotifier;
class URLSecurityManager;

// This class holds session objects used by HttpNetworkTransaction objects.
class HttpNetworkSession
    : public base::RefCounted<HttpNetworkSession>,
      public NetworkChangeNotifier::Observer {
 public:
  HttpNetworkSession(
      NetworkChangeNotifier* network_change_notifier,
      HostResolver* host_resolver,
      ProxyService* proxy_service,
      ClientSocketFactory* client_socket_factory,
      SSLConfigService* ssl_config_service,
      HttpAuthHandlerFactory* http_auth_handler_factory);

  HttpAuthCache* auth_cache() { return &auth_cache_; }
  SSLClientAuthCache* ssl_client_auth_cache() {
    return &ssl_client_auth_cache_;
  }

  const HttpAlternateProtocols& alternate_protocols() const {
    return alternate_protocols_;
  }
  HttpAlternateProtocols* mutable_alternate_protocols() {
    return &alternate_protocols_;
  }

  // Access to the SpdySettingsStorage
  const SpdySettingsStorage& spdy_settings() const {
    return spdy_settings_;
  }
  SpdySettingsStorage* mutable_spdy_settings() {
    return &spdy_settings_;
  }

  // TCP sockets come from the tcp_socket_pool().
  const scoped_refptr<TCPClientSocketPool>& tcp_socket_pool() {
    return tcp_socket_pool_;
  }
  const scoped_refptr<SOCKSClientSocketPool>& socks_socket_pool() {
    return socks_socket_pool_;
  }
  // SSL sockets come from the socket_factory().
  ClientSocketFactory* socket_factory() { return socket_factory_; }
  HostResolver* host_resolver() { return host_resolver_; }
  ProxyService* proxy_service() { return proxy_service_; }
  SSLConfigService* ssl_config_service() { return ssl_config_service_; }
  const scoped_refptr<SpdySessionPool>& spdy_session_pool() {
    return spdy_session_pool_;
  }
  HttpAuthHandlerFactory* http_auth_handler_factory() {
    return http_auth_handler_factory_;
  }

  // Returns a pointer to the URL security manager.
  URLSecurityManager* GetURLSecurityManager();

  // Flushes cached data in the HttpNetworkSession.
  void Flush();

  // NetworkChangeNotifier::Observer methods:
  virtual void OnIPAddressChanged();

  static void set_max_sockets_per_group(int socket_count);

  static uint16 fixed_http_port() { return g_fixed_http_port; }
  static void set_fixed_http_port(uint16 port) { g_fixed_http_port = port; }

  static uint16 fixed_https_port() { return g_fixed_https_port; }
  static void set_fixed_https_port(uint16 port) { g_fixed_https_port = port; }

 private:
  friend class base::RefCounted<HttpNetworkSession>;
  FRIEND_TEST(HttpNetworkTransactionTest, GroupNameForProxyConnections);

  ~HttpNetworkSession();

  scoped_refptr<TCPClientSocketPool> CreateNewTCPSocketPool();
  scoped_refptr<SOCKSClientSocketPool> CreateNewSOCKSSocketPool();

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
  HttpAlternateProtocols alternate_protocols_;
  NetworkChangeNotifier* const network_change_notifier_;
  ClientSocketFactory* socket_factory_;
  scoped_refptr<HostResolver> host_resolver_;
  scoped_refptr<TCPClientSocketPool> tcp_socket_pool_;
  scoped_refptr<SOCKSClientSocketPool> socks_socket_pool_;
  scoped_refptr<ProxyService> proxy_service_;
  scoped_refptr<SSLConfigService> ssl_config_service_;
  scoped_refptr<SpdySessionPool> spdy_session_pool_;
  HttpAuthHandlerFactory* http_auth_handler_factory_;
  scoped_ptr<URLSecurityManager> url_security_manager_;
  SpdySettingsStorage spdy_settings_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_H_
