// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/sys_addrinfo.h"
#include "net/http/http_network_session.h"
#include "net/spdy/spdy_session.h"


namespace net {

namespace {

enum SpdySessionGetTypes {
  CREATED_NEW                 = 0,
  FOUND_EXISTING              = 1,
  FOUND_EXISTING_FROM_IP_POOL = 2,
  IMPORTED_FROM_SOCKET        = 3,
  SPDY_SESSION_GET_MAX        = 4
};

bool HostPortProxyPairsAreEqual(const HostPortProxyPair& a,
                                const HostPortProxyPair& b) {
  return a.first.Equals(b.first) && a.second == b.second;
}

}

// The maximum number of sessions to open to a single domain.
static const size_t kMaxSessionsPerDomain = 1;

size_t SpdySessionPool::g_max_sessions_per_domain = kMaxSessionsPerDomain;
bool SpdySessionPool::g_force_single_domain = false;
bool SpdySessionPool::g_enable_ip_pooling = true;

SpdySessionPool::SpdySessionPool(HostResolver* resolver,
                                 SSLConfigService* ssl_config_service)
    : ssl_config_service_(ssl_config_service),
      resolver_(resolver) {
  NetworkChangeNotifier::AddIPAddressObserver(this);
  if (ssl_config_service_)
    ssl_config_service_->AddObserver(this);
  CertDatabase::AddObserver(this);
}

SpdySessionPool::~SpdySessionPool() {
  CloseAllSessions();

  if (ssl_config_service_)
    ssl_config_service_->RemoveObserver(this);
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  CertDatabase::RemoveObserver(this);
}

scoped_refptr<SpdySession> SpdySessionPool::Get(
    const HostPortProxyPair& host_port_proxy_pair,
    const BoundNetLog& net_log) {
  scoped_refptr<SpdySession> spdy_session;
  SpdySessionList* list = GetSessionList(host_port_proxy_pair);
  if (!list) {
    // Check if we have a Session through a domain alias.
    spdy_session = GetFromAlias(host_port_proxy_pair, net_log, true);
    if (spdy_session) {
      UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                                FOUND_EXISTING_FROM_IP_POOL,
                                SPDY_SESSION_GET_MAX);
      net_log.AddEvent(
          NetLog::TYPE_SPDY_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL,
          make_scoped_refptr(new NetLogSourceParameter(
          "session", spdy_session->net_log().source())));
      return spdy_session;
    }
    list = AddSessionList(host_port_proxy_pair);
  }

  DCHECK(list);
  if (list->size() && list->size() == g_max_sessions_per_domain) {
    UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                              FOUND_EXISTING,
                              SPDY_SESSION_GET_MAX);
    spdy_session = GetExistingSession(list, net_log);
    net_log.AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_FOUND_EXISTING_SESSION,
      make_scoped_refptr(new NetLogSourceParameter(
          "session", spdy_session->net_log().source())));
    return spdy_session;
  }

  spdy_session = new SpdySession(host_port_proxy_pair, this, &spdy_settings_,
                                 net_log.net_log());
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                            CREATED_NEW,
                            SPDY_SESSION_GET_MAX);
  list->push_back(spdy_session);
  net_log.AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_CREATED_NEW_SESSION,
      make_scoped_refptr(new NetLogSourceParameter(
          "session", spdy_session->net_log().source())));
  DCHECK_LE(list->size(), g_max_sessions_per_domain);
  return spdy_session;
}

net::Error SpdySessionPool::GetSpdySessionFromSocket(
    const HostPortProxyPair& host_port_proxy_pair,
    ClientSocketHandle* connection,
    const BoundNetLog& net_log,
    int certificate_error_code,
    scoped_refptr<SpdySession>* spdy_session,
    bool is_secure) {
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                            IMPORTED_FROM_SOCKET,
                            SPDY_SESSION_GET_MAX);
  // Create the SPDY session and add it to the pool.
  *spdy_session = new SpdySession(host_port_proxy_pair, this, &spdy_settings_,
                                  net_log.net_log());
  SpdySessionList* list = GetSessionList(host_port_proxy_pair);
  if (!list)
    list = AddSessionList(host_port_proxy_pair);
  DCHECK(list->empty());
  list->push_back(*spdy_session);

  net_log.AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_IMPORTED_SESSION_FROM_SOCKET,
      make_scoped_refptr(new NetLogSourceParameter(
          "session", (*spdy_session)->net_log().source())));

  // Now we can initialize the session with the SSL socket.
  return (*spdy_session)->InitializeWithSocket(connection, is_secure,
                                               certificate_error_code);
}

bool SpdySessionPool::HasSession(
    const HostPortProxyPair& host_port_proxy_pair) const {
  if (GetSessionList(host_port_proxy_pair))
    return true;

  // Check if we have a session via an alias.
  scoped_refptr<SpdySession> spdy_session =
      GetFromAlias(host_port_proxy_pair, BoundNetLog(), false);
  return spdy_session.get() != NULL;
}

