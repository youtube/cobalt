// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include "base/logging.h"
#include "net/spdy/spdy_session.h"

namespace net {

// The maximum number of sessions to open to a single domain.
static const size_t kMaxSessionsPerDomain = 1;

int SpdySessionPool::g_max_sessions_per_domain = kMaxSessionsPerDomain;

SpdySessionPool::SpdySessionPool() {
  NetworkChangeNotifier::AddObserver(this);
}

SpdySessionPool::~SpdySessionPool() {
  CloseAllSessions();

  NetworkChangeNotifier::RemoveObserver(this);
}

scoped_refptr<SpdySession> SpdySessionPool::Get(
    const HostPortPair& host_port_pair, HttpNetworkSession* session,
    const BoundNetLog& net_log) {
  scoped_refptr<SpdySession> spdy_session;
  SpdySessionList* list = GetSessionList(host_port_pair);
  if (list) {
    if (list->size() >= static_cast<unsigned int>(g_max_sessions_per_domain)) {
      spdy_session = list->front();
      list->pop_front();
    }
  } else {
    list = AddSessionList(host_port_pair);
  }

  DCHECK(list);
  if (!spdy_session)
    spdy_session = new SpdySession(host_port_pair, session, net_log.net_log());

  DCHECK(spdy_session);
  list->push_back(spdy_session);
  DCHECK_LE(list->size(), static_cast<unsigned int>(g_max_sessions_per_domain));
  return spdy_session;
}

net::Error SpdySessionPool::GetSpdySessionFromSSLSocket(
    const HostPortPair& host_port_pair,
    HttpNetworkSession* session,
    ClientSocketHandle* connection,
    const BoundNetLog& net_log,
    int certificate_error_code,
    scoped_refptr<SpdySession>* spdy_session) {
  // Create the SPDY session and add it to the pool.
  *spdy_session = new SpdySession(host_port_pair, session, net_log.net_log());
  SpdySessionList* list = GetSessionList(host_port_pair);
  if (!list)
    list = AddSessionList(host_port_pair);
  DCHECK(list->empty());
  list->push_back(*spdy_session);

  // Now we can initialize the session with the SSL socket.
  return (*spdy_session)->InitializeWithSSLSocket(connection,
                                                  certificate_error_code);
}

bool SpdySessionPool::HasSession(const HostPortPair& host_port_pair) const {
  if (GetSessionList(host_port_pair))
    return true;
  return false;
}

void SpdySessionPool::Remove(const scoped_refptr<SpdySession>& session) {
  SpdySessionList* list = GetSessionList(session->host_port_pair());
  DCHECK(list);  // We really shouldn't remove if we've already been removed.
  if (!list)
    return;
  list->remove(session);
  if (list->empty())
    RemoveSessionList(session->host_port_pair());
}

void SpdySessionPool::OnIPAddressChanged() {
  ClearSessions();
}

SpdySessionPool::SpdySessionList*
    SpdySessionPool::AddSessionList(const HostPortPair& host_port_pair) {
  DCHECK(sessions_.find(host_port_pair) == sessions_.end());
  SpdySessionPool::SpdySessionList* list = new SpdySessionList();
  sessions_[host_port_pair] = list;
  return list;
}

SpdySessionPool::SpdySessionList*
    SpdySessionPool::GetSessionList(const HostPortPair& host_port_pair) {
  SpdySessionsMap::iterator it = sessions_.find(host_port_pair);
  if (it == sessions_.end())
    return NULL;
  return it->second;
}

const SpdySessionPool::SpdySessionList*
    SpdySessionPool::GetSessionList(const HostPortPair& host_port_pair) const {
  SpdySessionsMap::const_iterator it = sessions_.find(host_port_pair);
  if (it == sessions_.end())
    return NULL;
  return it->second;
}

void SpdySessionPool::RemoveSessionList(const HostPortPair& host_port_pair) {
  SpdySessionList* list = GetSessionList(host_port_pair);
  if (list) {
    delete list;
    sessions_.erase(host_port_pair);
  } else {
    DCHECK(false) << "removing orphaned session list";
  }
}

void SpdySessionPool::ClearSessions() {
  while (!sessions_.empty()) {
    SpdySessionList* list = sessions_.begin()->second;
    DCHECK(list);
    sessions_.erase(sessions_.begin()->first);
    while (list->size()) {
      list->pop_front();
    }
    delete list;
  }
}

void SpdySessionPool::CloseAllSessions() {
  while (!sessions_.empty()) {
    SpdySessionList* list = sessions_.begin()->second;
    DCHECK(list);
    const scoped_refptr<SpdySession>& session = list->front();
    DCHECK(session);
    session->CloseSessionOnError(net::ERR_ABORTED);
  }
}

}  // namespace net
