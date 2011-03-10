// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ClientSocketPoolManager manages access to all ClientSocketPools.  It's a
// simple container for all of them.  Most importantly, it handles the lifetime
// and destruction order properly.

#include "net/socket/client_socket_pool_manager.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/tcp_client_socket_pool.h"

namespace net {

namespace {

// Total limit of sockets.
int g_max_sockets = 256;

// Default to allow up to 6 connections per host. Experiment and tuning may
// try other values (greater than 0).  Too large may cause many problems, such
// as home routers blocking the connections!?!?  See http://crbug.com/12066.
int g_max_sockets_per_group = 6;

// The max number of sockets to allow per proxy server.  This applies both to
// http and SOCKS proxies.  See http://crbug.com/12066 and
// http://crbug.com/44501 for details about proxy server connection limits.
int g_max_sockets_per_proxy_server = 32;

// Appends information about all |socket_pools| to the end of |list|.
template <class MapType>
static void AddSocketPoolsToList(ListValue* list,
                                 const MapType& socket_pools,
                                 const std::string& type,
                                 bool include_nested_pools) {
  for (typename MapType::const_iterator it = socket_pools.begin();
       it != socket_pools.end(); it++) {
    list->Append(it->second->GetInfoAsValue(it->first.ToString(),
                                            type,
                                            include_nested_pools));
  }
}

}  // namespace

ClientSocketPoolManager::ClientSocketPoolManager(
    NetLog* net_log,
    ClientSocketFactory* socket_factory,
    HostResolver* host_resolver,
    CertVerifier* cert_verifier,
    DnsRRResolver* dnsrr_resolver,
    DnsCertProvenanceChecker* dns_cert_checker,
    SSLHostInfoFactory* ssl_host_info_factory,
    ProxyService* proxy_service,
    SSLConfigService* ssl_config_service)
    : net_log_(net_log),
      socket_factory_(socket_factory),
      host_resolver_(host_resolver),
      cert_verifier_(cert_verifier),
      dnsrr_resolver_(dnsrr_resolver),
      dns_cert_checker_(dns_cert_checker),
      ssl_host_info_factory_(ssl_host_info_factory),
      proxy_service_(proxy_service),
      ssl_config_service_(ssl_config_service),
      tcp_pool_histograms_("TCP"),
      tcp_socket_pool_(new TCPClientSocketPool(
          g_max_sockets, g_max_sockets_per_group,
          &tcp_pool_histograms_,
          host_resolver,
          socket_factory_,
          net_log)),
      ssl_pool_histograms_("SSL2"),
      ssl_socket_pool_(new SSLClientSocketPool(
          g_max_sockets, g_max_sockets_per_group,
          &ssl_pool_histograms_,
          host_resolver,
          cert_verifier,
          dnsrr_resolver,
          dns_cert_checker,
          ssl_host_info_factory,
          socket_factory,
          tcp_socket_pool_.get(),
          NULL /* no socks proxy */,
          NULL /* no http proxy */,
          ssl_config_service,
          net_log)),
      tcp_for_socks_pool_histograms_("TCPforSOCKS"),
      socks_pool_histograms_("SOCK"),
      tcp_for_http_proxy_pool_histograms_("TCPforHTTPProxy"),
      tcp_for_https_proxy_pool_histograms_("TCPforHTTPSProxy"),
      ssl_for_https_proxy_pool_histograms_("SSLforHTTPSProxy"),
      http_proxy_pool_histograms_("HTTPProxy"),
      ssl_socket_pool_for_proxies_histograms_("SSLForProxies") {
  CertDatabase::AddObserver(this);
}

ClientSocketPoolManager::~ClientSocketPoolManager() {
  CertDatabase::RemoveObserver(this);
}

void ClientSocketPoolManager::FlushSocketPools() {
  // Flush the highest level pools first, since higher level pools may release
  // stuff to the lower level pools.

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_proxies_.begin();
       it != ssl_socket_pools_for_proxies_.end();
       ++it)
    it->second->Flush();

  for (HTTPProxySocketPoolMap::const_iterator it =
       http_proxy_socket_pools_.begin();
       it != http_proxy_socket_pools_.end();
       ++it)
    it->second->Flush();

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_https_proxies_.begin();
       it != ssl_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->Flush();

