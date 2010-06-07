// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_POOL_H_
#define NET_SPDY_SPDY_SESSION_POOL_H_

#include <map>
#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_change_notifier.h"

namespace net {

class BoundNetLog;
class ClientSocketHandle;
class HttpNetworkSession;
class NetworkChangeNotifier;
class SpdySession;

// This is a very simple pool for open SpdySessions.
// TODO(mbelshe): Make this production ready.
class SpdySessionPool
    : public base::RefCounted<SpdySessionPool>,
      public NetworkChangeNotifier::Observer {
 public:
  explicit SpdySessionPool(NetworkChangeNotifier* notifier);

  // Either returns an existing SpdySession or creates a new SpdySession for
  // use.
  scoped_refptr<SpdySession> Get(
      const HostPortPair& host_port_pair, HttpNetworkSession* session,
      const BoundNetLog& net_log);

  // Set the maximum concurrent sessions per domain.
  static void set_max_sessions_per_domain(int max) {
    if (max >= 1)
      g_max_sessions_per_domain = max;
  }

  // Builds a SpdySession from an existing SSL socket.  Users should try
  // calling Get() first to use an existing SpdySession so we don't get
  // multiple SpdySessions per domain.  Note that ownership of |connection| is
  // transferred from the caller to the SpdySession.
  scoped_refptr<SpdySession> GetSpdySessionFromSSLSocket(
      const HostPortPair& host_port_pair,
      HttpNetworkSession* session,
      ClientSocketHandle* connection,
      const BoundNetLog& net_log);

  // TODO(willchan): Consider renaming to HasReusableSession, since perhaps we
  // should be creating a new session.
  bool HasSession(const HostPortPair& host_port_pair)const;

  // Close all Spdy Sessions; used for debugging.
  void CloseAllSessions() { RemoveAllSessions(true); }

  // Removes a SpdySession from the SpdySessionPool.
  void Remove(const scoped_refptr<SpdySession>& session);

  // NetworkChangeNotifier::Observer methods:

  // We flush all idle sessions and release references to the active ones so
  // they won't get re-used.  The active ones will either complete successfully
  // or error out due to the IP address change.
  virtual void OnIPAddressChanged() { ClearSessions(); }

 private:
  friend class base::RefCounted<SpdySessionPool>;
  friend class SpdySessionPoolPeer;  // For testing.
  friend class SpdyNetworkTransactionTest;  // For testing.

  typedef std::list<scoped_refptr<SpdySession> > SpdySessionList;
  typedef std::map<HostPortPair, SpdySessionList*> SpdySessionsMap;

  virtual ~SpdySessionPool();

  // Helper functions for manipulating the lists.
  SpdySessionList* AddSessionList(const HostPortPair& host_port_pair);
  SpdySessionList* GetSessionList(const HostPortPair& host_port_pair);
  const SpdySessionList* GetSessionList(
      const HostPortPair& host_port_pair) const;
  void RemoveSessionList(const HostPortPair& host_port_pair);
  // Releases the SpdySessionPool reference to all sessions.  Will result in all
  // idle sessions being deleted, and the active sessions from being reused, so
  // they will be deleted once all active streams belonging to that session go
  // away.
  void ClearSessions() { RemoveAllSessions(false); }
  void RemoveAllSessions(bool close);

  // This is our weak session pool - one session per domain.
  SpdySessionsMap sessions_;

  NetworkChangeNotifier* const network_change_notifier_;

  static int g_max_sessions_per_domain;

  DISALLOW_COPY_AND_ASSIGN(SpdySessionPool);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_POOL_H_
