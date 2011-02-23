// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_H_
#pragma once

#include <set>
#include "base/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_client_auth_cache.h"
#include "net/http/http_alternate_protocols.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_stream_factory.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_settings_storage.h"

class Value;

namespace net {

class CertVerifier;
class ClientSocketFactory;
class DnsCertProvenanceChecker;
class DnsRRResolver;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpNetworkDelegate;
class HttpNetworkSessionPeer;
class HttpProxyClientSocketPool;
class HttpResponseBodyDrainer;
class NetLog;
class ProxyService;
class SSLConfigService;
class SSLHostInfoFactory;

// This class holds session objects used by HttpNetworkTransaction objects.
class HttpNetworkSession : public base::RefCounted<HttpNetworkSession>,
                           public base::NonThreadSafe {
 public:
  struct Params {
    Params()
        : client_socket_factory(NULL),
          host_resolver(NULL),
          cert_verifier(NULL),
          dnsrr_resolver(NULL),
          dns_cert_checker(NULL),
          proxy_service(NULL),
          ssl_host_info_factory(NULL),
          ssl_config_service(NULL),
          http_auth_handler_factory(NULL),
          network_delegate(NULL),
          net_log(NULL) {}

    ClientSocketFactory* client_socket_factory;
    HostResolver* host_resolver;
    CertVerifier* cert_verifier;
    DnsRRResolver* dnsrr_resolver;
    DnsCertProvenanceChecker* dns_cert_checker;
    ProxyService* proxy_service;
    SSLHostInfoFactory* ssl_host_info_factory;
    SSLConfigService* ssl_config_service;
    HttpAuthHandlerFactory* http_auth_handler_factory;
    HttpNetworkDelegate* network_delegate;
    NetLog* net_log;
  };

  explicit HttpNetworkSession(const Params& params);

  HttpAuthCache* auth_cache() { return &auth_cache_; }
  SSLClientAuthCache* ssl_client_auth_cache() {
    return &ssl_client_auth_cache_;
  }

  void AddResponseDrainer(HttpResponseBodyDrainer* drainer);

  void RemoveResponseDrainer(HttpResponseBodyDrainer* drainer);

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

  TCPClientSocketPool* tcp_socket_pool() {
    return socket_pool_manager_.tcp_socket_pool();
  }

  SSLClientSocketPool* ssl_socket_pool() {
    return socket_pool_manager_.ssl_socket_pool();
  }

  SOCKSClientSocketPool* GetSocketPoolForSOCKSProxy(
      const HostPortPair& socks_proxy) {
    return socket_pool_manager_.GetSocketPoolForSOCKSProxy(socks_proxy);
  }

  HttpProxyClientSocketPool* GetSocketPoolForHTTPProxy(
      const HostPortPair& http_proxy) {
    return socket_pool_manager_.GetSocketPoolForHTTPProxy(http_proxy);
  }

  SSLClientSocketPool* GetSocketPoolForSSLWithProxy(
      const HostPortPair& proxy_server) {
    return socket_pool_manager_.GetSocketPoolForSSLWithProxy(proxy_server);
  }

  CertVerifier* cert_verifier() { return cert_verifier_; }
  ProxyService* proxy_service() { return proxy_service_; }
  SSLConfigService* ssl_config_service() { return ssl_config_service_; }
  SpdySessionPool* spdy_session_pool() { return &spdy_session_pool_; }
  HttpAuthHandlerFactory* http_auth_handler_factory() {
    return http_auth_handler_factory_;
  }
  HttpNetworkDelegate* network_delegate() {
    return network_delegate_;
  }

  HttpStreamFactory* http_stream_factory() {
    return &http_stream_factory_;
  }

  NetLog* net_log() {
    return net_log_;
  }

  // Creates a Value summary of the state of the socket pools. The caller is
  // responsible for deleting the returned value.
  Value* SocketPoolInfoToValue() const {
    return socket_pool_manager_.SocketPoolInfoToValue();
  }

  // Creates a Value summary of the state of the SPDY sessions. The caller is
  // responsible for deleting the returned value.
  Value* SpdySessionPoolInfoToValue() const;

  void FlushSocketPools() {
    socket_pool_manager_.FlushSocketPools();
  }

 private:
  friend class base::RefCounted<HttpNetworkSession>;
  friend class HttpNetworkSessionPeer;

  ~HttpNetworkSession();

  HttpAuthCache auth_cache_;
  SSLClientAuthCache ssl_client_auth_cache_;
  HttpAlternateProtocols alternate_protocols_;
  CertVerifier* cert_verifier_;
  // Not const since it's modified by HttpNetworkSessionPeer for testing.
  scoped_refptr<ProxyService> proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;
  ClientSocketPoolManager socket_pool_manager_;
  SpdySessionPool spdy_session_pool_;
  HttpStreamFactory http_stream_factory_;
  HttpAuthHandlerFactory* const http_auth_handler_factory_;
  HttpNetworkDelegate* const network_delegate_;
  NetLog* const net_log_;
  SpdySettingsStorage spdy_settings_;
  std::set<HttpResponseBodyDrainer*> response_drainers_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_H_
