// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_
#pragma once

#include "base/ref_counted.h"

namespace net {

class HostPortPair;
class HttpNetworkSession;
class HttpProxyClientSocketPool;
class HttpStreamFactory;
class ProxyService;
class SOCKSClientSocketPool;
class SSLClientSocketPool;
class TCPClientSocketPool;

class HttpNetworkSessionPeer {
 public:
  explicit HttpNetworkSessionPeer(
      const scoped_refptr<HttpNetworkSession>& session);
  ~HttpNetworkSessionPeer();

  void SetTCPSocketPool(TCPClientSocketPool* pool);

  void SetSocketPoolForSOCKSProxy(
      const HostPortPair& socks_proxy,
      SOCKSClientSocketPool* pool);

  void SetSocketPoolForHTTPProxy(
      const HostPortPair& http_proxy,
      HttpProxyClientSocketPool* pool);

  void SetSSLSocketPool(SSLClientSocketPool* pool);

  void SetSocketPoolForSSLWithProxy(
      const HostPortPair& proxy_host,
      SSLClientSocketPool* pool);

  void SetProxyService(ProxyService* proxy_service);

  void SetHttpStreamFactory(HttpStreamFactory* http_stream_factory);

 private:
  const scoped_refptr<HttpNetworkSession> session_;

  DISALLOW_COPY_AND_ASSIGN(HttpNetworkSessionPeer);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_
