// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ClientSocketPoolManager manages access to all ClientSocketPools.  It's a
// simple container for all of them.  Most importantly, it handles the lifetime
// and destruction order properly.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_H_
#pragma once

#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"

class GURL;

namespace base {
class Value;
}

namespace net {

class BoundNetLog;
class ClientSocketHandle;
class HostPortPair;
class HttpNetworkSession;
class HttpProxyClientSocketPool;
class HttpRequestHeaders;
class ProxyInfo;
class TransportClientSocketPool;
class SOCKSClientSocketPool;
class SSLClientSocketPool;

struct SSLConfig;

// This should rather be a simple constant but Windows shared libs doesn't
// really offer much flexiblity in exporting contants.
enum DefaultMaxValues { kDefaultMaxSocketsPerProxyServer = 32 };

class NET_EXPORT_PRIVATE ClientSocketPoolManager {
 public:
  ClientSocketPoolManager();
  virtual ~ClientSocketPoolManager();

  // The setter methods below affect only newly created socket pools after the
  // methods are called. Normally they should be called at program startup
  // before any ClientSocketPoolManagerImpl is created.
  static int max_sockets_per_pool();
  static void set_max_sockets_per_pool(int socket_count);

  static int max_sockets_per_group();
  static void set_max_sockets_per_group(int socket_count);

  static int max_sockets_per_proxy_server();
  static void set_max_sockets_per_proxy_server(int socket_count);

  virtual void FlushSocketPools() = 0;
  virtual void CloseIdleSockets() = 0;
  virtual TransportClientSocketPool* GetTransportSocketPool() = 0;
  virtual SSLClientSocketPool* GetSSLSocketPool() = 0;
  virtual SOCKSClientSocketPool* GetSocketPoolForSOCKSProxy(
      const HostPortPair& socks_proxy) = 0;
  virtual HttpProxyClientSocketPool* GetSocketPoolForHTTPProxy(
      const HostPortPair& http_proxy) = 0;
  virtual SSLClientSocketPool* GetSocketPoolForSSLWithProxy(
      const HostPortPair& proxy_server) = 0;
  // Creates a Value summary of the state of the socket pools. The caller is
  // responsible for deleting the returned value.
  virtual base::Value* SocketPoolInfoToValue() const = 0;
};

// A helper method that uses the passed in proxy information to initialize a
// ClientSocketHandle with the relevant socket pool. Use this method for
// HTTP/HTTPS requests. |ssl_config_for_origin| is only used if the request
// uses SSL and |ssl_config_for_proxy| is used if the proxy server is HTTPS.
int InitSocketHandleForHttpRequest(
    const GURL& request_url,
    const HttpRequestHeaders& request_extra_headers,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    bool force_spdy_over_ssl,
    bool want_spdy_over_npn,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    const BoundNetLog& net_log,
    ClientSocketHandle* socket_handle,
    OldCompletionCallback* callback);

// A helper method that uses the passed in proxy information to initialize a
// ClientSocketHandle with the relevant socket pool. Use this method for
// a raw socket connection to a host-port pair (that needs to tunnel through
// the proxies).
NET_EXPORT int InitSocketHandleForRawConnect(
    const HostPortPair& host_port_pair,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    const BoundNetLog& net_log,
    ClientSocketHandle* socket_handle,
    OldCompletionCallback* callback);

// Similar to InitSocketHandleForHttpRequest except that it initiates the
// desired number of preconnect streams from the relevant socket pool.
int PreconnectSocketsForHttpRequest(
    const GURL& request_url,
    const HttpRequestHeaders& request_extra_headers,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    bool force_spdy_over_ssl,
    bool want_spdy_over_npn,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    const BoundNetLog& net_log,
    int num_preconnect_streams);

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_H_
