// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket_pool.h"

#include "net/base/dnsrr_resolver.h"
#include "net/base/dns_util.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket_pool.h"

namespace net {

SSLSocketParams::SSLSocketParams(
    const scoped_refptr<TCPSocketParams>& tcp_params,
    const scoped_refptr<HttpProxySocketParams>& http_proxy_params,
    const scoped_refptr<SOCKSSocketParams>& socks_params,
    ProxyServer::Scheme proxy,
    const std::string& hostname,
    const SSLConfig& ssl_config,
    int load_flags,
    bool force_spdy_over_ssl,
    bool want_spdy_over_npn)
    : tcp_params_(tcp_params),
      http_proxy_params_(http_proxy_params),
      socks_params_(socks_params),
      proxy_(proxy),
      hostname_(hostname),
      ssl_config_(ssl_config),
      load_flags_(load_flags),
      force_spdy_over_ssl_(force_spdy_over_ssl),
      want_spdy_over_npn_(want_spdy_over_npn),
      dnssec_resolution_attempted_(false),
      dnssec_resolution_complete_(false),
      dnssec_resolution_callback_(NULL) {
  switch (proxy_) {
    case ProxyServer::SCHEME_DIRECT:
      DCHECK(tcp_params_.get() != NULL);
      DCHECK(http_proxy_params_.get() == NULL);
      DCHECK(socks_params_.get() == NULL);
      break;
    case ProxyServer::SCHEME_HTTP:
    case ProxyServer::SCHEME_HTTPS:
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

void SSLSocketParams::StartDNSSECResolution() {
  dnssec_response_.reset(new RRResponse);
  // We keep a reference to ourselves while the DNS resolution is underway.
  // When it completes (in DNSSECResolutionComplete), we balance it out.
  AddRef();

  dnssec_resolution_attempted_ = true;
  bool r = DnsRRResolver::Resolve(
      hostname(), kDNS_TXT, DnsRRResolver::FLAG_WANT_DNSSEC,
      NewCallback(this, &SSLSocketParams::DNSSECResolutionComplete),
      dnssec_response_.get());
  if (!r) {
    dnssec_response_.reset();
    dnssec_resolution_attempted_ = false;
  }
}

void SSLSocketParams::DNSSECResolutionComplete(int rv) {
  CompletionCallback* callback = NULL;
  {
    DCHECK(dnssec_resolution_attempted_);
    DCHECK(!dnssec_resolution_complete_);

    if (rv != OK)
      dnssec_response_.reset();

    dnssec_resolution_complete_ = true;
    if (dnssec_resolution_callback_)
      callback = dnssec_resolution_callback_;
  }

  if (callback)
    callback->Run(OK);

  Release();
}

int SSLSocketParams::GetDNSSECRecords(RRResponse** out,
                                      CompletionCallback* callback) {
  if (!dnssec_resolution_attempted_) {
    *out = NULL;
    return OK;
  }

  if (dnssec_resolution_complete_) {
    *out = dnssec_response_.get();
    return OK;
  }

  DCHECK(dnssec_resolution_callback_ == NULL);

  dnssec_resolution_callback_ = callback;
  return ERR_IO_PENDING;
}

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
  switch (params_->proxy()) {
    case ProxyServer::SCHEME_DIRECT:
      next_state_ = STATE_TCP_CONNECT;
      break;
    case ProxyServer::SCHEME_HTTP:
    case ProxyServer::SCHEME_HTTPS:
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
  return DoLoop(OK);
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

  if (SSLConfigService::dnssec_enabled())
    params_->StartDNSSECResolution();

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
      http_proxy_params->destination().priority(), &callback_,
      http_proxy_pool_, net_log());
}

int SSLConnectJob::DoTunnelConnectComplete(int result) {
  ClientSocket* socket = transport_socket_handle_->socket();
  HttpProxyClientSocket* tunnel_socket =
      static_cast<HttpProxyClientSocket*>(socket);

  // Extract the information needed to prompt for the proxy authentication.
  // so that when ClientSocketPoolBaseHelper calls |GetAdditionalErrorState|,
  // we can easily set the state.
  if (result == ERR_PROXY_AUTH_REQUESTED)
    error_response_info_ = *tunnel_socket->GetResponseInfo();

  if (result < 0)
    return result;

  next_state_ = STATE_SSL_CONNECT;
  return result;
}

void SSLConnectJob::GetAdditionalErrorState(ClientSocketHandle * handle) {
  // Headers in |error_response_info_| indicate a proxy tunnel setup
  // problem. See DoTunnelConnectComplete.
  if (error_response_info_.headers) {
    handle->set_pending_http_proxy_connection(
        transport_socket_handle_.release());
  }
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
  if (SSLConfigService::dnssec_enabled())
    ssl_socket_->UseDNSSEC(params_.get());
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

  // If we want spdy over npn, make sure it succeeded.
  if (status == SSLClientSocket::kNextProtoNegotiated) {
    ssl_socket_->set_was_npn_negotiated(true);
    SSLClientSocket::NextProto next_protocol =
        SSLClientSocket::NextProtoFromString(proto);
    // If we negotiated either version of SPDY, we must have
    // advertised it, so allow it.
    // TODO(mbelshe): verify it was a protocol we advertised?
    if (next_protocol == SSLClientSocket::kProtoSPDY1 ||
        next_protocol == SSLClientSocket::kProtoSPDY2) {
      ssl_socket_->set_was_spdy_negotiated(true);
    }
  }
  if (params_->want_spdy_over_npn() && !ssl_socket_->was_spdy_negotiated())
    return ERR_NPN_NEGOTIATION_FAILED;

  // Spdy might be turned on by default, or it might be over npn.
  bool using_spdy = params_->force_spdy_over_ssl() ||
      params_->want_spdy_over_npn();

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
    SSLConfigService* ssl_config_service,
    NetLog* net_log)
    : base_(max_sockets, max_sockets_per_group, histograms,
            base::TimeDelta::FromSeconds(
                ClientSocketPool::unused_idle_socket_timeout()),
            base::TimeDelta::FromSeconds(kUsedIdleSocketTimeout),
            new SSLConnectJobFactory(tcp_pool, http_proxy_pool, socks_pool,
                                     client_socket_factory, host_resolver,
                                     net_log)),
      ssl_config_service_(ssl_config_service) {
  if (ssl_config_service_)
    ssl_config_service_->AddObserver(this);
}

SSLClientSocketPool::~SSLClientSocketPool() {
  if (ssl_config_service_)
    ssl_config_service_->RemoveObserver(this);
}

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

void SSLClientSocketPool::OnSSLConfigChanged() {
  Flush();
}

}  // namespace net
