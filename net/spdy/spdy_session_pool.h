// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_POOL_H_
#define NET_SPDY_SPDY_SESSION_POOL_H_

#include <map>
#include <list>
#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/host_resolver.h"

namespace net {

class ClientSocketHandle;
class HttpNetworkSession;
class SpdySession;

// This is a very simple pool for open SpdySessions.
// TODO(mbelshe): Make this production ready.
class SpdySessionPool : public base::RefCounted<SpdySessionPool> {
 public:
  SpdySessionPool();

  // Either returns an existing SpdySession or creates a new SpdySession for
  // use.
  scoped_refptr<SpdySession> Get(
      const HostResolver::RequestInfo& info, HttpNetworkSession* session);

  // Builds a SpdySession from an existing SSL socket.  Users should try
  // calling Get() first to use an existing SpdySession so we don't get
  // multiple SpdySessions per domain.  Note that ownership of |connection| is
  // transferred from the caller to the SpdySession.
  scoped_refptr<SpdySession> GetSpdySessionFromSSLSocket(
      const HostResolver::RequestInfo& info,
      HttpNetworkSession* session,
      ClientSocketHandle* connection);

  // TODO(willchan): Consider renaming to HasReusableSession, since perhaps we
  // should be creating a new session.
  bool HasSession(const HostResolver::RequestInfo& info) const;

  // Close all Spdy Sessions; used for debugging.
  void CloseAllSessions();

 private:
  friend class base::RefCounted<SpdySessionPool>;
  friend class SpdySession;  // Needed for Remove().
  friend class SpdySessionPoolPeer;  // For testing.

  typedef std::list<scoped_refptr<SpdySession> > SpdySessionList;
  typedef std::map<std::string, SpdySessionList*> SpdySessionsMap;

  virtual ~SpdySessionPool();

  // Removes a SpdySession from the SpdySessionPool.
  void Remove(const scoped_refptr<SpdySession>& session);

  // Helper functions for manipulating the lists.
  SpdySessionList* AddSessionList(const std::string& domain);
  SpdySessionList* GetSessionList(const std::string& domain);
  const SpdySessionList* GetSessionList(const std::string& domain) const;
  void RemoveSessionList(const std::string& domain);

  // This is our weak session pool - one session per domain.
  SpdySessionsMap sessions_;

  DISALLOW_COPY_AND_ASSIGN(SpdySessionPool);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_POOL_H_
