// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
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

SpdySessionPool::SpdySessionPool(
    HostResolver* resolver,
    SSLConfigService* ssl_config_service,
    HttpServerProperties* http_server_properties,
    const std::string& trusted_spdy_proxy)
    : http_server_properties_(http_server_properties),
      ssl_config_service_(ssl_config_service),
      resolver_(resolver),
      verify_domain_authentication_(true),
      enable_sending_initial_settings_(true),
      trusted_spdy_proxy_(
          HostPortPair::FromString(trusted_spdy_proxy)) {
  NetworkChangeNotifier::AddIPAddressObserver(this);
  if (ssl_config_service_)
    ssl_config_service_->AddObserver(this);
  CertDatabase::GetInstance()->AddObserver(this);
}

SpdySessionPool::~SpdySessionPool() {
  CloseAllSessions();

  if (ssl_config_service_)
    ssl_config_service_->RemoveObserver(this);
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  CertDatabase::GetInstance()->RemoveObserver(this);
}

scoped_refptr<SpdySession> SpdySessionPool::Get(
    const HostPortProxyPair& host_port_proxy_pair,
    const BoundNetLog& net_log) {
  return GetInternal(host_port_proxy_pair, net_log, false);
}

scoped_refptr<SpdySession> SpdySessionPool::GetIfExists(
    const HostPortProxyPair& host_port_proxy_pair,
    const BoundNetLog& net_log) {
  return GetInternal(host_port_proxy_pair, net_log, true);
}

scoped_refptr<SpdySession> SpdySessionPool::GetInternal(
    const HostPortProxyPair& host_port_proxy_pair,
    const BoundNetLog& net_log,
    bool only_use_existing_sessions) {
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
          spdy_session->net_log().source().ToEventParametersCallback());
      // Add this session to the map so that we can find it next time.
      list = AddSessionList(host_port_proxy_pair);
      list->push_back(spdy_session);
      spdy_session->AddPooledAlias(host_port_proxy_pair);
      return spdy_session;
    } else if (only_use_existing_sessions) {
      return NULL;
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
      spdy_session->net_log().source().ToEventParametersCallback());
    return spdy_session;
  }

  DCHECK(!only_use_existing_sessions);

  spdy_session = new SpdySession(host_port_proxy_pair, this,
                                 http_server_properties_,
                                 verify_domain_authentication_,
                                 enable_sending_initial_settings_,
                                 trusted_spdy_proxy_,
                                 net_log.net_log());
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                            CREATED_NEW,
                            SPDY_SESSION_GET_MAX);
  list->push_back(spdy_session);
  net_log.AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_CREATED_NEW_SESSION,
      spdy_session->net_log().source().ToEventParametersCallback());
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
  *spdy_session = new SpdySession(host_port_proxy_pair, this,
                                  http_server_properties_,
                                  verify_domain_authentication_,
                                  enable_sending_initial_settings_,
                                  trusted_spdy_proxy_,
                                  net_log.net_log());
  SpdySessionList* list = GetSessionList(host_port_proxy_pair);
  if (!list)
    list = AddSessionList(host_port_proxy_pair);
  DCHECK(list->empty());
  list->push_back(*spdy_session);

  net_log.AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_IMPORTED_SESSION_FROM_SOCKET,
      (*spdy_session)->net_log().source().ToEventParametersCallback());

  // We have a new session.  Lookup the IP address for this session so that we
  // can match future Sessions (potentially to different domains) which can
  // potentially be pooled with this one. Because GetPeerAddress() reports the
  // proxy's address instead of the origin server, check to see if this is a
  // direct connection.
  if (g_enable_ip_pooling  && host_port_proxy_pair.second.is_direct()) {
    IPEndPoint address;
    if (connection->socket()->GetPeerAddress(&address) == OK)
      AddAlias(address, host_port_proxy_pair);
  }

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
  bool ok = RemoveFromSessionList(session, session->host_port_proxy_pair());
  DCHECK(ok);
  session->net_log().AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_REMOVE_SESSION,
      session->net_log().source().ToEventParametersCallback());

  const std::set<HostPortProxyPair>& aliases = session->pooled_aliases();
  for (std::set<HostPortProxyPair>::const_iterator it = aliases.begin();
       it != aliases.end(); ++it) {
    ok = RemoveFromSessionList(session, *it);
    DCHECK(ok);
  }
}

bool SpdySessionPool::RemoveFromSessionList(
    const scoped_refptr<SpdySession>& session,
    const HostPortProxyPair& pair) {
  SpdySessionList* list = GetSessionList(pair);
  if (!list)
    return false;
  list->remove(session);
  if (list->empty())
    RemoveSessionList(pair);
  return true;
}

Value* SpdySessionPool::SpdySessionPoolInfoToValue() const {
  ListValue* list = new ListValue();

  for (SpdySessionsMap::const_iterator it = sessions_.begin();
       it != sessions_.end(); ++it) {
    SpdySessionList* sessions = it->second;
    for (SpdySessionList::const_iterator session = sessions->begin();
         session != sessions->end(); ++session) {
      // Only add the session if the key in the map matches the main
      // host_port_proxy_pair (not an alias).
      const HostPortProxyPair& key = it->first;
      const HostPortProxyPair& pair = session->get()->host_port_proxy_pair();
      if (key.first.Equals(pair.first) && key.second == pair.second)
        list->Append(session->get()->GetInfoAsValue());
    }
  }
  return list;
}

void SpdySessionPool::OnIPAddressChanged() {
  CloseCurrentSessions();
  http_server_properties_->ClearSpdySettings();
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
  if (!LookupAddresses(host_port_proxy_pair, net_log, &addresses))
    return NULL;
  for (AddressList::const_iterator iter = addresses.begin();
       iter != addresses.end();
       ++iter) {
    SpdyAliasMap::const_iterator alias_iter = aliases_.find(*iter);
    if (alias_iter == aliases_.end())
      continue;

    // We found an alias.
    const HostPortProxyPair& alias_pair = alias_iter->second;

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

void SpdySessionPool::OnCertAdded(const X509Certificate* cert) {
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
                                      const BoundNetLog& net_log,
                                      AddressList* addresses) const {
  net::HostResolver::RequestInfo resolve_info(pair.first);
  int rv = resolver_->ResolveFromCache(resolve_info, addresses, net_log);
  DCHECK_NE(ERR_IO_PENDING, rv);
  return rv == OK;
}

void SpdySessionPool::AddAlias(const IPEndPoint& endpoint,
                               const HostPortProxyPair& pair) {
  DCHECK(g_enable_ip_pooling);
  aliases_[endpoint] = pair;
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
    session->CloseSessionOnError(
        net::ERR_ABORTED, true, "Closing all sessions.");
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
    session->CloseSessionOnError(
        net::ERR_ABORTED, false, "Closing current sessions.");
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

void SpdySessionPool::CloseIdleSessions() {
  SpdySessionsMap::const_iterator map_it = sessions_.begin();
  while (map_it != sessions_.end()) {
    SpdySessionList* list = map_it->second;
    ++map_it;
    CHECK(list);

    // Assumes there is only 1 element in the list
    SpdySessionList::iterator session_it = list->begin();
    const scoped_refptr<SpdySession>& session = *session_it;
    CHECK(session);
    if (!session->is_active()) {
      session->CloseSessionOnError(
          net::ERR_ABORTED, true, "Closing idle sessions.");
    }
  }
}

}  // namespace net