  for (TCPSocketPoolMap::const_iterator it =
       tcp_socket_pools_for_https_proxies_.begin();
       it != tcp_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->Flush();

  for (TCPSocketPoolMap::const_iterator it =
       tcp_socket_pools_for_http_proxies_.begin();
       it != tcp_socket_pools_for_http_proxies_.end();
       ++it)
    it->second->Flush();

  for (SOCKSSocketPoolMap::const_iterator it =
       socks_socket_pools_.begin();
       it != socks_socket_pools_.end();
       ++it)
    it->second->Flush();

  for (TCPSocketPoolMap::const_iterator it =
       tcp_socket_pools_for_socks_proxies_.begin();
       it != tcp_socket_pools_for_socks_proxies_.end();
       ++it)
    it->second->Flush();

  ssl_socket_pool_->Flush();
  tcp_socket_pool_->Flush();
}

void ClientSocketPoolManager::CloseIdleSockets() {
  // Close sockets in the highest level pools first, since higher level pools'
  // sockets may release stuff to the lower level pools.
  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_proxies_.begin();
       it != ssl_socket_pools_for_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (HTTPProxySocketPoolMap::const_iterator it =
       http_proxy_socket_pools_.begin();
       it != http_proxy_socket_pools_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_https_proxies_.begin();
       it != ssl_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TCPSocketPoolMap::const_iterator it =
       tcp_socket_pools_for_https_proxies_.begin();
       it != tcp_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TCPSocketPoolMap::const_iterator it =
       tcp_socket_pools_for_http_proxies_.begin();
       it != tcp_socket_pools_for_http_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (SOCKSSocketPoolMap::const_iterator it =
       socks_socket_pools_.begin();
       it != socks_socket_pools_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TCPSocketPoolMap::const_iterator it =
       tcp_socket_pools_for_socks_proxies_.begin();
       it != tcp_socket_pools_for_socks_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  ssl_socket_pool_->CloseIdleSockets();
  tcp_socket_pool_->CloseIdleSockets();
}

SOCKSClientSocketPool* ClientSocketPoolManager::GetSocketPoolForSOCKSProxy(
    const HostPortPair& socks_proxy) {
  SOCKSSocketPoolMap::const_iterator it = socks_socket_pools_.find(socks_proxy);
  if (it != socks_socket_pools_.end()) {
    DCHECK(ContainsKey(tcp_socket_pools_for_socks_proxies_, socks_proxy));
    return it->second;
  }

  DCHECK(!ContainsKey(tcp_socket_pools_for_socks_proxies_, socks_proxy));

  std::pair<TCPSocketPoolMap::iterator, bool> tcp_ret =
      tcp_socket_pools_for_socks_proxies_.insert(
          std::make_pair(
              socks_proxy,
              new TCPClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  &tcp_for_socks_pool_histograms_,
                  host_resolver_,
                  socket_factory_,
                  net_log_)));
  DCHECK(tcp_ret.second);

  std::pair<SOCKSSocketPoolMap::iterator, bool> ret =
      socks_socket_pools_.insert(
          std::make_pair(socks_proxy, new SOCKSClientSocketPool(
              g_max_sockets_per_proxy_server, g_max_sockets_per_group,
              &socks_pool_histograms_,
              host_resolver_,
              tcp_ret.first->second,
              net_log_)));

  return ret.first->second;
}

HttpProxyClientSocketPool* ClientSocketPoolManager::GetSocketPoolForHTTPProxy(
    const HostPortPair& http_proxy) {
  HTTPProxySocketPoolMap::const_iterator it =
      http_proxy_socket_pools_.find(http_proxy);
  if (it != http_proxy_socket_pools_.end()) {
    DCHECK(ContainsKey(tcp_socket_pools_for_http_proxies_, http_proxy));
    DCHECK(ContainsKey(tcp_socket_pools_for_https_proxies_, http_proxy));
    DCHECK(ContainsKey(ssl_socket_pools_for_https_proxies_, http_proxy));
    return it->second;
  }

  DCHECK(!ContainsKey(tcp_socket_pools_for_http_proxies_, http_proxy));
  DCHECK(!ContainsKey(tcp_socket_pools_for_https_proxies_, http_proxy));
  DCHECK(!ContainsKey(ssl_socket_pools_for_https_proxies_, http_proxy));

  std::pair<TCPSocketPoolMap::iterator, bool> tcp_http_ret =
      tcp_socket_pools_for_http_proxies_.insert(
          std::make_pair(
              http_proxy,
              new TCPClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  &tcp_for_http_proxy_pool_histograms_,
                  host_resolver_,
                  socket_factory_,
                  net_log_)));
  DCHECK(tcp_http_ret.second);

  std::pair<TCPSocketPoolMap::iterator, bool> tcp_https_ret =
      tcp_socket_pools_for_https_proxies_.insert(
          std::make_pair(
              http_proxy,
              new TCPClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  &tcp_for_https_proxy_pool_histograms_,
                  host_resolver_,
                  socket_factory_,
                  net_log_)));
  DCHECK(tcp_https_ret.second);

