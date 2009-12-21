// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session.h"

#include "base/logging.h"
#include "net/base/network_change_notifier.h"
#include "net/flip/flip_session_pool.h"

namespace net {

// static
int HttpNetworkSession::max_sockets_ = 100;

// static
int HttpNetworkSession::max_sockets_per_group_ = 6;

// static
uint16 HttpNetworkSession::g_fixed_http_port = 0;
uint16 HttpNetworkSession::g_fixed_https_port = 0;

HttpNetworkSession::HttpNetworkSession(
    HostResolver* host_resolver,
    ProxyService* proxy_service,
    ClientSocketFactory* client_socket_factory,
    SSLConfigService* ssl_config_service,
    FlipSessionPool* flip_session_pool)
    : network_change_notifier_(
          NetworkChangeNotifier::CreateDefaultNetworkChangeNotifier()),
      tcp_socket_pool_(new TCPClientSocketPool(
          max_sockets_, max_sockets_per_group_,
          host_resolver, client_socket_factory, network_change_notifier_)),
      socket_factory_(client_socket_factory),
      host_resolver_(host_resolver),
      proxy_service_(proxy_service),
      ssl_config_service_(ssl_config_service),
      flip_session_pool_(flip_session_pool) {
  DCHECK(proxy_service);
  DCHECK(ssl_config_service);
}

HttpNetworkSession::~HttpNetworkSession() {
}

// static
void HttpNetworkSession::set_max_sockets_per_group(int socket_count) {
  DCHECK(0 < socket_count);
  // The following is a sanity check... but we should NEVER be near this value.
  DCHECK(100 > socket_count);
  max_sockets_per_group_ = socket_count;
}

} //  namespace net