void SpdySessionPool::Remove(const scoped_refptr<SpdySession>& session) {
  SpdySessionList* list = GetSessionList(session->host_port_proxy_pair());
  DCHECK(list);  // We really shouldn't remove if we've already been removed.
  if (!list)
    return;
  list->remove(session);
  session->net_log().AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_REMOVE_SESSION,
      make_scoped_refptr(new NetLogSourceParameter(
          "session", session->net_log().source())));
  if (list->empty())
    RemoveSessionList(session->host_port_proxy_pair());
}

Value* SpdySessionPool::SpdySessionPoolInfoToValue() const {
  ListValue* list = new ListValue();

  SpdySessionsMap::const_iterator spdy_session_pool_it = sessions_.begin();
  for (SpdySessionsMap::const_iterator it = sessions_.begin();
       it != sessions_.end(); ++it) {
    SpdySessionList* sessions = it->second;
    for (SpdySessionList::const_iterator session = sessions->begin();
         session != sessions->end(); ++session) {
      list->Append(session->get()->GetInfoAsValue());
    }
  }
  return list;
}

void SpdySessionPool::OnIPAddressChanged() {
  CloseCurrentSessions();
  spdy_settings_.Clear();
}

void SpdySessionPool::OnSSLConfigChanged() {
  CloseCurrentSessions();
}

scoped_refptr<SpdySession> SpdySessionPool::GetExistingSession(
    SpdySessionList* list,
    const BoundNetLog& net_log) const {
  DCHECK(list);
  DCHECK_LT(0u, list->size());
  scoped_refptr<SpdySession> spdy_session = list->front();
  if (list->size() > 1) {
    list->pop_front();  // Rotate the list.
    list->push_back(spdy_session);
  }

  return spdy_session;
}

scoped_refptr<SpdySession> SpdySessionPool::GetFromAlias(
      const HostPortProxyPair& host_port_proxy_pair,
      const BoundNetLog& net_log,
      bool record_histograms) const {
  // We should only be checking aliases when there is no direct session.
  DCHECK(!GetSessionList(host_port_proxy_pair));

  if (!g_enable_ip_pooling)
    return NULL;

  AddressList addresses;
  if (!LookupAddresses(host_port_proxy_pair, &addresses))
    return NULL;
  const addrinfo* address = addresses.head();
  while (address) {
    IPEndPoint endpoint;
    endpoint.FromSockAddr(address->ai_addr, address->ai_addrlen);
    address = address->ai_next;

    SpdyAliasMap::const_iterator it = aliases_.find(endpoint);
    if (it == aliases_.end())
      continue;

    // We found an alias.
    const HostPortProxyPair& alias_pair = it->second;

    // If the proxy settings match, we can reuse this session.
    if (!(alias_pair.second == host_port_proxy_pair.second))
      continue;

    SpdySessionList* list = GetSessionList(alias_pair);
    if (!list) {
      NOTREACHED();  // It shouldn't be in the aliases table if we can't get it!
      continue;
    }

    scoped_refptr<SpdySession> spdy_session = GetExistingSession(list, net_log);
    // If the SPDY session is a secure one, we need to verify that the server
    // is authenticated to serve traffic for |host_port_proxy_pair| too.
    if (!spdy_session->VerifyDomainAuthentication(
            host_port_proxy_pair.first.host())) {
      if (record_histograms)
        UMA_HISTOGRAM_ENUMERATION("Net.SpdyIPPoolDomainMatch", 0, 2);
      continue;
    }
    if (record_histograms)
      UMA_HISTOGRAM_ENUMERATION("Net.SpdyIPPoolDomainMatch", 1, 2);
    return spdy_session;
  }
  return NULL;
}

void SpdySessionPool::OnUserCertAdded(const X509Certificate* cert) {
  CloseCurrentSessions();
}

void SpdySessionPool::OnCertTrustChanged(const X509Certificate* cert) {
  // Per wtc, we actually only need to CloseCurrentSessions when trust is
  // reduced. CloseCurrentSessions now because OnCertTrustChanged does not
  // tell us this.
  // See comments in ClientSocketPoolManager::OnCertTrustChanged.
  CloseCurrentSessions();
}

const HostPortProxyPair& SpdySessionPool::NormalizeListPair(
    const HostPortProxyPair& host_port_proxy_pair) const {
  if (!g_force_single_domain)
    return host_port_proxy_pair;

  static HostPortProxyPair* single_domain_pair = NULL;
  if (!single_domain_pair) {
    HostPortPair single_domain = HostPortPair("singledomain.com", 80);
    single_domain_pair = new HostPortProxyPair(single_domain,
                                               ProxyServer::Direct());
  }
  return *single_domain_pair;
}

