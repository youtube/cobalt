// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include <set>

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/single_request_host_resolver.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/socket/client_socket_factory.h"

namespace net {

// Responsible for creating a new QUIC session to the specified server, and
// for notifying any associated requests when complete.
class QuicStreamFactory::Job {
 public:
  Job(QuicStreamFactory* factory,
      HostResolver* host_resolver,
      const HostPortProxyPair& host_port_proxy_pair,
      const BoundNetLog& net_log);

  ~Job();

  int Run(const CompletionCallback& callback);

  int DoLoop(int rv);
  int DoResolveHost();
  int DoResolveHostComplete(int rv);
  int DoConnect();
  int DoConnectComplete(int rv);

  void OnIOComplete(int rv);

  CompletionCallback callback() {
    return callback_;
  }

  const HostPortProxyPair& host_port_proxy_pair() const {
    return host_port_proxy_pair_;
  }

 private:
  enum IoState {
    STATE_NONE,
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_CONNECT,
    STATE_CONNECT_COMPLETE,
  };
  IoState io_state_;

  QuicStreamFactory* factory_;
  SingleRequestHostResolver host_resolver_;
  const HostPortProxyPair host_port_proxy_pair_;
  const BoundNetLog net_log_;
  QuicClientSession* session_;
  CompletionCallback callback_;
  AddressList address_list_;
  DISALLOW_COPY_AND_ASSIGN(Job);
};

QuicStreamFactory::Job::Job(
    QuicStreamFactory* factory,
    HostResolver* host_resolver,
    const HostPortProxyPair& host_port_proxy_pair,
    const BoundNetLog& net_log)
    : factory_(factory),
      host_resolver_(host_resolver),
      host_port_proxy_pair_(host_port_proxy_pair),
      net_log_(net_log) {
}

QuicStreamFactory::Job::~Job() {
}

int QuicStreamFactory::Job::Run(const CompletionCallback& callback) {
  io_state_ = STATE_RESOLVE_HOST;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv > 0 ? OK : rv;
}

int QuicStreamFactory::Job::DoLoop(int rv) {
  do {
    IoState state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_HOST:
        CHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      case STATE_CONNECT:
        CHECK_EQ(OK, rv);
        rv = DoConnect();
        break;
      case STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      default:
        NOTREACHED() << "io_state_: " << io_state_;
        break;
    }
  } while (io_state_ != STATE_NONE && rv != ERR_IO_PENDING);
  return rv;
}

void QuicStreamFactory::Job::OnIOComplete(int rv) {
  rv = DoLoop(rv);

  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    callback_.Run(rv);
  }
}

int QuicStreamFactory::Job::DoResolveHost() {
  io_state_ = STATE_RESOLVE_HOST_COMPLETE;
  return host_resolver_.Resolve(
      HostResolver::RequestInfo(host_port_proxy_pair_.first), &address_list_,
      base::Bind(&QuicStreamFactory::Job::OnIOComplete,
                 base::Unretained(this)),
      net_log_);
}

int QuicStreamFactory::Job::DoResolveHostComplete(int rv) {
  if (rv != OK)
    return rv;

  if (address_list_.empty())
    return ERR_NAME_NOT_RESOLVED;

  DCHECK(!factory_->HasActiveSession(host_port_proxy_pair_));
  io_state_ = STATE_CONNECT;
  return OK;
}

QuicStreamRequest::QuicStreamRequest(QuicStreamFactory* factory)
    : factory_(factory),
      stream_(NULL){
}

QuicStreamRequest::~QuicStreamRequest() {
  if (factory_ && !callback_.is_null())
    factory_->CancelRequest(this);
}

int QuicStreamRequest::Request(
    const HostPortProxyPair& host_port_proxy_pair,
    const BoundNetLog& net_log,
    const CompletionCallback& callback) {
  DCHECK(!stream_);
  DCHECK(callback_.is_null());
  int rv = factory_->Create(host_port_proxy_pair, net_log, this);
  if (rv == ERR_IO_PENDING) {
    host_port_proxy_pair_ = host_port_proxy_pair;
    net_log_ = net_log;
    callback_ = callback;
  } else {
    factory_ = NULL;
  }
  return rv;
}

void QuicStreamRequest::set_stream(scoped_ptr<QuicHttpStream> stream) {
  stream_ = stream.Pass();
}

void QuicStreamRequest::OnRequestComplete(int rv) {
  factory_ = NULL;
  callback_.Run(rv);
}

scoped_ptr<QuicHttpStream> QuicStreamRequest::ReleaseStream() {
  DCHECK(stream_);
  return stream_.Pass();
}

int QuicStreamFactory::Job::DoConnect() {
  io_state_ = STATE_CONNECT_COMPLETE;

  session_ = factory_->CreateSession(address_list_, net_log_);
  session_->StartReading();
  int rv = session_->CryptoConnect(
      base::Bind(&QuicStreamFactory::Job::OnIOComplete,
                 base::Unretained(this)));
  return rv;
}

int QuicStreamFactory::Job::DoConnectComplete(int rv) {
  if (rv != OK)
    return rv;

  DCHECK(!factory_->HasActiveSession(host_port_proxy_pair_));
  factory_->ActivateSession(host_port_proxy_pair_, session_);

  return OK;
}

QuicStreamFactory::QuicStreamFactory(
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory,
    const RandomUint64Callback& random_uint64_callback,
    QuicClock* clock)
    : host_resolver_(host_resolver),
      client_socket_factory_(client_socket_factory),
      random_uint64_callback_(random_uint64_callback),
      clock_(clock),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

