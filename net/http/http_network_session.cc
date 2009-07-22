// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session.h"

#include "base/logging.h"

namespace net {

// static
int HttpNetworkSession::max_sockets_ = 100;

// static
int HttpNetworkSession::max_sockets_per_group_ = 6;

HttpNetworkSession::HttpNetworkSession(
    HostResolver* host_resolver,
    ProxyService* proxy_service,
    ClientSocketFactory* client_socket_factory)
    : connection_pool_(new TCPClientSocketPool(
          max_sockets_, max_sockets_per_group_, host_resolver,
          client_socket_factory)),
      host_resolver_(host_resolver),
      proxy_service_(proxy_service) {
  DCHECK(proxy_service);
}

// static
void HttpNetworkSession::set_max_sockets_per_group(int socket_count) {
  DCHECK(0 < socket_count);
  // The following is a sanity check... but we should NEVER be near this value.
  DCHECK(100 > socket_count);
  max_sockets_per_group_ = socket_count;
}

} //  namespace net