SpdySessionPool::SpdySessionList*
    SpdySessionPool::AddSessionList(
        const HostPortProxyPair& host_port_proxy_pair) {
  const HostPortProxyPair& pair = NormalizeListPair(host_port_proxy_pair);
  DCHECK(sessions_.find(pair) == sessions_.end());
  SpdySessionPool::SpdySessionList* list = new SpdySessionList();
  sessions_[pair] = list;

  // We have a new session.  Lookup the IP addresses for this session so that
  // we can match future Sessions (potentially to different domains) which can
  // potentially be pooled with this one.
  if (g_enable_ip_pooling) {
    AddressList addresses;
    if (LookupAddresses(host_port_proxy_pair, &addresses))
      AddAliases(addresses, host_port_proxy_pair);
  }

  return list;
}

SpdySessionPool::SpdySessionList*
    SpdySessionPool::GetSessionList(
        const HostPortProxyPair& host_port_proxy_pair) const {
  const HostPortProxyPair& pair = NormalizeListPair(host_port_proxy_pair);
  SpdySessionsMap::const_iterator it = sessions_.find(pair);
  if (it != sessions_.end())
    return it->second;
  return NULL;
}

void SpdySessionPool::RemoveSessionList(
    const HostPortProxyPair& host_port_proxy_pair) {
  const HostPortProxyPair& pair = NormalizeListPair(host_port_proxy_pair);
  SpdySessionList* list = GetSessionList(pair);
  if (list) {
    delete list;
    sessions_.erase(pair);
  } else {
    DCHECK(false) << "removing orphaned session list";
  }
  RemoveAliases(host_port_proxy_pair);
}

bool SpdySessionPool::LookupAddresses(const HostPortProxyPair& pair,
                                      AddressList* addresses) const {
  net::HostResolver::RequestInfo resolve_info(pair.first);
  resolve_info.set_only_use_cached_response(true);
  int rv = resolver_->Resolve(resolve_info,
                              addresses,
                              NULL,
                              NULL,
                              net::BoundNetLog());
  DCHECK_NE(ERR_IO_PENDING, rv);
  return rv == OK;
}

void SpdySessionPool::AddAliases(const AddressList& addresses,
                                 const HostPortProxyPair& pair) {
  // Note:  it is possible to think of strange overlapping sets of ip addresses
  // for hosts such that a new session can override the alias for an IP
  // address that was previously aliased to a different host.  This is probably
  // undesirable, but seemingly unlikely and complicated to fix.
  //    Example:
  //      host1 = 1.1.1.1, 1.1.1.4
  //      host2 = 1.1.1.4, 1.1.1.5
  //      host3 = 1.1.1.3, 1.1.1.5
  //      Creating session1 (to host1), creates an alias for host2 to host1.
  //      Creating session2 (to host3), overrides the alias for host2 to host3.

  const addrinfo* address = addresses.head();
  while (address) {
    IPEndPoint endpoint;
    endpoint.FromSockAddr(address->ai_addr, address->ai_addrlen);
    aliases_[endpoint] = pair;
    address = address->ai_next;
  }
}

void SpdySessionPool::RemoveAliases(const HostPortProxyPair& pair) {
  // Walk the aliases map, find references to this pair.
  // TODO(mbelshe):  Figure out if this is too expensive.
  SpdyAliasMap::iterator alias_it = aliases_.begin();
  while (alias_it != aliases_.end()) {
    if (HostPortProxyPairsAreEqual(alias_it->second, pair)) {
      aliases_.erase(alias_it);
      alias_it = aliases_.begin();  // Iterator was invalidated.
      continue;
    }
    ++alias_it;
  }
}

void SpdySessionPool::CloseAllSessions() {
  while (!sessions_.empty()) {
    SpdySessionList* list = sessions_.begin()->second;
    CHECK(list);
    const scoped_refptr<SpdySession>& session = list->front();
    CHECK(session);
    // This call takes care of removing the session from the pool, as well as
    // removing the session list if the list is empty.
    session->CloseSessionOnError(net::ERR_ABORTED, true);
  }
}

void SpdySessionPool::CloseCurrentSessions() {
  SpdySessionsMap old_map;
  old_map.swap(sessions_);
  for (SpdySessionsMap::const_iterator it = old_map.begin();
       it != old_map.end(); ++it) {
    SpdySessionList* list = it->second;
    CHECK(list);
    const scoped_refptr<SpdySession>& session = list->front();
    CHECK(session);
    session->set_spdy_session_pool(NULL);
  }

  while (!old_map.empty()) {
    SpdySessionList* list = old_map.begin()->second;
    CHECK(list);
    const scoped_refptr<SpdySession>& session = list->front();
    CHECK(session);
    session->CloseSessionOnError(net::ERR_ABORTED, false);
    list->pop_front();
    if (list->empty()) {
      delete list;
      RemoveAliases(old_map.begin()->first);
      old_map.erase(old_map.begin()->first);
    }
  }
  DCHECK(sessions_.empty());
  DCHECK(aliases_.empty());
}

}  // namespace net
