// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket_pool.h"

#include "net/base/net_errors.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"

namespace net {

SSLSocketParams::SSLSocketParams(
    const scoped_refptr<TCPSocketParams>& tcp_params,
    const scoped_refptr<HttpProxySocketParams>& http_proxy_params,
    const scoped_refptr<SOCKSSocketParams>& socks_params,
    ProxyServer::Scheme proxy,
    const std::string& hostname,
    const SSLConfig& ssl_config,
    int load_flags,
    bool want_spdy)
    : tcp_params_(tcp_params),
      http_proxy_params_(http_proxy_params),
      socks_params_(socks_params),
      proxy_(proxy),
      hostname_(hostname),
      ssl_config_(ssl_config),
      load_flags_(load_flags),
      want_spdy_(want_spdy) {
  switch (proxy_) {
    case ProxyServer::SCHEME_DIRECT:
      DCHECK(tcp_params_.get() != NULL);
      DCHECK(http_proxy_params_.get() == NULL);
      DCHECK(socks_params_.get() == NULL);
      break;
    case ProxyServer::SCHEME_HTTP:
      DCHECK(tcp_params_.get() == NULL);
      DCHECK(http_proxy_params_.get() != NULL);
      DCHECK(socks_params_.get() == NULL);
      break;
    case ProxyServer::SCHEME_SOCKS4:
    case ProxyServer::SCHEME_SOCKS5:
      DCHECK(tcp_params_.get() == NULL);
      DCHECK(http_proxy_params_.get() == NULL);
      DCHECK(socks_params_.get() != NULL);
      break;
    default:
      LOG(DFATAL) << "unknown proxy type";
      break;
  }
}

SSLSocketParams::~SSLSocketParams() {}

// Timeout for the SSL handshake portion of the connect.
static const int kSSLHandshakeTimeoutInSeconds = 30;

SSLConnectJob::SSLConnectJob(
    const std::string& group_name,
    const scoped_refptr<SSLSocketParams>& params,
    const base::TimeDelta& timeout_duration,
    const scoped_refptr<TCPClientSocketPool>& tcp_pool,
    const scoped_refptr<HttpProxyClientSocketPool>& http_proxy_pool,
    const scoped_refptr<SOCKSClientSocketPool>& socks_pool,
    ClientSocketFactory* client_socket_factory,
    const scoped_refptr<HostResolver>& host_resolver,
    Delegate* delegate,
    NetLog* net_log)
    : ConnectJob(group_name, timeout_duration, delegate,
                 BoundNetLog::Make(net_log, NetLog::SOURCE_CONNECT_JOB)),
      params_(params),
      tcp_pool_(tcp_pool),
      http_proxy_pool_(http_proxy_pool),
      socks_pool_(socks_pool),
      client_socket_factory_(client_socket_factory),
      resolver_(host_resolver),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &SSLConnectJob::OnIOComplete)) {}

SSLConnectJob::~SSLConnectJob() {}

LoadState SSLConnectJob::GetLoadState() const {
  switch (next_state_) {
    case STATE_TUNNEL_CONNECT_COMPLETE:
      if (transport_socket_handle_->socket())
        return LOAD_STATE_ESTABLISHING_PROXY_TUNNEL;
      // else, fall through.
    case STATE_TCP_CONNECT:
    case STATE_TCP_CONNECT_COMPLETE:
    case STATE_SOCKS_CONNECT:
    case STATE_SOCKS_CONNECT_COMPLETE:
    case STATE_TUNNEL_CONNECT:
      return transport_socket_handle_->GetLoadState();
    case STATE_SSL_CONNECT:
    case STATE_SSL_CONNECT_COMPLETE:
      return LOAD_STATE_SSL_HANDSHAKE;
    default:
      NOTREACHED();
      return LOAD_STATE_IDLE;
  }
}

int SSLConnectJob::ConnectInternal() {
  DetermineFirstState();
  return DoLoop(OK);
}

void SSLConnectJob::DetermineFirstState() {
  switch (params_->proxy()) {
    case ProxyServer::SCHEME_DIRECT:
      next_state_ = STATE_TCP_CONNECT;
      break;
    case ProxyServer::SCHEME_HTTP:
      next_state_ = STATE_TUNNEL_CONNECT;
      break;
    case ProxyServer::SCHEME_SOCKS4:
    case ProxyServer::SCHEME_SOCKS5:
      next_state_ = STATE_SOCKS_CONNECT;
      break;
    default:
      NOTREACHED() << "unknown proxy type";
      break;
  }
}

void SSLConnectJob::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    NotifyDelegateOfCompletion(rv);  // Deletes |this|.
}

