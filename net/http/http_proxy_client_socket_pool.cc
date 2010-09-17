// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include <algorithm>

#include "base/time.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

HttpProxySocketParams::HttpProxySocketParams(
    const scoped_refptr<TCPSocketParams>& tcp_params,
    const scoped_refptr<SSLSocketParams>& ssl_params,
    const GURL& request_url,
    const std::string& user_agent,
    HostPortPair endpoint,
    HttpAuthCache* http_auth_cache,
    HttpAuthHandlerFactory* http_auth_handler_factory,
    bool tunnel)
    : tcp_params_(tcp_params),
      ssl_params_(ssl_params),
      request_url_(request_url),
      user_agent_(user_agent),
      endpoint_(endpoint),
      http_auth_cache_(tunnel ? http_auth_cache : NULL),
      http_auth_handler_factory_(tunnel ? http_auth_handler_factory : NULL),
      tunnel_(tunnel) {
  DCHECK((tcp_params == NULL && ssl_params != NULL) ||
         (tcp_params != NULL && ssl_params == NULL));
}

const HostResolver::RequestInfo& HttpProxySocketParams::destination() const {
  if (tcp_params_ == NULL)
    return ssl_params_->tcp_params()->destination();
  else
    return tcp_params_->destination();
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
    const scoped_refptr<SSLClientSocketPool>& ssl_pool,
    const scoped_refptr<HostResolver>& host_resolver,
    Delegate* delegate,
    NetLog* net_log)
    : ConnectJob(group_name, timeout_duration, delegate,
                 BoundNetLog::Make(net_log, NetLog::SOURCE_CONNECT_JOB)),
      params_(params),
      tcp_pool_(tcp_pool),
      ssl_pool_(ssl_pool),
      resolver_(host_resolver),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &HttpProxyConnectJob::OnIOComplete)),
      using_spdy_(false) {
}

HttpProxyConnectJob::~HttpProxyConnectJob() {}

LoadState HttpProxyConnectJob::GetLoadState() const {
  switch (next_state_) {
    case kStateTCPConnect:
    case kStateTCPConnectComplete:
    case kStateSSLConnect:
    case kStateSSLConnectComplete:
      return transport_socket_handle_->GetLoadState();
    case kStateHttpProxyConnect:
    case kStateHttpProxyConnectComplete:
      return LOAD_STATE_ESTABLISHING_PROXY_TUNNEL;
    default:
      NOTREACHED();
      return LOAD_STATE_IDLE;
  }
}

int HttpProxyConnectJob::ConnectInternal() {
  if (params_->tcp_params())
    next_state_ = kStateTCPConnect;
  else
    next_state_ = kStateSSLConnect;
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
      case kStateSSLConnect:
        DCHECK_EQ(OK, rv);
        rv = DoSSLConnect();
        break;
      case kStateSSLConnectComplete:
        rv = DoSSLConnectComplete(rv);
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
  transport_socket_handle_.reset(new ClientSocketHandle());
  return transport_socket_handle_->Init(
      group_name(), params_->tcp_params(),
      params_->tcp_params()->destination().priority(), &callback_, tcp_pool_,
      net_log());
}

int HttpProxyConnectJob::DoTCPConnectComplete(int result) {
  if (result != OK)
    return ERR_PROXY_CONNECTION_FAILED;

  // Reset the timer to just the length of time allowed for HttpProxy handshake
  // so that a fast TCP connection plus a slow HttpProxy failure doesn't take
  // longer to timeout than it should.
  ResetTimer(base::TimeDelta::FromSeconds(
      kHttpProxyConnectJobTimeoutInSeconds));
  next_state_ = kStateHttpProxyConnect;
  return result;
}

int HttpProxyConnectJob::DoSSLConnect() {
  next_state_ = kStateSSLConnectComplete;
  transport_socket_handle_.reset(new ClientSocketHandle());
  return transport_socket_handle_->Init(
      group_name(), params_->ssl_params(),
      params_->ssl_params()->tcp_params()->destination().priority(),
      &callback_, ssl_pool_, net_log());
}

int HttpProxyConnectJob::DoSSLConnectComplete(int result) {
  if (IsCertificateError(result) &&
      params_->ssl_params()->load_flags() & LOAD_IGNORE_ALL_CERT_ERRORS)
    result = OK;
  if (result < 0) {
    // TODO(eroman): return ERR_PROXY_CONNECTION_FAILED if failed with the
    //               TCP connection.
    if (transport_socket_handle_->socket())
      transport_socket_handle_->socket()->Disconnect();
    return result;
  }

  SSLClientSocket* ssl =
      static_cast<SSLClientSocket*>(transport_socket_handle_->socket());
  using_spdy_ = ssl->was_spdy_negotiated();

  // Reset the timer to just the length of time allowed for HttpProxy handshake
  // so that a fast SSL connection plus a slow HttpProxy failure doesn't take
  // longer to timeout than it should.
  ResetTimer(base::TimeDelta::FromSeconds(
      kHttpProxyConnectJobTimeoutInSeconds));
  next_state_ = kStateHttpProxyConnect;
  return result;
}

