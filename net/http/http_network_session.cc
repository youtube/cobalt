// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session.h"

#include "base/logging.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/url_security_manager.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

// static
int HttpNetworkSession::max_sockets_ = 100;

// static
int HttpNetworkSession::max_sockets_per_group_ = 6;

// static
uint16 HttpNetworkSession::g_fixed_http_port = 0;
uint16 HttpNetworkSession::g_fixed_https_port = 0;

// TODO(vandebo) when we've completely converted to pools, the base TCP
// pool name should get changed to TCP instead of Transport.
HttpNetworkSession::HttpNetworkSession(
    NetworkChangeNotifier* network_change_notifier,
    HostResolver* host_resolver,
    ProxyService* proxy_service,
    ClientSocketFactory* client_socket_factory,
    SSLConfigService* ssl_config_service,
    SpdySessionPool* spdy_session_pool,
    HttpAuthHandlerFactory* http_auth_handler_factory)
    : network_change_notifier_(network_change_notifier),
      tcp_socket_pool_(new TCPClientSocketPool(
          max_sockets_, max_sockets_per_group_, "Transport",
          host_resolver, client_socket_factory, network_change_notifier_)),
      socks_socket_pool_(new SOCKSClientSocketPool(
          max_sockets_, max_sockets_per_group_, "SOCKS", host_resolver,
          new TCPClientSocketPool(max_sockets_, max_sockets_per_group_,
                                  "TCPForSOCKS", host_resolver,
                                  client_socket_factory,
                                  network_change_notifier_),
          network_change_notifier_)),
      socket_factory_(client_socket_factory),
      host_resolver_(host_resolver),
      proxy_service_(proxy_service),
      ssl_config_service_(ssl_config_service),
      spdy_session_pool_(spdy_session_pool),
      http_auth_handler_factory_(http_auth_handler_factory) {
  DCHECK(proxy_service);
  DCHECK(ssl_config_service);
}

HttpNetworkSession::~HttpNetworkSession() {
}

URLSecurityManager* HttpNetworkSession::GetURLSecurityManager() {
  // Create the URL security manager lazily in the first call.
  // This is called on a single thread.
  if (!url_security_manager_.get()) {
    url_security_manager_.reset(
        URLSecurityManager::Create(http_auth_handler_factory_->filter()));
  }
  return url_security_manager_.get();
}

// static
void HttpNetworkSession::set_max_sockets_per_group(int socket_count) {
  DCHECK(0 < socket_count);
  // The following is a sanity check... but we should NEVER be near this value.
  DCHECK(100 > socket_count);
  max_sockets_per_group_ = socket_count;
}

// TODO(vandebo) when we've completely converted to pools, the base TCP
// pool name should get changed to TCP instead of Transport.
void HttpNetworkSession::ReplaceTCPSocketPool() {
  tcp_socket_pool_ = new TCPClientSocketPool(max_sockets_,
                                             max_sockets_per_group_,
                                             "Transport",
                                             host_resolver_,
                                             socket_factory_,
                                             network_change_notifier_);
}

}  //  namespace net
