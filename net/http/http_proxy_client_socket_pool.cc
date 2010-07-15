// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_base.h"

namespace net {

HttpProxySocketParams::HttpProxySocketParams(
    const scoped_refptr<TCPSocketParams>& proxy_server,
    const GURL& request_url,
    HostPortPair endpoint,
    scoped_refptr<HttpAuthController> auth_controller,
    bool tunnel)
    : tcp_params_(proxy_server),
      request_url_(request_url),
      endpoint_(endpoint),
      auth_controller_(auth_controller),
      tunnel_(tunnel) {
}

HttpProxySocketParams::~HttpProxySocketParams() {}

// HttpProxyConnectJobs will time out after this many seconds.  Note this is on
// top of the timeout for the transport socket.
static const int kHttpProxyConnectJobTimeoutInSeconds = 30;

HttpProxyConnectJob::HttpProxyConnectJob(
    const std::string& group_name,
    const scoped_refptr<HttpProxySocketParams>& params,
    const base::TimeDelta& timeout_duration,
    const scoped_refptr<TCPClientSocketPool>& tcp_pool,
    const scoped_refptr<HostResolver>& host_resolver,
    Delegate* delegate,
    NetLog* net_log)
    : ConnectJob(group_name, timeout_duration, delegate,
                 BoundNetLog::Make(net_log, NetLog::SOURCE_CONNECT_JOB)),
      params_(params),
      tcp_pool_(tcp_pool),
      resolver_(host_resolver),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &HttpProxyConnectJob::OnIOComplete)) {
}

HttpProxyConnectJob::~HttpProxyConnectJob() {}

LoadState HttpProxyConnectJob::GetLoadState() const {
  switch (next_state_) {
    case kStateTCPConnect:
    case kStateTCPConnectComplete:
      return tcp_socket_handle_->GetLoadState();
    case kStateHttpProxyConnect:
    case kStateHttpProxyConnectComplete:
      return LOAD_STATE_ESTABLISHING_PROXY_TUNNEL;
    default:
      NOTREACHED();
      return LOAD_STATE_IDLE;
  }
}

int HttpProxyConnectJob::ConnectInternal() {
  next_state_ = kStateTCPConnect;
  return DoLoop(OK);
}

void HttpProxyConnectJob::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    NotifyDelegateOfCompletion(rv);  // Deletes |this|
}

int HttpProxyConnectJob::DoLoop(int result) {
  DCHECK_NE(next_state_, kStateNone);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = kStateNone;
    switch (state) {
      case kStateTCPConnect:
        DCHECK_EQ(OK, rv);
        rv = DoTCPConnect();
        break;
      case kStateTCPConnectComplete:
        rv = DoTCPConnectComplete(rv);
        break;
      case kStateHttpProxyConnect:
        DCHECK_EQ(OK, rv);
        rv = DoHttpProxyConnect();
        break;
      case kStateHttpProxyConnectComplete:
        rv = DoHttpProxyConnectComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != kStateNone);

  return rv;
}

int HttpProxyConnectJob::DoTCPConnect() {
  next_state_ = kStateTCPConnectComplete;
  tcp_socket_handle_.reset(new ClientSocketHandle());
  return tcp_socket_handle_->Init(
      group_name(), params_->tcp_params(),
      params_->tcp_params()->destination().priority(), &callback_, tcp_pool_,
      net_log());
}

int HttpProxyConnectJob::DoTCPConnectComplete(int result) {
  if (result != OK)
    return result;

  // Reset the timer to just the length of time allowed for HttpProxy handshake
  // so that a fast TCP connection plus a slow HttpProxy failure doesn't take
  // longer to timeout than it should.
  ResetTimer(base::TimeDelta::FromSeconds(
      kHttpProxyConnectJobTimeoutInSeconds));
  next_state_ = kStateHttpProxyConnect;
  return result;
}

int HttpProxyConnectJob::DoHttpProxyConnect() {
  next_state_ = kStateHttpProxyConnectComplete;

  // Add a HttpProxy connection on top of the tcp socket.
  socket_.reset(new HttpProxyClientSocket(tcp_socket_handle_.release(),
                                          params_->request_url(),
                                          params_->endpoint(),
                                          params_->auth_controller(),
                                          params_->tunnel()));
  return socket_->Connect(&callback_);
}

int HttpProxyConnectJob::DoHttpProxyConnectComplete(int result) {
  DCHECK_NE(result, ERR_RETRY_CONNECTION);

  if (result == OK || result == ERR_PROXY_AUTH_REQUESTED)
      set_socket(socket_.release());

  return result;
}

ConnectJob*
HttpProxyClientSocketPool::HttpProxyConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return new HttpProxyConnectJob(group_name, request.params(),
                                 ConnectionTimeout(), tcp_pool_, host_resolver_,
                                 delegate, net_log_);
}

base::TimeDelta
HttpProxyClientSocketPool::HttpProxyConnectJobFactory::ConnectionTimeout()
const {
  return tcp_pool_->ConnectionTimeout() +
      base::TimeDelta::FromSeconds(kHttpProxyConnectJobTimeoutInSeconds);
}

HttpProxyClientSocketPool::HttpProxyClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    const scoped_refptr<ClientSocketPoolHistograms>& histograms,
    const scoped_refptr<HostResolver>& host_resolver,
    const scoped_refptr<TCPClientSocketPool>& tcp_pool,
    NetLog* net_log)
    : base_(max_sockets, max_sockets_per_group, histograms,
            base::TimeDelta::FromSeconds(
                ClientSocketPool::unused_idle_socket_timeout()),
            base::TimeDelta::FromSeconds(kUsedIdleSocketTimeout),
            new HttpProxyConnectJobFactory(tcp_pool, host_resolver, net_log)) {}

HttpProxyClientSocketPool::~HttpProxyClientSocketPool() {}

int HttpProxyClientSocketPool::RequestSocket(const std::string& group_name,
                                             const void* socket_params,
                                             RequestPriority priority,
                                             ClientSocketHandle* handle,
                                             CompletionCallback* callback,
                                             const BoundNetLog& net_log) {
  const scoped_refptr<HttpProxySocketParams>* casted_socket_params =
      static_cast<const scoped_refptr<HttpProxySocketParams>*>(socket_params);

  return base_.RequestSocket(group_name, *casted_socket_params, priority,
                             handle, callback, net_log);
}

void HttpProxyClientSocketPool::CancelRequest(
    const std::string& group_name,
    ClientSocketHandle* handle) {
  base_.CancelRequest(group_name, handle);
}

void HttpProxyClientSocketPool::ReleaseSocket(const std::string& group_name,
                                              ClientSocket* socket, int id) {
  base_.ReleaseSocket(group_name, socket, id);
}

void HttpProxyClientSocketPool::Flush() {
  base_.Flush();
}

void HttpProxyClientSocketPool::CloseIdleSockets() {
  base_.CloseIdleSockets();
}

int HttpProxyClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_.IdleSocketCountInGroup(group_name);
}

LoadState HttpProxyClientSocketPool::GetLoadState(
    const std::string& group_name, const ClientSocketHandle* handle) const {
  return base_.GetLoadState(group_name, handle);
}

}  // namespace net
