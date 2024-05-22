// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ClientSocketPoolManager manages access to all ClientSocketPools.  It's a
// simple container for all of them.  Most importantly, it handles the lifetime
// and destruction order properly.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_H_

#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_network_session.h"
#include "net/socket/client_socket_pool.h"
#include "url/scheme_host_port.h"

namespace net {

class ClientSocketHandle;
class NetLogWithSource;
class NetworkAnonymizationKey;
class ProxyInfo;
class ProxyServer;

struct SSLConfig;

constexpr int kDefaultMaxSocketsPerProxyServer = 32;

class NET_EXPORT_PRIVATE ClientSocketPoolManager {
 public:
  ClientSocketPoolManager();
  virtual ~ClientSocketPoolManager();

  // The setter methods below affect only newly created socket pools after the
  // methods are called. Normally they should be called at program startup
  // before any ClientSocketPoolManagerImpl is created.
  static int max_sockets_per_pool(HttpNetworkSession::SocketPoolType pool_type);
  static void set_max_sockets_per_pool(
      HttpNetworkSession::SocketPoolType pool_type,
      int socket_count);

  static int max_sockets_per_group(
      HttpNetworkSession::SocketPoolType pool_type);
  static void set_max_sockets_per_group(
      HttpNetworkSession::SocketPoolType pool_type,
      int socket_count);

  static int max_sockets_per_proxy_server(
      HttpNetworkSession::SocketPoolType pool_type);
  static void set_max_sockets_per_proxy_server(
      HttpNetworkSession::SocketPoolType pool_type,
      int socket_count);

  static base::TimeDelta unused_idle_socket_timeout(
      HttpNetworkSession::SocketPoolType pool_type);

  // The |net_error| is returned to clients of pending socket requests, while
  // |reason| is logged at the socket layer.
  virtual void FlushSocketPoolsWithError(int net_error,
                                         const char* net_log_reason_utf8) = 0;
  virtual void CloseIdleSockets(const char* net_log_reason_utf8) = 0;

  // Returns the socket pool for the specified ProxyServer (Which may be
  // ProxyServer::Direct()).
  virtual ClientSocketPool* GetSocketPool(const ProxyServer& proxy_server) = 0;

  // Creates a Value summary of the state of the socket pools.
  virtual base::Value SocketPoolInfoToValue() const = 0;
};

// A helper method that uses the passed in proxy information to initialize a
// ClientSocketHandle with the relevant socket pool. Use this method for
// HTTP/HTTPS requests. |ssl_config_for_origin| is only used if the request
// uses SSL and |ssl_config_for_proxy| is used if the proxy server is HTTPS.
// |resolution_callback| will be invoked after the the hostname is
// resolved.  If |resolution_callback| does not return OK, then the
// connection will be aborted with that value.
int InitSocketHandleForHttpRequest(
    url::SchemeHostPort endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    NetworkAnonymizationKey network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    const SocketTag& socket_tag,
    const NetLogWithSource& net_log,
    ClientSocketHandle* socket_handle,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback);

// A helper method that uses the passed in proxy information to initialize a
// ClientSocketHandle with the relevant socket pool. Use this method for
// HTTP/HTTPS requests for WebSocket handshake.
// |ssl_config_for_origin| is only used if the request
// uses SSL and |ssl_config_for_proxy| is used if the proxy server is HTTPS.
// |resolution_callback| will be invoked after the the hostname is
// resolved.  If |resolution_callback| does not return OK, then the
// connection will be aborted with that value.
// This function uses WEBSOCKET_SOCKET_POOL socket pools.
int InitSocketHandleForWebSocketRequest(
    url::SchemeHostPort endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    NetworkAnonymizationKey network_anonymization_key,
    const NetLogWithSource& net_log,
    ClientSocketHandle* socket_handle,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback);

// Similar to InitSocketHandleForHttpRequest except that it initiates the
// desired number of preconnect streams from the relevant socket pool.
int PreconnectSocketsForHttpRequest(
    url::SchemeHostPort endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    NetworkAnonymizationKey network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    const NetLogWithSource& net_log,
    int num_preconnect_streams,
    CompletionOnceCallback callback);

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_H_
