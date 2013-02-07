// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_STREAM_FACTORY_H_
#define NET_QUIC_QUIC_STREAM_FACTORY_H_

#include <map>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_log.h"
#include "net/proxy/proxy_server.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_protocol.h"

namespace net {

class HostResolver;
class ClientSocketFactory;
class QuicClock;
class QuicClientSession;
class QuicStreamFactory;

// Encapsulates a pending request for a QuicHttpStream.
// If the request is still pending when it is destroyed, it will
// cancel the request with the factory.
class NET_EXPORT_PRIVATE QuicStreamRequest {
 public:
  explicit QuicStreamRequest(QuicStreamFactory* factory);
  ~QuicStreamRequest();

  int Request(const HostPortProxyPair& host_port_proxy_pair,
              const BoundNetLog& net_log,
              const CompletionCallback& callback);

  void OnRequestComplete(int rv);

  scoped_ptr<QuicHttpStream> ReleaseStream();

  void set_stream(scoped_ptr<QuicHttpStream> stream);

  const BoundNetLog& net_log() const{
    return net_log_;
  }

 private:
  QuicStreamFactory* factory_;
  HostPortProxyPair host_port_proxy_pair_;
  BoundNetLog net_log_;
  CompletionCallback callback_;
  scoped_ptr<QuicHttpStream> stream_;

  DISALLOW_COPY_AND_ASSIGN(QuicStreamRequest);
};

// A factory for creating new QuicHttpStreams on top of a pool of
// QuicClientSessions.
class NET_EXPORT_PRIVATE QuicStreamFactory {
 public:
  typedef base::Callback<uint64()> RandomUint64Callback;

  QuicStreamFactory(HostResolver* host_resolver,
                    ClientSocketFactory* client_socket_factory,
                    const RandomUint64Callback& random_uint64_callback,
                    QuicClock* clock);
  virtual ~QuicStreamFactory();

  // Creates a new QuicHttpStream to |host_port_proxy_pair| which will be
  // owned by |request|. If a matching session already exists, this
  // method will return OK.  If no matching session exists, this will
  // return ERR_IO_PENDING and will invoke OnRequestComplete asynchronously.
  int Create(const HostPortProxyPair& host_port_proxy_pair,
             const BoundNetLog& net_log,
             QuicStreamRequest* request);

  // Returns a newly created QuicHttpStream owned by the caller, if a
  // matching session already exists.  Returns NULL otherwise.
  scoped_ptr<QuicHttpStream> CreateIfSessionExists(
      const HostPortProxyPair& host_port_proxy_pair,
      const BoundNetLog& net_log);

  // Called by a session when it becomes idle.
  void OnIdleSession(QuicClientSession* session);

  // Called by a session after it shuts down.
  void OnSessionClose(QuicClientSession* session);

  // Cancels a pending request.
  void CancelRequest(QuicStreamRequest* request);

 private:
  class Job;

  typedef std::map<HostPortProxyPair, QuicClientSession*> SessionMap;
  typedef std::set<HostPortProxyPair> AliasSet;
  typedef std::map<QuicClientSession*, AliasSet> SessionAliasMap;
  typedef std::set<QuicClientSession*> SessionSet;
  typedef std::map<HostPortProxyPair, Job*> JobMap;
  typedef std::map<QuicStreamRequest*, Job*> RequestMap;
  typedef std::set<QuicStreamRequest*> RequestSet;
  typedef std::map<Job*, RequestSet> JobRequestsMap;

  void OnJobComplete(Job* job, int rv);
  bool HasActiveSession(const HostPortProxyPair& host_port_proxy_pair);
  bool HasActiveJob(const HostPortProxyPair& host_port_proxy_pair);
  QuicClientSession* CreateSession(const AddressList& address_list_,
                     const BoundNetLog& net_log);
  void ActivateSession(const HostPortProxyPair& host_port_proxy_pair,
                       QuicClientSession* session);

  HostResolver* host_resolver_;
  ClientSocketFactory* client_socket_factory_;
  RandomUint64Callback random_uint64_callback_;
  scoped_ptr<QuicClock> clock_;

  // Contains owning pointers to all sessions that currently exist.
  SessionSet all_sessions_;
  // Contains non-owning pointers to currently active session
  // (not going away session, once they're implemented).
  SessionMap active_sessions_;
  SessionAliasMap session_aliases_;

  JobMap active_jobs_;
  JobRequestsMap job_requests_map_;
  RequestMap active_requests_;

  base::WeakPtrFactory<QuicStreamFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicStreamFactory);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_STREAM_FACTORY_H_
