// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_H_
#pragma once

#include <set>
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/net_export.h"
#include "net/base/ssl_client_auth_cache.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_stream_factory.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_settings_storage.h"

namespace base {
class Value;
}

namespace net {

class CertVerifier;
class ClientSocketFactory;
class DnsCertProvenanceChecker;
class DnsRRResolver;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpNetworkSessionPeer;
class HttpProxyClientSocketPool;
class HttpResponseBodyDrainer;
class HttpServerProperties;
class NetLog;
class NetworkDelegate;
class OriginBoundCertService;
class ProxyService;
class SOCKSClientSocketPool;
class SSLClientSocketPool;
class SSLConfigService;
class SSLHostInfoFactory;
class TransportClientSocketPool;

// This class holds session objects used by HttpNetworkTransaction objects.
class NET_EXPORT HttpNetworkSession
    : public base::RefCounted<HttpNetworkSession>,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  struct NET_EXPORT Params {
    Params()
        : client_socket_factory(NULL),
          host_resolver(NULL),
          cert_verifier(NULL),
          origin_bound_cert_service(NULL),
          dnsrr_resolver(NULL),
          dns_cert_checker(NULL),
          proxy_service(NULL),
          ssl_host_info_factory(NULL),
          ssl_config_service(NULL),
          http_auth_handler_factory(NULL),
          network_delegate(NULL),
          http_server_properties(NULL),
          net_log(NULL) {}

    ClientSocketFactory* client_socket_factory;
    HostResolver* host_resolver;
    CertVerifier* cert_verifier;
    OriginBoundCertService* origin_bound_cert_service;
    DnsRRResolver* dnsrr_resolver;
    DnsCertProvenanceChecker* dns_cert_checker;
    ProxyService* proxy_service;
    SSLHostInfoFactory* ssl_host_info_factory;
    SSLConfigService* ssl_config_service;
    HttpAuthHandlerFactory* http_auth_handler_factory;
    NetworkDelegate* network_delegate;
    HttpServerProperties* http_server_properties;
    NetLog* net_log;
  };

  explicit HttpNetworkSession(const Params& params);

  HttpAuthCache* http_auth_cache() { return &http_auth_cache_; }
  SSLClientAuthCache* ssl_client_auth_cache() {
    return &ssl_client_auth_cache_;
  }

  void AddResponseDrainer(HttpResponseBodyDrainer* drainer);

  void RemoveResponseDrainer(HttpResponseBodyDrainer* drainer);

  TransportClientSocketPool* GetTransportSocketPool() {
    return socket_pool_manager_->GetTransportSocketPool();
  }

  SSLClientSocketPool* GetSSLSocketPool() {
    return socket_pool_manager_->GetSSLSocketPool();
  }

  SOCKSClientSocketPool* GetSocketPoolForSOCKSProxy(
      const HostPortPair& socks_proxy);

  HttpProxyClientSocketPool* GetSocketPoolForHTTPProxy(
      const HostPortPair& http_proxy);

  SSLClientSocketPool* GetSocketPoolForSSLWithProxy(
      const HostPortPair& proxy_server);

  CertVerifier* cert_verifier() { return cert_verifier_; }
  ProxyService* proxy_service() { return proxy_service_; }
  SSLConfigService* ssl_config_service() { return ssl_config_service_; }
  SpdySessionPool* spdy_session_pool() { return &spdy_session_pool_; }
  HttpAuthHandlerFactory* http_auth_handler_factory() {
    return http_auth_handler_factory_;
  }
  NetworkDelegate* network_delegate() {
    return network_delegate_;
  }
  HttpServerProperties* http_server_properties() {
    return http_server_properties_;
  }
  HttpStreamFactory* http_stream_factory() {
    return http_stream_factory_.get();
  }
  NetLog* net_log() {
    return net_log_;
  }

  // Creates a Value summary of the state of the socket pools. The caller is
  // responsible for deleting the returned value.
  base::Value* SocketPoolInfoToValue() const;

  // Creates a Value summary of the state of the SPDY sessions. The caller is
  // responsible for deleting the returned value.
  base::Value* SpdySessionPoolInfoToValue() const;

  void CloseAllConnections();
  void CloseIdleConnections();

 private:
  friend class base::RefCounted<HttpNetworkSession>;
  friend class HttpNetworkSessionPeer;

  ~HttpNetworkSession();

  NetLog* const net_log_;
  NetworkDelegate* const network_delegate_;
  HttpServerProperties* const http_server_properties_;
  CertVerifier* const cert_verifier_;
  HttpAuthHandlerFactory* const http_auth_handler_factory_;

  // Not const since it's modified by HttpNetworkSessionPeer for testing.
  ProxyService* proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;

  HttpAuthCache http_auth_cache_;
  SSLClientAuthCache ssl_client_auth_cache_;
  scoped_ptr<ClientSocketPoolManager> socket_pool_manager_;
  SpdySessionPool spdy_session_pool_;
  scoped_ptr<HttpStreamFactory> http_stream_factory_;
  std::set<HttpResponseBodyDrainer*> response_drainers_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_H_
