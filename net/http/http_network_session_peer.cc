// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session_peer.h"

#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"

namespace net {

HttpNetworkSessionPeer::HttpNetworkSessionPeer(
    const scoped_refptr<HttpNetworkSession>& session)
    : session_(session) {}

HttpNetworkSessionPeer::~HttpNetworkSessionPeer() {}

void HttpNetworkSessionPeer::SetClientSocketPoolManager(
    ClientSocketPoolManager* socket_pool_manager) {
  session_->socket_pool_manager_.reset(socket_pool_manager);
}

void HttpNetworkSessionPeer::SetProxyService(ProxyService* proxy_service) {
  session_->proxy_service_ = proxy_service;
}

void HttpNetworkSessionPeer::SetHttpStreamFactory(
    HttpStreamFactory* http_stream_factory) {
  session_->http_stream_factory_.reset(http_stream_factory);
}

}  // namespace net