int SSLConnectJob::DoLoop(int result) {
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
      case STATE_SOCKS_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoSOCKSConnect();
        break;
      case STATE_SOCKS_CONNECT_COMPLETE:
        rv = DoSOCKSConnectComplete(rv);
        break;
      case STATE_TUNNEL_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTunnelConnect();
        break;
      case STATE_TUNNEL_CONNECT_COMPLETE:
        rv = DoTunnelConnectComplete(rv);
        break;
      case STATE_SSL_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoSSLConnect();
        break;
      case STATE_SSL_CONNECT_COMPLETE:
        rv = DoSSLConnectComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int SSLConnectJob::DoTCPConnect() {
  DCHECK(tcp_pool_.get());
  next_state_ = STATE_TCP_CONNECT_COMPLETE;
  transport_socket_handle_.reset(new ClientSocketHandle());
  scoped_refptr<TCPSocketParams> tcp_params = params_->tcp_params();
  return transport_socket_handle_->Init(group_name(), tcp_params,
                                        tcp_params->destination().priority(),
                                        &callback_, tcp_pool_, net_log());
}

int SSLConnectJob::DoTCPConnectComplete(int result) {
  if (result == OK)
    next_state_ = STATE_SSL_CONNECT;

  return result;
}

int SSLConnectJob::DoSOCKSConnect() {
  DCHECK(socks_pool_.get());
  next_state_ = STATE_SOCKS_CONNECT_COMPLETE;
  transport_socket_handle_.reset(new ClientSocketHandle());
  scoped_refptr<SOCKSSocketParams> socks_params = params_->socks_params();
  return transport_socket_handle_->Init(group_name(), socks_params,
                                        socks_params->destination().priority(),
                                        &callback_, socks_pool_, net_log());
}

int SSLConnectJob::DoSOCKSConnectComplete(int result) {
  if (result == OK)
    next_state_ = STATE_SSL_CONNECT;

  return result;
}

int SSLConnectJob::DoTunnelConnect() {
  DCHECK(http_proxy_pool_.get());
  next_state_ = STATE_TUNNEL_CONNECT_COMPLETE;
  transport_socket_handle_.reset(new ClientSocketHandle());
  scoped_refptr<HttpProxySocketParams> http_proxy_params =
      params_->http_proxy_params();
  return transport_socket_handle_->Init(
      group_name(), http_proxy_params,
      http_proxy_params->tcp_params()->destination().priority(), &callback_,
      http_proxy_pool_, net_log());
}

int SSLConnectJob::DoTunnelConnectComplete(int result) {
  ClientSocket* socket = transport_socket_handle_->socket();
  HttpProxyClientSocket* tunnel_socket =
      static_cast<HttpProxyClientSocket*>(socket);

  if (result == ERR_RETRY_CONNECTION) {
    DetermineFirstState();
    transport_socket_handle_->socket()->Disconnect();
    return OK;
  }

  // Extract the information needed to prompt for the proxy authentication.
  // so that when ClientSocketPoolBaseHelper calls |GetAdditionalErrorState|,
  // we can easily set the state.
  if (result == ERR_PROXY_AUTH_REQUESTED)
    error_response_info_ = *tunnel_socket->GetResponseInfo();

  if (result < 0)
    return result;

  if (tunnel_socket->NeedsRestartWithAuth()) {
    // We must have gotten an 'idle' tunnel socket that is waiting for auth.
    // The HttpAuthController should have new credentials, we just need
    // to retry.
    next_state_ = STATE_TUNNEL_CONNECT_COMPLETE;
    return tunnel_socket->RestartWithAuth(&callback_);
  }

  next_state_ = STATE_SSL_CONNECT;
  return result;
}

void SSLConnectJob::GetAdditionalErrorState(ClientSocketHandle * handle) {
  handle->set_ssl_error_response_info(error_response_info_);
  if (!ssl_connect_start_time_.is_null())
    handle->set_is_ssl_error(true);
}

int SSLConnectJob::DoSSLConnect() {
  next_state_ = STATE_SSL_CONNECT_COMPLETE;
  // Reset the timeout to just the time allowed for the SSL handshake.
  ResetTimer(base::TimeDelta::FromSeconds(kSSLHandshakeTimeoutInSeconds));
  ssl_connect_start_time_ = base::TimeTicks::Now();

  ssl_socket_.reset(client_socket_factory_->CreateSSLClientSocket(
        transport_socket_handle_.release(), params_->hostname(),
        params_->ssl_config()));
  return ssl_socket_->Connect(&callback_);
}

int SSLConnectJob::DoSSLConnectComplete(int result) {
  SSLClientSocket::NextProtoStatus status =
      SSLClientSocket::kNextProtoUnsupported;
  std::string proto;
  // GetNextProto will fail and and trigger a NOTREACHED if we pass in a socket
  // that hasn't had SSL_ImportFD called on it. If we get a certificate error
  // here, then we know that we called SSL_ImportFD.
  if (result == OK || IsCertificateError(result))
    status = ssl_socket_->GetNextProto(&proto);

  bool using_spdy = false;
  if (status == SSLClientSocket::kNextProtoNegotiated) {
    ssl_socket_->setWasNpnNegotiated(true);
    if (SSLClientSocket::NextProtoFromString(proto) ==
        SSLClientSocket::kProtoSPDY1) {
          using_spdy = true;
    }
  }
  if (params_->want_spdy() && !using_spdy)
    return ERR_NPN_NEGOTIATION_FAILED;

  if (result == OK ||
      ssl_socket_->IgnoreCertError(result, params_->load_flags())) {
    DCHECK(ssl_connect_start_time_ != base::TimeTicks());
    base::TimeDelta connect_duration =
        base::TimeTicks::Now() - ssl_connect_start_time_;
    if (using_spdy)
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SpdyConnectionLatency",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
    else
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
  }

  if (result == OK || IsCertificateError(result)) {
    set_socket(ssl_socket_.release());
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    error_response_info_.cert_request_info = new SSLCertRequestInfo;
    ssl_socket_->GetSSLCertRequestInfo(error_response_info_.cert_request_info);
  }

  return result;
}

ConnectJob* SSLClientSocketPool::SSLConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return new SSLConnectJob(group_name, request.params(), ConnectionTimeout(),
                           tcp_pool_, http_proxy_pool_, socks_pool_,
                           client_socket_factory_, host_resolver_, delegate,
                           net_log_);
}

