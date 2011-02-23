// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include "base/logging.h"
#include "base/values.h"
#include "net/http/http_network_session.h"
#include "net/spdy/spdy_session.h"

namespace net {

// The maximum number of sessions to open to a single domain.
static const size_t kMaxSessionsPerDomain = 1;

int SpdySessionPool::g_max_sessions_per_domain = kMaxSessionsPerDomain;
bool SpdySessionPool::g_force_single_domain = false;

SpdySessionPool::SpdySessionPool(SSLConfigService* ssl_config_service)
    : ssl_config_service_(ssl_config_service) {
  NetworkChangeNotifier::AddObserver(this);
  if (ssl_config_service_)
    ssl_config_service_->AddObserver(this);
}

SpdySessionPool::~SpdySessionPool() {
  CloseAllSessions();

  if (ssl_config_service_)
    ssl_config_service_->RemoveObserver(this);
  NetworkChangeNotifier::RemoveObserver(this);
}

scoped_refptr<SpdySession> SpdySessionPool::Get(
    const HostPortProxyPair& host_port_proxy_pair,
    SpdySettingsStorage* spdy_settings,
    const BoundNetLog& net_log) {
  scoped_refptr<SpdySession> spdy_session;
  SpdySessionList* list = GetSessionList(host_port_proxy_pair);
  if (list) {
    if (list->size() >= static_cast<unsigned int>(g_max_sessions_per_domain)) {
      spdy_session = list->front();
      list->pop_front();
      net_log.AddEvent(
          NetLog::TYPE_SPDY_SESSION_POOL_FOUND_EXISTING_SESSION,
          make_scoped_refptr(new NetLogSourceParameter(
              "session", spdy_session->net_log().source())));
    }
  } else {
    list = AddSessionList(host_port_proxy_pair);
  }

  DCHECK(list);
  if (!spdy_session) {
    spdy_session = new SpdySession(host_port_proxy_pair, this, spdy_settings,
                                   net_log.net_log());
    net_log.AddEvent(
        NetLog::TYPE_SPDY_SESSION_POOL_CREATED_NEW_SESSION,
        make_scoped_refptr(new NetLogSourceParameter(
            "session", spdy_session->net_log().source())));
  }

  DCHECK(spdy_session);
  list->push_back(spdy_session);
  DCHECK_LE(list->size(), static_cast<unsigned int>(g_max_sessions_per_domain));
  return spdy_session;
}

net::Error SpdySessionPool::GetSpdySessionFromSocket(
    const HostPortProxyPair& host_port_proxy_pair,
    SpdySettingsStorage* spdy_settings,
    ClientSocketHandle* connection,
    const BoundNetLog& net_log,
    int certificate_error_code,
    scoped_refptr<SpdySession>* spdy_session,
    bool is_secure) {
  // Create the SPDY session and add it to the pool.
  *spdy_session = new SpdySession(host_port_proxy_pair, this, spdy_settings,
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
  return false;
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
}

void SpdySessionPool::OnSSLConfigChanged() {
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
  if (it == sessions_.end())
    return NULL;
  return it->second;
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
      old_map.erase(old_map.begin()->first);
    }
  }
}

}  // namespace net
