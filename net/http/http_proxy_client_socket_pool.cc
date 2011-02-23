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
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/spdy/spdy_proxy_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_settings_storage.h"
#include "net/spdy/spdy_stream.h"

namespace net {

HttpProxySocketParams::HttpProxySocketParams(
    const scoped_refptr<TCPSocketParams>& tcp_params,
    const scoped_refptr<SSLSocketParams>& ssl_params,
    const GURL& request_url,
    const std::string& user_agent,
    HostPortPair endpoint,
    HttpAuthCache* http_auth_cache,
    HttpAuthHandlerFactory* http_auth_handler_factory,
    SpdySessionPool* spdy_session_pool,
    SpdySettingsStorage* spdy_settings,
    bool tunnel)
    : tcp_params_(tcp_params),
      ssl_params_(ssl_params),
      spdy_session_pool_(spdy_session_pool),
      spdy_settings_(spdy_settings),
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
    TCPClientSocketPool* tcp_pool,
    SSLClientSocketPool* ssl_pool,
    HostResolver* host_resolver,
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
    case STATE_TCP_CONNECT:
    case STATE_TCP_CONNECT_COMPLETE:
    case STATE_SSL_CONNECT:
    case STATE_SSL_CONNECT_COMPLETE:
      return transport_socket_handle_->GetLoadState();
    case STATE_HTTP_PROXY_CONNECT:
    case STATE_HTTP_PROXY_CONNECT_COMPLETE:
    case STATE_SPDY_PROXY_CREATE_STREAM:
    case STATE_SPDY_PROXY_CREATE_STREAM_COMPLETE:
      return LOAD_STATE_ESTABLISHING_PROXY_TUNNEL;
    default:
      NOTREACHED();
      return LOAD_STATE_IDLE;
  }
}

void HttpProxyConnectJob::GetAdditionalErrorState(ClientSocketHandle * handle) {
  if (error_response_info_.cert_request_info) {
    handle->set_ssl_error_response_info(error_response_info_);
    handle->set_is_ssl_error(true);
  }
}

void HttpProxyConnectJob::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    NotifyDelegateOfCompletion(rv);  // Deletes |this|
}

int HttpProxyConnectJob::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_TCP_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTCPConnect();
        break;
      case STATE_TCP_CONNECT_COMPLETE:
        rv = DoTCPConnectComplete(rv);
        break;
      case STATE_SSL_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoSSLConnect();
        break;
      case STATE_SSL_CONNECT_COMPLETE:
        rv = DoSSLConnectComplete(rv);
        break;
      case STATE_HTTP_PROXY_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoHttpProxyConnect();
        break;
      case STATE_HTTP_PROXY_CONNECT_COMPLETE:
        rv = DoHttpProxyConnectComplete(rv);
        break;
      case STATE_SPDY_PROXY_CREATE_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoSpdyProxyCreateStream();
        break;
      case STATE_SPDY_PROXY_CREATE_STREAM_COMPLETE:
        rv = DoSpdyProxyCreateStreamComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int HttpProxyConnectJob::DoTCPConnect() {
  next_state_ = STATE_TCP_CONNECT_COMPLETE;
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

  next_state_ = STATE_HTTP_PROXY_CONNECT;
  return result;
}

int HttpProxyConnectJob::DoSSLConnect() {
  if (params_->tunnel()) {
    HostPortProxyPair pair(params_->destination().host_port_pair(),
                           ProxyServer::Direct());
    if (params_->spdy_session_pool()->HasSession(pair)) {
      using_spdy_ = true;
      next_state_ = STATE_SPDY_PROXY_CREATE_STREAM;
      return OK;
    }
  }
  next_state_ = STATE_SSL_CONNECT_COMPLETE;
  transport_socket_handle_.reset(new ClientSocketHandle());
  return transport_socket_handle_->Init(
      group_name(), params_->ssl_params(),
      params_->ssl_params()->tcp_params()->destination().priority(),
      &callback_, ssl_pool_, net_log());
}

int HttpProxyConnectJob::DoSSLConnectComplete(int result) {
  if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    error_response_info_ = transport_socket_handle_->ssl_error_response_info();
    DCHECK(error_response_info_.cert_request_info.get());
    return result;
  }
  if (IsCertificateError(result)) {
    if (params_->ssl_params()->load_flags() & LOAD_IGNORE_ALL_CERT_ERRORS)
      result = OK;
    else
      // TODO(rch): allow the user to deal with proxy cert errors in the
      // same way as server cert errors.
      return ERR_PROXY_CERTIFICATE_INVALID;
  }
  if (result < 0) {
    if (transport_socket_handle_->socket())
      transport_socket_handle_->socket()->Disconnect();
    return ERR_PROXY_CONNECTION_FAILED;
  }

  SSLClientSocket* ssl =
      static_cast<SSLClientSocket*>(transport_socket_handle_->socket());
  using_spdy_ = ssl->was_spdy_negotiated();

  // Reset the timer to just the length of time allowed for HttpProxy handshake
  // so that a fast SSL connection plus a slow HttpProxy failure doesn't take
  // longer to timeout than it should.
  ResetTimer(base::TimeDelta::FromSeconds(
      kHttpProxyConnectJobTimeoutInSeconds));
  // TODO(rch): If we ever decide to implement a "trusted" SPDY proxy
  // (one that we speak SPDY over SSL to, but to which we send HTTPS
  // request directly instead of through CONNECT tunnels, then we
  // need to add a predicate to this if statement so we fall through
  // to the else case. (HttpProxyClientSocket currently acts as
  // a "trusted" SPDY proxy).
  if (using_spdy_ && params_->tunnel())
    next_state_ = STATE_SPDY_PROXY_CREATE_STREAM;
  else
    next_state_ = STATE_HTTP_PROXY_CONNECT;
  return result;
}

