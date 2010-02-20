// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include "base/logging.h"
#include "net/spdy/spdy_session.h"

namespace net {

// The maximum number of sessions to open to a single domain.
static const size_t kMaxSessionsPerDomain = 1;

SpdySessionPool::SpdySessionPool() {}
SpdySessionPool::~SpdySessionPool() {
  CloseAllSessions();
}

scoped_refptr<SpdySession> SpdySessionPool::Get(
    const HostResolver::RequestInfo& info, HttpNetworkSession* session) {
  const std::string& domain = info.hostname();
  scoped_refptr<SpdySession> spdy_session;
  SpdySessionList* list = GetSessionList(domain);
  if (list) {
    if (list->size() >= kMaxSessionsPerDomain) {
      spdy_session = list->front();
      list->pop_front();
    }
  } else {
    list = AddSessionList(domain);
  }

  DCHECK(list);
  if (!spdy_session)
    spdy_session = new SpdySession(domain, session);

  DCHECK(spdy_session);
  list->push_back(spdy_session);
  DCHECK(list->size() <= kMaxSessionsPerDomain);
  return spdy_session;
}

scoped_refptr<SpdySession> SpdySessionPool::GetSpdySessionFromSSLSocket(
    const HostResolver::RequestInfo& info,
    HttpNetworkSession* session,
    ClientSocketHandle* connection) {
  const std::string& domain = info.hostname();
  SpdySessionList* list = GetSessionList(domain);
  if (!list)
    list = AddSessionList(domain);
  DCHECK(list->empty());
  scoped_refptr<SpdySession> spdy_session(new SpdySession(domain, session));
  spdy_session->InitializeWithSSLSocket(connection);
  list->push_back(spdy_session);
  return spdy_session;
}

bool SpdySessionPool::HasSession(const HostResolver::RequestInfo& info) const {
  const std::string& domain = info.hostname();
  if (GetSessionList(domain))
    return true;
  return false;
}

void SpdySessionPool::Remove(const scoped_refptr<SpdySession>& session) {
  std::string domain = session->domain();
  SpdySessionList* list = GetSessionList(domain);
  CHECK(list);
  list->remove(session);
  if (list->empty())
    RemoveSessionList(domain);
}

SpdySessionPool::SpdySessionList*
    SpdySessionPool::AddSessionList(const std::string& domain) {
  DCHECK(sessions_.find(domain) == sessions_.end());
  return sessions_[domain] = new SpdySessionList();
}

SpdySessionPool::SpdySessionList*
    SpdySessionPool::GetSessionList(const std::string& domain) {
  SpdySessionsMap::iterator it = sessions_.find(domain);
  if (it == sessions_.end())
    return NULL;
  return it->second;
}

const SpdySessionPool::SpdySessionList*
    SpdySessionPool::GetSessionList(const std::string& domain) const {
  SpdySessionsMap::const_iterator it = sessions_.find(domain);
  if (it == sessions_.end())
    return NULL;
  return it->second;
}

void SpdySessionPool::RemoveSessionList(const std::string& domain) {
  SpdySessionList* list = GetSessionList(domain);
  if (list) {
    delete list;
    sessions_.erase(domain);
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
