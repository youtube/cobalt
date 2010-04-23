// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include "base/logging.h"
#include "net/spdy/spdy_session.h"

namespace net {

// The maximum number of sessions to open to a single domain.
static const size_t kMaxSessionsPerDomain = 1;

int SpdySessionPool::g_max_sessions_per_domain = kMaxSessionsPerDomain;

SpdySessionPool::SpdySessionPool() {}
SpdySessionPool::~SpdySessionPool() {
  CloseAllSessions();
}

scoped_refptr<SpdySession> SpdySessionPool::Get(
    const HostPortPair& host_port_pair, HttpNetworkSession* session) {
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
    spdy_session = new SpdySession(host_port_pair, session);

  DCHECK(spdy_session);
  list->push_back(spdy_session);
  DCHECK_LE(list->size(), static_cast<unsigned int>(g_max_sessions_per_domain));
  return spdy_session;
}

scoped_refptr<SpdySession> SpdySessionPool::GetSpdySessionFromSSLSocket(
    const HostPortPair& host_port_pair,
    HttpNetworkSession* session,
    ClientSocketHandle* connection) {
  SpdySessionList* list = GetSessionList(host_port_pair);
  if (!list)
    list = AddSessionList(host_port_pair);
  DCHECK(list->empty());
  scoped_refptr<SpdySession> spdy_session(
      new SpdySession(host_port_pair, session));
  spdy_session->InitializeWithSSLSocket(connection);
  list->push_back(spdy_session);
  return spdy_session;
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

SpdySessionPool::SpdySessionList*
    SpdySessionPool::AddSessionList(const HostPortPair& host_port_pair) {
  DCHECK(sessions_.find(host_port_pair) == sessions_.end());
  return sessions_[host_port_pair] = new SpdySessionList();
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

void SpdySessionPool::CloseAllSessions() {
  while (sessions_.size()) {
    SpdySessionList* list = sessions_.begin()->second;
    DCHECK(list);
    sessions_.erase(sessions_.begin()->first);
    while (list->size()) {
      scoped_refptr<SpdySession> session = list->front();
      list->pop_front();
      session->CloseAllStreams(net::ERR_ABORTED);
    }
    delete list;
  }
}

}  // namespace net