int HttpProxyConnectJob::DoHttpProxyConnect() {
  next_state_ = kStateHttpProxyConnectComplete;
  const HostResolver::RequestInfo& tcp_destination = params_->destination();
  const HostPortPair& proxy_server = tcp_destination.host_port_pair();

  // Add a HttpProxy connection on top of the tcp socket.
  transport_socket_.reset(
      new HttpProxyClientSocket(transport_socket_handle_.release(),
                                params_->request_url(),
                                params_->user_agent(),
                                params_->endpoint(),
                                proxy_server,
                                params_->http_auth_cache(),
                                params_->http_auth_handler_factory(),
                                params_->tunnel(),
                                using_spdy_));
  int result = transport_socket_->Connect(&callback_);

  // Clear the circular reference to HttpNetworkSession (|params_| reference
  // HttpNetworkSession, which reference HttpProxyClientSocketPool, which
  // references |this|) here because it is safe to do so now but not at other
  // points.  This may cancel this ConnectJob.
  params_ = NULL;
  return result;
}

int HttpProxyConnectJob::DoHttpProxyConnectComplete(int result) {
  if (result == OK || result == ERR_PROXY_AUTH_REQUESTED)
      set_socket(transport_socket_.release());

  return result;
}

HttpProxyClientSocketPool::
HttpProxyConnectJobFactory::HttpProxyConnectJobFactory(
    const scoped_refptr<TCPClientSocketPool>& tcp_pool,
    const scoped_refptr<SSLClientSocketPool>& ssl_pool,
    HostResolver* host_resolver,
    NetLog* net_log)
    : tcp_pool_(tcp_pool),
      ssl_pool_(ssl_pool),
      host_resolver_(host_resolver),
      net_log_(net_log) {
  base::TimeDelta max_pool_timeout = base::TimeDelta();
  if (tcp_pool_)
    max_pool_timeout = tcp_pool_->ConnectionTimeout();
  if (ssl_pool_)
    max_pool_timeout = std::max(max_pool_timeout,
                                ssl_pool_->ConnectionTimeout());
  timeout_ = max_pool_timeout +
    base::TimeDelta::FromSeconds(kHttpProxyConnectJobTimeoutInSeconds);
}


ConnectJob*
HttpProxyClientSocketPool::HttpProxyConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return new HttpProxyConnectJob(group_name, request.params(),
                                 ConnectionTimeout(), tcp_pool_, ssl_pool_,
                                 host_resolver_, delegate, net_log_);
}

HttpProxyClientSocketPool::HttpProxyClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    const scoped_refptr<ClientSocketPoolHistograms>& histograms,
    const scoped_refptr<HostResolver>& host_resolver,
    const scoped_refptr<TCPClientSocketPool>& tcp_pool,
    const scoped_refptr<SSLClientSocketPool>& ssl_pool,
    NetLog* net_log)
    : tcp_pool_(tcp_pool),
      ssl_pool_(ssl_pool),
      base_(max_sockets, max_sockets_per_group, histograms,
            base::TimeDelta::FromSeconds(
                ClientSocketPool::unused_idle_socket_timeout()),
            base::TimeDelta::FromSeconds(kUsedIdleSocketTimeout),
            new HttpProxyConnectJobFactory(tcp_pool, ssl_pool, host_resolver,
                                           net_log)) {}

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
  if (ssl_pool_)
    ssl_pool_->Flush();
  if (tcp_pool_)
    tcp_pool_->Flush();
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

DictionaryValue* HttpProxyClientSocketPool::GetInfoAsValue(
    const std::string& name,
    const std::string& type,
    bool include_nested_pools) const {
  DictionaryValue* dict = base_.GetInfoAsValue(name, type);
  if (include_nested_pools) {
    ListValue* list = new ListValue();
    if (tcp_pool_.get()) {
      list->Append(tcp_pool_->GetInfoAsValue("tcp_socket_pool",
                                             "tcp_socket_pool",
                                             true));
    }
    if (ssl_pool_.get()) {
      list->Append(ssl_pool_->GetInfoAsValue("ssl_socket_pool",
                                             "ssl_socket_pool",
                                             true));
    }
    dict->Set("nested_pools", list);
  }
  return dict;
}

}  // namespace net