  std::pair<SSLSocketPoolMap::iterator, bool> ssl_https_ret =
      ssl_socket_pools_for_https_proxies_.insert(
          std::make_pair(
              http_proxy,
              new SSLClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  &ssl_for_https_proxy_pool_histograms_,
                  host_resolver_,
                  cert_verifier_,
                  dnsrr_resolver_,
                  dns_cert_checker_,
                  ssl_host_info_factory_,
                  socket_factory_,
                  tcp_https_ret.first->second /* https proxy */,
                  NULL /* no socks proxy */,
                  NULL /* no http proxy */,
                  ssl_config_service_, net_log_)));
  DCHECK(tcp_https_ret.second);

  std::pair<HTTPProxySocketPoolMap::iterator, bool> ret =
      http_proxy_socket_pools_.insert(
          std::make_pair(
              http_proxy,
              new HttpProxyClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  &http_proxy_pool_histograms_,
                  host_resolver_,
                  tcp_http_ret.first->second,
                  ssl_https_ret.first->second,
                  net_log_)));

  return ret.first->second;
}

SSLClientSocketPool* ClientSocketPoolManager::GetSocketPoolForSSLWithProxy(
    const HostPortPair& proxy_server) {
  SSLSocketPoolMap::const_iterator it =
      ssl_socket_pools_for_proxies_.find(proxy_server);
  if (it != ssl_socket_pools_for_proxies_.end())
    return it->second;

  SSLClientSocketPool* new_pool = new SSLClientSocketPool(
      g_max_sockets_per_proxy_server, g_max_sockets_per_group,
      &ssl_pool_histograms_,
      host_resolver_,
      cert_verifier_,
      dnsrr_resolver_,
      dns_cert_checker_,
      ssl_host_info_factory_,
      socket_factory_,
      NULL, /* no tcp pool, we always go through a proxy */
      GetSocketPoolForSOCKSProxy(proxy_server),
      GetSocketPoolForHTTPProxy(proxy_server),
      ssl_config_service_,
      net_log_);

  std::pair<SSLSocketPoolMap::iterator, bool> ret =
      ssl_socket_pools_for_proxies_.insert(std::make_pair(proxy_server,
                                                          new_pool));

  return ret.first->second;
}

// static
int ClientSocketPoolManager::max_sockets_per_group() {
  return g_max_sockets_per_group;
}

// static
void ClientSocketPoolManager::set_max_sockets_per_group(int socket_count) {
  DCHECK_LT(0, socket_count);
  // The following is a sanity check... but we should NEVER be near this value.
  DCHECK_GT(100, socket_count);
  g_max_sockets_per_group = socket_count;

  DCHECK_GE(g_max_sockets, g_max_sockets_per_group);
  DCHECK_GE(g_max_sockets_per_proxy_server, g_max_sockets_per_group);
}

// static
void ClientSocketPoolManager::set_max_sockets_per_proxy_server(
    int socket_count) {
  DCHECK_LT(0, socket_count);
  DCHECK_GT(100, socket_count);  // Sanity check.
  // Assert this case early on. The max number of sockets per group cannot
  // exceed the max number of sockets per proxy server.
  DCHECK_LE(g_max_sockets_per_group, socket_count);
  g_max_sockets_per_proxy_server = socket_count;
}

Value* ClientSocketPoolManager::SocketPoolInfoToValue() const {
  ListValue* list = new ListValue();
  list->Append(tcp_socket_pool_->GetInfoAsValue("tcp_socket_pool",
                                                "tcp_socket_pool",
                                                false));
  // Third parameter is false because |ssl_socket_pool_| uses |tcp_socket_pool_|
  // internally, and do not want to add it a second time.
  list->Append(ssl_socket_pool_->GetInfoAsValue("ssl_socket_pool",
                                                "ssl_socket_pool",
                                                false));
  AddSocketPoolsToList(list,
                       http_proxy_socket_pools_,
                       "http_proxy_socket_pool",
                       true);
  AddSocketPoolsToList(list,
                       socks_socket_pools_,
                       "socks_socket_pool",
                       true);

  // Third parameter is false because |ssl_socket_pools_for_proxies_| use
  // socket pools in |http_proxy_socket_pools_| and |socks_socket_pools_|.
  AddSocketPoolsToList(list,
                       ssl_socket_pools_for_proxies_,
                       "ssl_socket_pool_for_proxies",
                       false);
  return list;
}

void ClientSocketPoolManager::OnUserCertAdded(X509Certificate* cert) {
  FlushSocketPools();
}

}  // namespace net