int HttpProxyConnectJob::DoHttpProxyConnect() {
  next_state_ = STATE_HTTP_PROXY_CONNECT_COMPLETE;
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
                                using_spdy_,
                                params_->ssl_params() != NULL));
  return transport_socket_->Connect(&callback_);
}

int HttpProxyConnectJob::DoHttpProxyConnectComplete(int result) {
  if (result == OK || result == ERR_PROXY_AUTH_REQUESTED ||
      result == ERR_HTTPS_PROXY_TUNNEL_RESPONSE) {
      set_socket(transport_socket_.release());
  }

  return result;
}

int HttpProxyConnectJob::DoSpdyProxyCreateStream() {
  DCHECK(using_spdy_);
  DCHECK(params_->tunnel());

  HostPortProxyPair pair(params_->destination().host_port_pair(),
                         ProxyServer::Direct());
  SpdySessionPool* spdy_pool = params_->spdy_session_pool();
  scoped_refptr<SpdySession> spdy_session;
  // It's possible that a session to the proxy has recently been created
  if (spdy_pool->HasSession(pair)) {
    if (transport_socket_handle_.get()) {
      if (transport_socket_handle_->socket())
        transport_socket_handle_->socket()->Disconnect();
      transport_socket_handle_->Reset();
    }
    spdy_session = spdy_pool->Get(pair, params_->spdy_settings(), net_log());
  } else {
    // Create a session direct to the proxy itself
    int rv = spdy_pool->GetSpdySessionFromSocket(
        pair, params_->spdy_settings(), transport_socket_handle_.release(),
        net_log(), OK, &spdy_session, /*using_ssl_*/ true);
    if (rv < 0)
      return rv;
  }

  next_state_ = STATE_SPDY_PROXY_CREATE_STREAM_COMPLETE;
  return spdy_session->CreateStream(params_->request_url(),
                                    params_->destination().priority(),
                                    &spdy_stream_, spdy_session->net_log(),
                                    &callback_);
}

int HttpProxyConnectJob::DoSpdyProxyCreateStreamComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_HTTP_PROXY_CONNECT_COMPLETE;
  transport_socket_.reset(
      new SpdyProxyClientSocket(spdy_stream_,
                                params_->user_agent(),
                                params_->endpoint(),
                                params_->request_url(),
                                params_->destination().host_port_pair(),
                                params_->http_auth_cache(),
                                params_->http_auth_handler_factory()));
  return transport_socket_->Connect(&callback_);
}

int HttpProxyConnectJob::ConnectInternal() {
  if (params_->tcp_params())
    next_state_ = STATE_TCP_CONNECT;
  else
    next_state_ = STATE_SSL_CONNECT;
  return DoLoop(OK);
}

HttpProxyClientSocketPool::
HttpProxyConnectJobFactory::HttpProxyConnectJobFactory(
    TCPClientSocketPool* tcp_pool,
    SSLClientSocketPool* ssl_pool,
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
    ClientSocketPoolHistograms* histograms,
    HostResolver* host_resolver,
    TCPClientSocketPool* tcp_pool,
    SSLClientSocketPool* ssl_pool,
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

void HttpProxyClientSocketPool::RequestSockets(
    const std::string& group_name,
    const void* params,
    int num_sockets,
    const BoundNetLog& net_log) {
  const scoped_refptr<HttpProxySocketParams>* casted_params =
      static_cast<const scoped_refptr<HttpProxySocketParams>*>(params);

  base_.RequestSockets(group_name, *casted_params, num_sockets, net_log);
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

int HttpProxyClientSocketPool::IdleSocketCount() const {
  return base_.idle_socket_count();
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
    if (tcp_pool_) {
      list->Append(tcp_pool_->GetInfoAsValue("tcp_socket_pool",
                                             "tcp_socket_pool",
                                             true));
    }
    if (ssl_pool_) {
      list->Append(ssl_pool_->GetInfoAsValue("ssl_socket_pool",
                                             "ssl_socket_pool",
                                             true));
    }
    dict->Set("nested_pools", list);
  }
  return dict;
}

base::TimeDelta HttpProxyClientSocketPool::ConnectionTimeout() const {
  return base_.ConnectionTimeout();
}

ClientSocketPoolHistograms* HttpProxyClientSocketPool::histograms() const {
  return base_.histograms();
}

}  // namespace net
