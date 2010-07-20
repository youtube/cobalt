// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

namespace net {

class BoundNetLog;
class ClientSocketHandle;
class HttpNetworkSession;
class SpdySession;

// This is a very simple pool for open SpdySessions.
// TODO(mbelshe): Make this production ready.
class SpdySessionPool
    : public base::RefCounted<SpdySessionPool>,
      public NetworkChangeNotifier::Observer {
 public:
  SpdySessionPool();

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
  // |certificate_error_code| is used to indicate the certificate error
  // encountered when connecting the SSL socket.  OK means there was no error.
  // Returns OK on success, and the |spdy_session| will be provided.
  // Returns an error on failure, and |spdy_session| will be NULL.
  net::Error GetSpdySessionFromSSLSocket(
      const HostPortPair& host_port_pair,
      HttpNetworkSession* session,
      ClientSocketHandle* connection,
      const BoundNetLog& net_log,
      int certificate_error_code,
      scoped_refptr<SpdySession>* spdy_session);

  // TODO(willchan): Consider renaming to HasReusableSession, since perhaps we
  // should be creating a new session.
  bool HasSession(const HostPortPair& host_port_pair)const;

  // Close all Spdy Sessions; used for debugging.
  void CloseAllSessions();

  // Removes a SpdySession from the SpdySessionPool.
  void Remove(const scoped_refptr<SpdySession>& session);

  // NetworkChangeNotifier::Observer methods:

  // We flush all idle sessions and release references to the active ones so
  // they won't get re-used.  The active ones will either complete successfully
  // or error out due to the IP address change.
  virtual void OnIPAddressChanged();

 private:
  friend class base::RefCounted<SpdySessionPool>;
  friend class SpdySessionPoolPeer;  // For testing.
  friend class SpdyNetworkTransactionTest;  // For testing.
  FRIEND_TEST(SpdyNetworkTransactionTest, WindowUpdateOverflow);

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
  void ClearSessions();

  // This is our weak session pool - one session per domain.
  SpdySessionsMap sessions_;

  static int g_max_sessions_per_domain;

  DISALLOW_COPY_AND_ASSIGN(SpdySessionPool);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_POOL_H_
