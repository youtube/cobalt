// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ClientSocketPoolManager manages access to all ClientSocketPools.  It's a
// simple container for all of them.  Most importantly, it handles the lifetime
// and destruction order properly.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_H_
#pragma once

#include <map>
#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/template_util.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/cert_database.h"
#include "net/socket/client_socket_pool_histograms.h"

class Value;

namespace net {

class CertVerifier;
class ClientSocketFactory;
class ClientSocketPoolHistograms;
class DnsCertProvenanceChecker;
class DnsRRResolver;
class HostPortPair;
class HttpProxyClientSocketPool;
class HostResolver;
class NetLog;
class ProxyService;
class SOCKSClientSocketPool;
class SSLClientSocketPool;
class SSLConfigService;
class SSLHostInfoFactory;
class TCPClientSocketPool;

namespace internal {

// A helper class for auto-deleting Values in the destructor.
template <typename Key, typename Value>
class OwnedPoolMap : public std::map<Key, Value> {
 public:
  OwnedPoolMap() {
    COMPILE_ASSERT(base::is_pointer<Value>::value,
                   value_must_be_a_pointer);
  }

  ~OwnedPoolMap() {
    STLDeleteValues(this);
  }
};

}  // namespace internal

class ClientSocketPoolManager : public base::NonThreadSafe,
                                public CertDatabase::Observer {
 public:
  ClientSocketPoolManager(NetLog* net_log,
                          ClientSocketFactory* socket_factory,
                          HostResolver* host_resolver,
                          CertVerifier* cert_verifier,
                          DnsRRResolver* dnsrr_resolver,
                          DnsCertProvenanceChecker* dns_cert_checker,
                          SSLHostInfoFactory* ssl_host_info_factory,
                          ProxyService* proxy_service,
                          SSLConfigService* ssl_config_service);
  ~ClientSocketPoolManager();

  void FlushSocketPools();
  void CloseIdleSockets();

  TCPClientSocketPool* tcp_socket_pool() { return tcp_socket_pool_.get(); }

  SSLClientSocketPool* ssl_socket_pool() { return ssl_socket_pool_.get(); }

  SOCKSClientSocketPool* GetSocketPoolForSOCKSProxy(
      const HostPortPair& socks_proxy);

  HttpProxyClientSocketPool* GetSocketPoolForHTTPProxy(
      const HostPortPair& http_proxy);

  SSLClientSocketPool* GetSocketPoolForSSLWithProxy(
      const HostPortPair& proxy_server);

  static int max_sockets_per_group();
  static void set_max_sockets_per_group(int socket_count);
  static void set_max_sockets_per_proxy_server(int socket_count);

  // Creates a Value summary of the state of the socket pools. The caller is
  // responsible for deleting the returned value.
  Value* SocketPoolInfoToValue() const;

  // CertDatabase::Observer methods:
  virtual void OnUserCertAdded(X509Certificate* cert);

 private:
  friend class HttpNetworkSessionPeer;

  typedef internal::OwnedPoolMap<HostPortPair, TCPClientSocketPool*>
      TCPSocketPoolMap;
  typedef internal::OwnedPoolMap<HostPortPair, SOCKSClientSocketPool*>
      SOCKSSocketPoolMap;
  typedef internal::OwnedPoolMap<HostPortPair, HttpProxyClientSocketPool*>
      HTTPProxySocketPoolMap;
  typedef internal::OwnedPoolMap<HostPortPair, SSLClientSocketPool*>
      SSLSocketPoolMap;

  NetLog* const net_log_;
  ClientSocketFactory* const socket_factory_;
  HostResolver* const host_resolver_;
  CertVerifier* const cert_verifier_;
  DnsRRResolver* const dnsrr_resolver_;
  DnsCertProvenanceChecker* const dns_cert_checker_;
  SSLHostInfoFactory* const ssl_host_info_factory_;
  const scoped_refptr<ProxyService> proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;

  // Note: this ordering is important.

  ClientSocketPoolHistograms tcp_pool_histograms_;
  scoped_ptr<TCPClientSocketPool> tcp_socket_pool_;

  ClientSocketPoolHistograms ssl_pool_histograms_;
  scoped_ptr<SSLClientSocketPool> ssl_socket_pool_;

  ClientSocketPoolHistograms tcp_for_socks_pool_histograms_;
  TCPSocketPoolMap tcp_socket_pools_for_socks_proxies_;

  ClientSocketPoolHistograms socks_pool_histograms_;
  SOCKSSocketPoolMap socks_socket_pools_;

  ClientSocketPoolHistograms tcp_for_http_proxy_pool_histograms_;
  TCPSocketPoolMap tcp_socket_pools_for_http_proxies_;

  ClientSocketPoolHistograms tcp_for_https_proxy_pool_histograms_;
  TCPSocketPoolMap tcp_socket_pools_for_https_proxies_;

  ClientSocketPoolHistograms ssl_for_https_proxy_pool_histograms_;
  SSLSocketPoolMap ssl_socket_pools_for_https_proxies_;

  ClientSocketPoolHistograms http_proxy_pool_histograms_;
  HTTPProxySocketPoolMap http_proxy_socket_pools_;

  ClientSocketPoolHistograms ssl_socket_pool_for_proxies_histograms_;
  SSLSocketPoolMap ssl_socket_pools_for_proxies_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolManager);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_H_