QuicStreamFactory::~QuicStreamFactory() {
  STLDeleteElements(&all_sessions_);
  STLDeleteValues(&active_jobs_);
}

int QuicStreamFactory::Create(const HostPortProxyPair& host_port_proxy_pair,
                              const BoundNetLog& net_log,
                              QuicStreamRequest* request) {
  if (HasActiveSession(host_port_proxy_pair)) {
    request->set_stream(CreateIfSessionExists(host_port_proxy_pair, net_log));
    return OK;
  }

  if (HasActiveJob(host_port_proxy_pair)) {
    Job* job = active_jobs_[host_port_proxy_pair];
    active_requests_[request] = job;
    job_requests_map_[job].insert(request);
    return ERR_IO_PENDING;
  }

  Job* job = new Job(weak_factory_.GetWeakPtr(), host_resolver_,
                     host_port_proxy_pair, net_log);
  int rv = job->Run(base::Bind(&QuicStreamFactory::OnJobComplete,
                               base::Unretained(this), job));

  if (rv == ERR_IO_PENDING) {
    active_jobs_[host_port_proxy_pair] = job;
    job_requests_map_[job].insert(request);
    active_requests_[request] = job;
  }
  if (rv == OK) {
    DCHECK(HasActiveSession(host_port_proxy_pair));
  }
  return rv;
}

void QuicStreamFactory::OnJobComplete(Job* job, int rv) {
  if (rv == OK) {
    // Create all the streams, but do not notify them yet.
    for (RequestSet::iterator it = job_requests_map_[job].begin();
         it != job_requests_map_[job].end() ; ++it) {
      (*it)->set_stream(CreateIfSessionExists(job->host_port_proxy_pair(),
                                              (*it)->net_log()));
    }
  }
  while (!job_requests_map_[job].empty()) {
    RequestSet::iterator it = job_requests_map_[job].begin();
    QuicStreamRequest* request = *it;
    job_requests_map_[job].erase(it);
    active_requests_.erase(request);
    // Even though we're invoking callbacks here, we don't need to worry
    // about |this| being deleted, because the factory is owned by the
    // profile which can not be deleted via callbacks.
    request->OnRequestComplete(rv);
  }
  active_jobs_.erase(job->host_port_proxy_pair());
  job_requests_map_.erase(job);
  delete job;
  return;
}

// Returns a newly created QuicHttpStream owned by the caller, if a
// matching session already exists.  Returns NULL otherwise.
scoped_ptr<QuicHttpStream> QuicStreamFactory::CreateIfSessionExists(
    const HostPortProxyPair& host_port_proxy_pair,
    const BoundNetLog& net_log) {
  if (!HasActiveSession(host_port_proxy_pair)) {
    return scoped_ptr<QuicHttpStream>(NULL);
  }

  QuicClientSession* session = active_sessions_[host_port_proxy_pair];
  DCHECK(session);
  return scoped_ptr<QuicHttpStream>(
      new QuicHttpStream(session->CreateOutgoingReliableStream()));
}

void QuicStreamFactory::OnIdleSession(QuicClientSession* session) {
}

void QuicStreamFactory::OnSessionClose(QuicClientSession* session) {
  DCHECK_EQ(0u, session->GetNumOpenStreams());
  const AliasSet& aliases = session_aliases_[session];
  for (AliasSet::const_iterator it = aliases.begin(); it != aliases.end();
       ++it) {
    DCHECK(active_sessions_.count(*it));
    DCHECK_EQ(session, active_sessions_[*it]);
    active_sessions_.erase(*it);
  }
  all_sessions_.erase(session);
  delete session;
}

void QuicStreamFactory::CancelRequest(QuicStreamRequest* request) {
  DCHECK(ContainsKey(active_requests_, request));
  Job* job = active_requests_[request];
  job_requests_map_[job].erase(request);
  active_requests_.erase(request);
}

bool QuicStreamFactory::HasActiveSession(
    const HostPortProxyPair& host_port_proxy_pair) {
  return ContainsKey(active_sessions_, host_port_proxy_pair);
}

QuicClientSession* QuicStreamFactory::CreateSession(
    const AddressList& address_list_,
    const BoundNetLog& net_log) {
  QuicGuid guid = random_uint64_callback_.Run();
  IPEndPoint addr = *address_list_.begin();
  DatagramClientSocket* socket =
      client_socket_factory_->CreateDatagramClientSocket(
          DatagramSocket::DEFAULT_BIND, base::Bind(&base::RandInt),
          net_log.net_log(), net_log.source());
  socket->Connect(addr);
  socket->GetLocalAddress(&addr);

  QuicConnectionHelper* helper = new QuicConnectionHelper(
      MessageLoop::current()->message_loop_proxy(),
      clock_.get(), socket);

  QuicConnection* connection = new QuicConnection(guid, addr, helper);
  QuicClientSession* session = new QuicClientSession(connection, helper, this);
  all_sessions_.insert(session);  // owning pointer
  return session;
}

bool QuicStreamFactory::HasActiveJob(
    const HostPortProxyPair& host_port_proxy_pair) {
  return ContainsKey(active_jobs_, host_port_proxy_pair);
}

void QuicStreamFactory::ActivateSession(
    const HostPortProxyPair& host_port_proxy_pair,
    QuicClientSession* session) {
  DCHECK(!HasActiveSession(host_port_proxy_pair));
  active_sessions_[host_port_proxy_pair] = session;
  session_aliases_[session].insert(host_port_proxy_pair);
}


}  // namespace net