SSLClientSocketPool::SSLConnectJobFactory::SSLConnectJobFactory(
    const scoped_refptr<TCPClientSocketPool>& tcp_pool,
    const scoped_refptr<HttpProxyClientSocketPool>& http_proxy_pool,
    const scoped_refptr<SOCKSClientSocketPool>& socks_pool,
    ClientSocketFactory* client_socket_factory,
    HostResolver* host_resolver,
    NetLog* net_log)
    : tcp_pool_(tcp_pool),
      http_proxy_pool_(http_proxy_pool),
      socks_pool_(socks_pool),
      client_socket_factory_(client_socket_factory),
      host_resolver_(host_resolver),
      net_log_(net_log) {
  base::TimeDelta max_transport_timeout = base::TimeDelta();
  base::TimeDelta pool_timeout;
  if (tcp_pool_)
    max_transport_timeout = tcp_pool_->ConnectionTimeout();
  if (socks_pool_) {
    pool_timeout = socks_pool_->ConnectionTimeout();
    if (pool_timeout > max_transport_timeout)
      max_transport_timeout = pool_timeout;
  }
  if (http_proxy_pool_) {
    pool_timeout = http_proxy_pool_->ConnectionTimeout();
    if (pool_timeout > max_transport_timeout)
      max_transport_timeout = pool_timeout;
  }
  timeout_ = max_transport_timeout +
      base::TimeDelta::FromSeconds(kSSLHandshakeTimeoutInSeconds);
}

SSLClientSocketPool::SSLClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    const scoped_refptr<ClientSocketPoolHistograms>& histograms,
    const scoped_refptr<HostResolver>& host_resolver,
    ClientSocketFactory* client_socket_factory,
    const scoped_refptr<TCPClientSocketPool>& tcp_pool,
    const scoped_refptr<HttpProxyClientSocketPool>& http_proxy_pool,
    const scoped_refptr<SOCKSClientSocketPool>& socks_pool,
    NetLog* net_log)
    : base_(max_sockets, max_sockets_per_group, histograms,
            base::TimeDelta::FromSeconds(
                ClientSocketPool::unused_idle_socket_timeout()),
            base::TimeDelta::FromSeconds(kUsedIdleSocketTimeout),
            new SSLConnectJobFactory(tcp_pool, http_proxy_pool, socks_pool,
                                     client_socket_factory, host_resolver,
                                     net_log)) {}

SSLClientSocketPool::~SSLClientSocketPool() {}

int SSLClientSocketPool::RequestSocket(const std::string& group_name,
                                       const void* socket_params,
                                       RequestPriority priority,
                                       ClientSocketHandle* handle,
                                       CompletionCallback* callback,
                                       const BoundNetLog& net_log) {
  const scoped_refptr<SSLSocketParams>* casted_socket_params =
      static_cast<const scoped_refptr<SSLSocketParams>*>(socket_params);

  return base_.RequestSocket(group_name, *casted_socket_params, priority,
                             handle, callback, net_log);
}

void SSLClientSocketPool::CancelRequest(const std::string& group_name,
                                        ClientSocketHandle* handle) {
  base_.CancelRequest(group_name, handle);
}

void SSLClientSocketPool::ReleaseSocket(const std::string& group_name,
                                        ClientSocket* socket, int id) {
  base_.ReleaseSocket(group_name, socket, id);
}

void SSLClientSocketPool::Flush() {
  base_.Flush();
}

void SSLClientSocketPool::CloseIdleSockets() {
  base_.CloseIdleSockets();
}

int SSLClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_.IdleSocketCountInGroup(group_name);
}

LoadState SSLClientSocketPool::GetLoadState(
    const std::string& group_name, const ClientSocketHandle* handle) const {
  return base_.GetLoadState(group_name, handle);
}

}  // namespace net
