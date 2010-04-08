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
    HttpAuthHandlerFactory* http_auth_handler_factory)
    : network_change_notifier_(network_change_notifier),
      socket_factory_(client_socket_factory),
      host_resolver_(host_resolver),
      tcp_socket_pool_(CreateNewTCPSocketPool()),
      socks_socket_pool_(CreateNewSOCKSSocketPool()),
      proxy_service_(proxy_service),
      ssl_config_service_(ssl_config_service),
      spdy_session_pool_(new SpdySessionPool()),
      http_auth_handler_factory_(http_auth_handler_factory) {
  DCHECK(proxy_service);
  DCHECK(ssl_config_service);

  if (network_change_notifier)
    network_change_notifier_->AddObserver(this);
}

HttpNetworkSession::~HttpNetworkSession() {
  if (network_change_notifier_)
    network_change_notifier_->RemoveObserver(this);
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
  DCHECK_LT(0, socket_count);
  // The following is a sanity check... but we should NEVER be near this value.
  DCHECK_GT(100, socket_count);
  max_sockets_per_group_ = socket_count;
}

void HttpNetworkSession::Flush() {
  host_resolver()->Flush();
  tcp_socket_pool()->CloseIdleSockets();
  tcp_socket_pool_ = CreateNewTCPSocketPool();
  socks_socket_pool()->CloseIdleSockets();
  socks_socket_pool_ = CreateNewSOCKSSocketPool();
  spdy_session_pool_->CloseAllSessions();
  spdy_session_pool_ = new SpdySessionPool;
}

void HttpNetworkSession::OnIPAddressChanged() {
  Flush();
}

scoped_refptr<TCPClientSocketPool>
HttpNetworkSession::CreateNewTCPSocketPool() {
  // TODO(vandebo) when we've completely converted to pools, the base TCP
  // pool name should get changed to TCP instead of Transport.
  return new TCPClientSocketPool(max_sockets_,
                                 max_sockets_per_group_,
                                 "Transport",
                                 host_resolver_,
                                 socket_factory_);
}

scoped_refptr<SOCKSClientSocketPool>
HttpNetworkSession::CreateNewSOCKSSocketPool() {
  return new SOCKSClientSocketPool(
      max_sockets_, max_sockets_per_group_, "SOCKS", host_resolver_,
      new TCPClientSocketPool(max_sockets_, max_sockets_per_group_,
                              "TCPForSOCKS", host_resolver_,
                              socket_factory_));
}

}  //  namespace net
