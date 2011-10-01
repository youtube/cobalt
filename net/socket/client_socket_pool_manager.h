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

#include <map>
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/template_util.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/cert_database.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/socket/client_socket_pool_histograms.h"

class GURL;

namespace base {
class Value;
}

namespace net {

class BoundNetLog;
class CertVerifier;
class ClientSocketFactory;
class ClientSocketHandle;
class ClientSocketPoolHistograms;
class DnsCertProvenanceChecker;
class DnsRRResolver;
class HttpNetworkSession;
class HttpRequestHeaders;
class HostPortPair;
class HttpProxyClientSocketPool;
class HostResolver;
class NetLog;
class OriginBoundCertService;
class ProxyInfo;
class ProxyService;
class SOCKSClientSocketPool;
class SSLClientSocketPool;
class SSLConfigService;
class SSLHostInfoFactory;
class TransportClientSocketPool;

struct SSLConfig;

// This should rather be a simple constant but Windows shared libs doesn't
// really offer much flexiblity in exporting contants.
enum DefaultMaxValues { kDefaultMaxSocketsPerProxyServer = 32 };

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
                          OriginBoundCertService* origin_bound_cert_service,
                          DnsRRResolver* dnsrr_resolver,
                          DnsCertProvenanceChecker* dns_cert_checker,
                          SSLHostInfoFactory* ssl_host_info_factory,
                          ProxyService* proxy_service,
                          SSLConfigService* ssl_config_service);
  virtual ~ClientSocketPoolManager();

  void FlushSocketPools();
  void CloseIdleSockets();

  TransportClientSocketPool* transport_socket_pool() {
    return transport_socket_pool_.get();
  }

  SSLClientSocketPool* ssl_socket_pool() { return ssl_socket_pool_.get(); }

  SOCKSClientSocketPool* GetSocketPoolForSOCKSProxy(
      const HostPortPair& socks_proxy);

  HttpProxyClientSocketPool* GetSocketPoolForHTTPProxy(
      const HostPortPair& http_proxy);

  SSLClientSocketPool* GetSocketPoolForSSLWithProxy(
      const HostPortPair& proxy_server);

  NET_EXPORT static int max_sockets_per_group();
  NET_EXPORT static void set_max_sockets_per_group(int socket_count);
  NET_EXPORT static void set_max_sockets_per_proxy_server(int socket_count);

  // A helper method that uses the passed in proxy information to initialize a
  // ClientSocketHandle with the relevant socket pool. Use this method for
  // HTTP/HTTPS requests. |ssl_config_for_origin| is only used if the request
  // uses SSL and |ssl_config_for_proxy| is used if the proxy server is HTTPS.
  static int InitSocketHandleForHttpRequest(
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
  NET_EXPORT static int InitSocketHandleForRawConnect(
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
  static int PreconnectSocketsForHttpRequest(
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

  // Creates a Value summary of the state of the socket pools. The caller is
  // responsible for deleting the returned value.
  base::Value* SocketPoolInfoToValue() const;

  // CertDatabase::Observer methods:
  virtual void OnUserCertAdded(const X509Certificate* cert);
  virtual void OnCertTrustChanged(const X509Certificate* cert);

 private:
  friend class HttpNetworkSessionPeer;

  typedef internal::OwnedPoolMap<HostPortPair, TransportClientSocketPool*>
      TransportSocketPoolMap;
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
  OriginBoundCertService* const origin_bound_cert_service_;
  DnsRRResolver* const dnsrr_resolver_;
  DnsCertProvenanceChecker* const dns_cert_checker_;
  SSLHostInfoFactory* const ssl_host_info_factory_;
  ProxyService* const proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;

  // Note: this ordering is important.

  ClientSocketPoolHistograms transport_pool_histograms_;
  scoped_ptr<TransportClientSocketPool> transport_socket_pool_;

  ClientSocketPoolHistograms ssl_pool_histograms_;
  scoped_ptr<SSLClientSocketPool> ssl_socket_pool_;

  ClientSocketPoolHistograms transport_for_socks_pool_histograms_;
  TransportSocketPoolMap transport_socket_pools_for_socks_proxies_;

  ClientSocketPoolHistograms socks_pool_histograms_;
  SOCKSSocketPoolMap socks_socket_pools_;

  ClientSocketPoolHistograms transport_for_http_proxy_pool_histograms_;
  TransportSocketPoolMap transport_socket_pools_for_http_proxies_;

  ClientSocketPoolHistograms transport_for_https_proxy_pool_histograms_;
  TransportSocketPoolMap transport_socket_pools_for_https_proxies_;

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
