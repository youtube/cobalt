// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl_job.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "nb/memory_scope.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_pipelined_connection.h"
#include "net/http/http_pipelined_host.h"
#include "net/http/http_pipelined_host_pool.h"
#include "net/http/http_pipelined_stream.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_factory_impl_request.h"
#include "net/quic/quic_http_stream.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

// Returns parameters associated with the start of a HTTP stream job.
Value* NetLogHttpStreamJobCallback(const GURL* original_url,
                                   const GURL* url,
                                   NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("original_url", original_url->GetOrigin().spec());
  dict->SetString("url", url->GetOrigin().spec());
  return dict;
}

// Returns parameters associated with the Proto (with NPN negotiation) of a HTTP
// stream.
Value* NetLogHttpStreamProtoCallback(
    const SSLClientSocket::NextProtoStatus status,
    const std::string* proto,
    const std::string* server_protos,
    NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();

  dict->SetString("next_proto_status",
                  SSLClientSocket::NextProtoStatusToString(status));
  dict->SetString("proto", *proto);
  dict->SetString("server_protos",
                  SSLClientSocket::ServerProtosToString(*server_protos));
  return dict;
}

HttpStreamFactoryImpl::Job::Job(HttpStreamFactoryImpl* stream_factory,
                                HttpNetworkSession* session,
                                const HttpRequestInfo& request_info,
                                const SSLConfig& server_ssl_config,
                                const SSLConfig& proxy_ssl_config,
                                NetLog* net_log)
    : request_(NULL),
      request_info_(request_info),
      server_ssl_config_(server_ssl_config),
      proxy_ssl_config_(proxy_ssl_config),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_HTTP_STREAM_JOB)),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_callback_(
          base::Bind(&Job::OnIOComplete, base::Unretained(this)))),
      connection_(new ClientSocketHandle),
      session_(session),
      stream_factory_(stream_factory),
      next_state_(STATE_NONE),
      pac_request_(NULL),
      blocking_job_(NULL),
      waiting_job_(NULL),
      using_ssl_(false),
      using_spdy_(false),
      using_quic_(false),
      quic_request_(session_->quic_stream_factory()),
      force_spdy_always_(HttpStreamFactory::force_spdy_always()),
      force_spdy_over_ssl_(HttpStreamFactory::force_spdy_over_ssl()),
      spdy_certificate_error_(OK),
      establishing_tunnel_(false),
      was_npn_negotiated_(false),
      protocol_negotiated_(kProtoUnknown),
      num_streams_(0),
      spdy_session_direct_(false),
      existing_available_pipeline_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(ptr_factory_(this)) {
  DCHECK(stream_factory);
  DCHECK(session);
}

HttpStreamFactoryImpl::Job::~Job() {
  net_log_.EndEvent(NetLog::TYPE_HTTP_STREAM_JOB);

  // When we're in a partially constructed state, waiting for the user to
  // provide certificate handling information or authentication, we can't reuse
  // this stream at all.
  if (next_state_ == STATE_WAITING_USER_ACTION) {
    connection_->socket()->Disconnect();
    connection_.reset();
  }

  if (pac_request_)
    session_->proxy_service()->CancelPacRequest(pac_request_);

  // The stream could be in a partial state.  It is not reusable.
  if (stream_.get() && next_state_ != STATE_DONE)
    stream_->Close(true /* not reusable */);
}

void HttpStreamFactoryImpl::Job::Start(Request* request) {
  DCHECK(request);
  request_ = request;
  StartInternal();
}

int HttpStreamFactoryImpl::Job::Preconnect(int num_streams) {
  DCHECK_GT(num_streams, 0);
  HostPortPair origin_server =
      HostPortPair(request_info_.url.HostNoBrackets(),
                   request_info_.url.EffectiveIntPort());
  HttpServerProperties* http_server_properties =
      session_->http_server_properties();
  if (http_server_properties &&
      http_server_properties->SupportsSpdy(origin_server)) {
    num_streams_ = 1;
  } else {
    num_streams_ = num_streams;
  }
  return StartInternal();
}

int HttpStreamFactoryImpl::Job::RestartTunnelWithProxyAuth(
    const AuthCredentials& credentials) {
  DCHECK(establishing_tunnel_);
  next_state_ = STATE_RESTART_TUNNEL_AUTH;
  stream_.reset();
  return RunLoop(OK);
}

LoadState HttpStreamFactoryImpl::Job::GetLoadState() const {
  switch (next_state_) {
    case STATE_RESOLVE_PROXY_COMPLETE:
      return session_->proxy_service()->GetLoadState(pac_request_);
    case STATE_CREATE_STREAM_COMPLETE:
      return using_quic_ ? LOAD_STATE_CONNECTING : connection_->GetLoadState();
    case STATE_INIT_CONNECTION_COMPLETE:
      return LOAD_STATE_SENDING_REQUEST;
    default:
      return LOAD_STATE_IDLE;
  }
}

void HttpStreamFactoryImpl::Job::MarkAsAlternate(const GURL& original_url) {
  DCHECK(!original_url_.get());
  original_url_.reset(new GURL(original_url));
}

void HttpStreamFactoryImpl::Job::WaitFor(Job* job) {
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK_EQ(STATE_NONE, job->next_state_);
  DCHECK(!blocking_job_);
  DCHECK(!job->waiting_job_);
  blocking_job_ = job;
  job->waiting_job_ = this;
}

void HttpStreamFactoryImpl::Job::Resume(Job* job) {
  DCHECK_EQ(blocking_job_, job);
  blocking_job_ = NULL;

  // We know we're blocked if the next_state_ is STATE_WAIT_FOR_JOB_COMPLETE.
  // Unblock |this|.
  if (next_state_ == STATE_WAIT_FOR_JOB_COMPLETE) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&HttpStreamFactoryImpl::Job::OnIOComplete,
                   ptr_factory_.GetWeakPtr(), OK));
  }
}

void HttpStreamFactoryImpl::Job::Orphan(const Request* request) {
  DCHECK_EQ(request_, request);
  request_ = NULL;
  // We've been orphaned, but there's a job we're blocked on. Don't bother
  // racing, just cancel ourself.
  if (blocking_job_) {
    DCHECK(blocking_job_->waiting_job_);
    blocking_job_->waiting_job_ = NULL;
    blocking_job_ = NULL;
    stream_factory_->OnOrphanedJobComplete(this);
  }
}

bool HttpStreamFactoryImpl::Job::was_npn_negotiated() const {
  return was_npn_negotiated_;
}

NextProto HttpStreamFactoryImpl::Job::protocol_negotiated()
    const {
  return protocol_negotiated_;
}

bool HttpStreamFactoryImpl::Job::using_spdy() const {
  return using_spdy_;
}

const SSLConfig& HttpStreamFactoryImpl::Job::server_ssl_config() const {
  return server_ssl_config_;
}

const SSLConfig& HttpStreamFactoryImpl::Job::proxy_ssl_config() const {
  return proxy_ssl_config_;
}

const ProxyInfo& HttpStreamFactoryImpl::Job::proxy_info() const {
  return proxy_info_;
}

void HttpStreamFactoryImpl::Job::GetSSLInfo() {
  DCHECK(using_ssl_);
  DCHECK(!establishing_tunnel_);
  DCHECK(connection_.get() && connection_->socket());
  SSLClientSocket* ssl_socket =
      static_cast<SSLClientSocket*>(connection_->socket());
  ssl_socket->GetSSLInfo(&ssl_info_);
}

HostPortProxyPair HttpStreamFactoryImpl::Job::GetSpdySessionKey() const {
  // In the case that we're using an HTTPS proxy for an HTTP url,
  // we look for a SPDY session *to* the proxy, instead of to the
  // origin server.
  if (IsHttpsProxyAndHttpUrl()) {
    return HostPortProxyPair(proxy_info_.proxy_server().host_port_pair(),
                             ProxyServer::Direct());
  } else {
    return HostPortProxyPair(origin_, proxy_info_.proxy_server());
  }
}

bool HttpStreamFactoryImpl::Job::CanUseExistingSpdySession() const {
  // We need to make sure that if a spdy session was created for
  // https://somehost/ that we don't use that session for http://somehost:443/.
  // The only time we can use an existing session is if the request URL is
  // https (the normal case) or if we're connection to a SPDY proxy, or
  // if we're running with force_spdy_always_.  crbug.com/133176
  return request_info_.url.SchemeIs("https") ||
         proxy_info_.proxy_server().is_https() ||
         force_spdy_always_;
}

void HttpStreamFactoryImpl::Job::OnStreamReadyCallback() {
  TRACK_MEMORY_SCOPE("Network");
  DCHECK(stream_.get());
  DCHECK(!IsPreconnecting());
  if (IsOrphaned()) {
    stream_factory_->OnOrphanedJobComplete(this);
  } else {
    request_->Complete(was_npn_negotiated(),
                       protocol_negotiated(),
                       using_spdy(),
                       net_log_);
    request_->OnStreamReady(this, server_ssl_config_, proxy_info_,
                            stream_.release());
  }
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnSpdySessionReadyCallback() {
  DCHECK(!stream_.get());
  DCHECK(!IsPreconnecting());
  DCHECK(using_spdy());
  DCHECK(new_spdy_session_);
  scoped_refptr<SpdySession> spdy_session = new_spdy_session_;
  new_spdy_session_ = NULL;
  if (IsOrphaned()) {
    stream_factory_->OnSpdySessionReady(
        spdy_session, spdy_session_direct_, server_ssl_config_, proxy_info_,
        was_npn_negotiated(), protocol_negotiated(), using_spdy(), net_log_);
    stream_factory_->OnOrphanedJobComplete(this);
  } else {
    request_->OnSpdySessionReady(this, spdy_session, spdy_session_direct_);
  }
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnStreamFailedCallback(int result) {
  DCHECK(!IsPreconnecting());
  if (IsOrphaned())
    stream_factory_->OnOrphanedJobComplete(this);
  else
    request_->OnStreamFailed(this, result, server_ssl_config_);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnCertificateErrorCallback(
    int result, const SSLInfo& ssl_info) {
  DCHECK(!IsPreconnecting());
  if (IsOrphaned())
    stream_factory_->OnOrphanedJobComplete(this);
  else
    request_->OnCertificateError(this, result, server_ssl_config_, ssl_info);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnNeedsProxyAuthCallback(
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller) {
  DCHECK(!IsPreconnecting());
  if (IsOrphaned())
    stream_factory_->OnOrphanedJobComplete(this);
  else
    request_->OnNeedsProxyAuth(
        this, response, server_ssl_config_, proxy_info_, auth_controller);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnNeedsClientAuthCallback(
    SSLCertRequestInfo* cert_info) {
  DCHECK(!IsPreconnecting());
  if (IsOrphaned())
    stream_factory_->OnOrphanedJobComplete(this);
  else
    request_->OnNeedsClientAuth(this, server_ssl_config_, cert_info);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnHttpsProxyTunnelResponseCallback(
    const HttpResponseInfo& response_info,
    HttpStream* stream) {
  DCHECK(!IsPreconnecting());
  if (IsOrphaned())
    stream_factory_->OnOrphanedJobComplete(this);
  else
    request_->OnHttpsProxyTunnelResponse(
        this, response_info, server_ssl_config_, proxy_info_, stream);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnPreconnectsComplete() {
  DCHECK(!request_);
  if (new_spdy_session_) {
    stream_factory_->OnSpdySessionReady(
        new_spdy_session_, spdy_session_direct_, server_ssl_config_,
        proxy_info_, was_npn_negotiated(), protocol_negotiated(), using_spdy(),
        net_log_);
  }
  stream_factory_->OnPreconnectsComplete(this);
  // |this| may be deleted after this call.
}

// static
int HttpStreamFactoryImpl::Job::OnHostResolution(
    SpdySessionPool* spdy_session_pool,
    const HostPortProxyPair& spdy_session_key,
    const AddressList& addresses,
    const BoundNetLog& net_log) {
#if defined(COBALT_DISABLE_SPDY)
  return OK;
#else
  // It is OK to dereference spdy_session_pool, because the
  // ClientSocketPoolManager will be destroyed in the same callback that
  // destroys the SpdySessionPool.
  bool has_session =
      spdy_session_pool->GetIfExists(spdy_session_key, net_log) != NULL;
  return has_session ? ERR_SPDY_SESSION_ALREADY_EXISTS  : OK;
#endif  // defined(COBALT_DISABLE_SPDY)
}

void HttpStreamFactoryImpl::Job::OnIOComplete(int result) {
  RunLoop(result);
}

int HttpStreamFactoryImpl::Job::RunLoop(int result) {
  result = DoLoop(result);

  if (result == ERR_IO_PENDING)
    return result;

  // If there was an error, we should have already resumed the |waiting_job_|,
  // if there was one.
  DCHECK(result == OK || waiting_job_ == NULL);

  if (IsPreconnecting()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &HttpStreamFactoryImpl::Job::OnPreconnectsComplete,
            ptr_factory_.GetWeakPtr()));
    return ERR_IO_PENDING;
  }

  if (IsCertificateError(result)) {
    // Retrieve SSL information from the socket.
    GetSSLInfo();

    next_state_ = STATE_WAITING_USER_ACTION;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &HttpStreamFactoryImpl::Job::OnCertificateErrorCallback,
            ptr_factory_.GetWeakPtr(),
            result, ssl_info_));
    return ERR_IO_PENDING;
  }

  switch (result) {
    case ERR_PROXY_AUTH_REQUESTED:
      {
        DCHECK(connection_.get());
        DCHECK(connection_->socket());
        DCHECK(establishing_tunnel_);

        ProxyClientSocket* proxy_socket =
            static_cast<ProxyClientSocket*>(connection_->socket());
        const HttpResponseInfo* tunnel_auth_response =
            proxy_socket->GetConnectResponseInfo();

        next_state_ = STATE_WAITING_USER_ACTION;
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(
                &HttpStreamFactoryImpl::Job::OnNeedsProxyAuthCallback,
                ptr_factory_.GetWeakPtr(),
                *tunnel_auth_response,
                proxy_socket->GetAuthController()));
      }
      return ERR_IO_PENDING;

    case ERR_SSL_CLIENT_AUTH_CERT_NEEDED:
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(
              &HttpStreamFactoryImpl::Job::OnNeedsClientAuthCallback,
              ptr_factory_.GetWeakPtr(),
              connection_->ssl_error_response_info().cert_request_info));
      return ERR_IO_PENDING;

    case ERR_HTTPS_PROXY_TUNNEL_RESPONSE:
      {
        DCHECK(connection_.get());
        DCHECK(connection_->socket());
        DCHECK(establishing_tunnel_);

        ProxyClientSocket* proxy_socket =
            static_cast<ProxyClientSocket*>(connection_->socket());
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(
                &HttpStreamFactoryImpl::Job::OnHttpsProxyTunnelResponseCallback,
                ptr_factory_.GetWeakPtr(),
                *proxy_socket->GetConnectResponseInfo(),
                proxy_socket->CreateConnectResponseStream()));
        return ERR_IO_PENDING;
      }

    case OK:
      next_state_ = STATE_DONE;
      if (new_spdy_session_) {
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(
                &HttpStreamFactoryImpl::Job::OnSpdySessionReadyCallback,
                ptr_factory_.GetWeakPtr()));
      } else {
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(
                &HttpStreamFactoryImpl::Job::OnStreamReadyCallback,
                ptr_factory_.GetWeakPtr()));
      }
      return ERR_IO_PENDING;

    default:
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(
              &HttpStreamFactoryImpl::Job::OnStreamFailedCallback,
              ptr_factory_.GetWeakPtr(),
              result));
      return ERR_IO_PENDING;
  }
  return result;
}

int HttpStreamFactoryImpl::Job::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_START:
        DCHECK_EQ(OK, rv);
        rv = DoStart();
        break;
      case STATE_RESOLVE_PROXY:
        DCHECK_EQ(OK, rv);
        rv = DoResolveProxy();
        break;
      case STATE_RESOLVE_PROXY_COMPLETE:
        rv = DoResolveProxyComplete(rv);
        break;
      case STATE_WAIT_FOR_JOB:
        DCHECK_EQ(OK, rv);
        rv = DoWaitForJob();
        break;
      case STATE_WAIT_FOR_JOB_COMPLETE:
        rv = DoWaitForJobComplete(rv);
        break;
      case STATE_INIT_CONNECTION:
        DCHECK_EQ(OK, rv);
        rv = DoInitConnection();
        break;
      case STATE_INIT_CONNECTION_COMPLETE:
        rv = DoInitConnectionComplete(rv);
        break;
      case STATE_WAITING_USER_ACTION:
        rv = DoWaitingUserAction(rv);
        break;
      case STATE_RESTART_TUNNEL_AUTH:
        DCHECK_EQ(OK, rv);
        rv = DoRestartTunnelAuth();
        break;
      case STATE_RESTART_TUNNEL_AUTH_COMPLETE:
        rv = DoRestartTunnelAuthComplete(rv);
        break;
      case STATE_CREATE_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoCreateStream();
        break;
      case STATE_CREATE_STREAM_COMPLETE:
        rv = DoCreateStreamComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int HttpStreamFactoryImpl::Job::StartInternal() {
  CHECK_EQ(STATE_NONE, next_state_);
  next_state_ = STATE_START;
  int rv = RunLoop(OK);
  DCHECK_EQ(ERR_IO_PENDING, rv);
  return rv;
}

int HttpStreamFactoryImpl::Job::DoStart() {
  int port = request_info_.url.EffectiveIntPort();
  origin_ = HostPortPair(request_info_.url.HostNoBrackets(), port);
  origin_url_ = stream_factory_->ApplyHostMappingRules(
      request_info_.url, &origin_);
  http_pipelining_key_.reset(new HttpPipelinedHost::Key(origin_));

  net_log_.BeginEvent(NetLog::TYPE_HTTP_STREAM_JOB,
                      base::Bind(&NetLogHttpStreamJobCallback,
                                 &request_info_.url, &origin_url_));

  // Don't connect to restricted ports.
  if (!IsPortAllowedByDefault(port) && !IsPortAllowedByOverride(port)) {
    if (waiting_job_) {
      waiting_job_->Resume(this);
      waiting_job_ = NULL;
    }
    return ERR_UNSAFE_PORT;
  }

  next_state_ = STATE_RESOLVE_PROXY;
  return OK;
}

int HttpStreamFactoryImpl::Job::DoResolveProxy() {
  DCHECK(!pac_request_);

  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;

  if (request_info_.load_flags & LOAD_BYPASS_PROXY) {
    proxy_info_.UseDirect();
    return OK;
  }

  return session_->proxy_service()->ResolveProxy(
      request_info_.url, &proxy_info_, io_callback_, &pac_request_, net_log_);
}

int HttpStreamFactoryImpl::Job::DoResolveProxyComplete(int result) {
  pac_request_ = NULL;

  if (result == OK) {
    // Remove unsupported proxies from the list.
    proxy_info_.RemoveProxiesWithoutScheme(
        ProxyServer::SCHEME_DIRECT |
        ProxyServer::SCHEME_HTTP | ProxyServer::SCHEME_HTTPS |
        ProxyServer::SCHEME_SOCKS4 | ProxyServer::SCHEME_SOCKS5);

    if (proxy_info_.is_empty()) {
      // No proxies/direct to choose from. This happens when we don't support
      // any of the proxies in the returned list.
      result = ERR_NO_SUPPORTED_PROXIES;
    }
  }

  if (result != OK) {
    if (waiting_job_) {
      waiting_job_->Resume(this);
      waiting_job_ = NULL;
    }
    return result;
  }

  if (blocking_job_)
    next_state_ = STATE_WAIT_FOR_JOB;
  else
    next_state_ = STATE_INIT_CONNECTION;
  return OK;
}

bool HttpStreamFactoryImpl::Job::ShouldForceSpdySSL() const {
  bool rv = force_spdy_always_ && force_spdy_over_ssl_;
  return rv && !HttpStreamFactory::HasSpdyExclusion(origin_);
}

bool HttpStreamFactoryImpl::Job::ShouldForceSpdyWithoutSSL() const {
  bool rv = force_spdy_always_ && !force_spdy_over_ssl_;
  return rv && !HttpStreamFactory::HasSpdyExclusion(origin_);
}

bool HttpStreamFactoryImpl::Job::ShouldForceQuic() const {
#if defined(__LB_SHELL__)
  return false;
#else
  return session_->params().origin_port_to_force_quic_on == origin_.port()
      && session_->params().origin_port_to_force_quic_on != 0
      && proxy_info_.is_direct();
#endif
}

int HttpStreamFactoryImpl::Job::DoWaitForJob() {
  DCHECK(blocking_job_);
  next_state_ = STATE_WAIT_FOR_JOB_COMPLETE;
  return ERR_IO_PENDING;
}

int HttpStreamFactoryImpl::Job::DoWaitForJobComplete(int result) {
  DCHECK(!blocking_job_);
  DCHECK_EQ(OK, result);
  next_state_ = STATE_INIT_CONNECTION;
  return OK;
}

int HttpStreamFactoryImpl::Job::DoInitConnection() {
  DCHECK(!blocking_job_);
  DCHECK(!connection_->is_initialized());
  DCHECK(proxy_info_.proxy_server().is_valid());
  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  using_ssl_ = request_info_.url.SchemeIs("https") || ShouldForceSpdySSL();
  using_spdy_ = false;

  if (ShouldForceQuic()) {
    next_state_ = STATE_CREATE_STREAM;
    using_quic_ = true;
    return OK;
  }

#if !defined(COBALT_DISABLE_SPDY)
  // Check first if we have a spdy session for this group.  If so, then go
  // straight to using that.
  HostPortProxyPair spdy_session_key = GetSpdySessionKey();
  scoped_refptr<SpdySession> spdy_session =
      session_->spdy_session_pool()->GetIfExists(spdy_session_key, net_log_);
  if (spdy_session && CanUseExistingSpdySession()) {
    // If we're preconnecting, but we already have a SpdySession, we don't
    // actually need to preconnect any sockets, so we're done.
    if (IsPreconnecting())
      return OK;
    using_spdy_ = true;
    next_state_ = STATE_CREATE_STREAM;
    existing_spdy_session_ = spdy_session;
    return OK;
  } else if (request_ && !request_->HasSpdySessionKey() &&
             (using_ssl_ || ShouldForceSpdyWithoutSSL())) {
    // Update the spdy session key for the request that launched this job.
    request_->SetSpdySessionKey(spdy_session_key);
  } else if (IsRequestEligibleForPipelining()) {
    // TODO(simonjam): With pipelining, we might be better off using fewer
    // connections and thus should make fewer preconnections. Explore
    // preconnecting fewer than the requested num_connections.
    //
    // Separate note: A forced pipeline is always available if one exists for
    // this key. This is different than normal pipelines, which may be
    // unavailable or unusable. So, there is no need to worry about a race
    // between when a pipeline becomes available and when this job blocks.
    existing_available_pipeline_ = stream_factory_->http_pipelined_host_pool_.
        IsExistingPipelineAvailableForKey(*http_pipelining_key_.get());
    if (existing_available_pipeline_) {
      return OK;
    } else {
      bool was_new_key = request_->SetHttpPipeliningKey(
          *http_pipelining_key_.get());
      if (!was_new_key && session_->force_http_pipelining()) {
        return ERR_IO_PENDING;
      }
    }
  }
#endif  // !defined(COBALT_DISABLE_SPDY)

  // OK, there's no available SPDY session. Let |waiting_job_| resume if it's
  // paused.

  if (waiting_job_) {
    waiting_job_->Resume(this);
    waiting_job_ = NULL;
  }

  if (proxy_info_.is_http() || proxy_info_.is_https())
    establishing_tunnel_ = using_ssl_;

  bool want_spdy_over_npn = original_url_ != NULL;

  if (proxy_info_.is_https()) {
    InitSSLConfig(proxy_info_.proxy_server().host_port_pair(),
                  &proxy_ssl_config_);
    // Disable revocation checking for HTTPS proxies since the revocation
    // requests are probably going to need to go through the proxy too.
    proxy_ssl_config_.rev_checking_enabled = false;
  }
  if (using_ssl_) {
    InitSSLConfig(origin_, &server_ssl_config_);
  }

  if (IsPreconnecting()) {
    return PreconnectSocketsForHttpRequest(
        origin_url_,
        request_info_.extra_headers,
        request_info_.load_flags,
        request_info_.priority,
        session_,
        proxy_info_,
        ShouldForceSpdySSL(),
        want_spdy_over_npn,
        server_ssl_config_,
        proxy_ssl_config_,
        net_log_,
        num_streams_);
  } else {
    // If we can't use a SPDY session, don't both checking for one after
    // the hostname is resolved.
    OnHostResolutionCallback resolution_callback = CanUseExistingSpdySession() ?
        base::Bind(&Job::OnHostResolution, session_->spdy_session_pool(),
                   GetSpdySessionKey()) :
        OnHostResolutionCallback();
    return InitSocketHandleForHttpRequest(
        origin_url_, request_info_.extra_headers, request_info_.load_flags,
        request_info_.priority, session_, proxy_info_, ShouldForceSpdySSL(),
        want_spdy_over_npn, server_ssl_config_, proxy_ssl_config_, net_log_,
        connection_.get(), resolution_callback, io_callback_);
  }
}

int HttpStreamFactoryImpl::Job::DoInitConnectionComplete(int result) {
  if (IsPreconnecting()) {
    DCHECK_EQ(OK, result);
    return OK;
  }

  if (result == ERR_SPDY_SESSION_ALREADY_EXISTS) {
#if defined(COBALT_DISABLE_SPDY)
    NOTREACHED();
    return ERR_FAILED;
#else
    // We found a SPDY connection after resolving the host.  This is
    // probably an IP pooled connection.
    HostPortProxyPair spdy_session_key = GetSpdySessionKey();
    existing_spdy_session_ =
        session_->spdy_session_pool()->GetIfExists(spdy_session_key, net_log_);
    if (existing_spdy_session_) {
      using_spdy_ = true;
      next_state_ = STATE_CREATE_STREAM;
    } else {
      // It is possible that the spdy session no longer exists.
      ReturnToStateInitConnection(true /* close connection */);
    }
    return OK;
#endif  // !defined(COBALT_DISABLE_SPDY)
  }

  // TODO(willchan): Make this a bit more exact. Maybe there are recoverable
  // errors, such as ignoring certificate errors for Alternate-Protocol.
  if (result < 0 && waiting_job_) {
    waiting_job_->Resume(this);
    waiting_job_ = NULL;
  }

  if (result < 0 && session_->force_http_pipelining()) {
    stream_factory_->AbortPipelinedRequestsWithKey(
        this, *http_pipelining_key_.get(), result, server_ssl_config_);
  }

  // |result| may be the result of any of the stacked pools. The following
  // logic is used when determining how to interpret an error.
  // If |result| < 0:
  //   and connection_->socket() != NULL, then the SSL handshake ran and it
  //     is a potentially recoverable error.
  //   and connection_->socket == NULL and connection_->is_ssl_error() is true,
  //     then the SSL handshake ran with an unrecoverable error.
  //   otherwise, the error came from one of the other pools.
  bool ssl_started = using_ssl_ && (result == OK || connection_->socket() ||
                                    connection_->is_ssl_error());

  if (ssl_started && (result == OK || IsCertificateError(result))) {
    SSLClientSocket* ssl_socket =
      static_cast<SSLClientSocket*>(connection_->socket());
    if (ssl_socket->WasNpnNegotiated()) {
      was_npn_negotiated_ = true;
      std::string proto;
      std::string server_protos;
      SSLClientSocket::NextProtoStatus status =
          ssl_socket->GetNextProto(&proto, &server_protos);
      NextProto protocol_negotiated =
          SSLClientSocket::NextProtoFromString(proto);
      protocol_negotiated_ = protocol_negotiated;
      net_log_.AddEvent(
           NetLog::TYPE_HTTP_STREAM_REQUEST_PROTO,
           base::Bind(&NetLogHttpStreamProtoCallback,
                      status, &proto, &server_protos));
      if (ssl_socket->was_spdy_negotiated())
        SwitchToSpdyMode();
    }
    if (ShouldForceSpdySSL())
      SwitchToSpdyMode();
  } else if (proxy_info_.is_https() && connection_->socket() &&
        result == OK) {
    ProxyClientSocket* proxy_socket =
      static_cast<ProxyClientSocket*>(connection_->socket());
    if (proxy_socket->IsUsingSpdy()) {
      was_npn_negotiated_ = true;
      protocol_negotiated_ = proxy_socket->GetProtocolNegotiated();
      SwitchToSpdyMode();
    }
  }

  // We may be using spdy without SSL
  if (ShouldForceSpdyWithoutSSL())
    SwitchToSpdyMode();

  if (result == ERR_PROXY_AUTH_REQUESTED ||
      result == ERR_HTTPS_PROXY_TUNNEL_RESPONSE) {
    DCHECK(!ssl_started);
    // Other state (i.e. |using_ssl_|) suggests that |connection_| will have an
    // SSL socket, but there was an error before that could happen.  This
    // puts the in progress HttpProxy socket into |connection_| in order to
    // complete the auth (or read the response body).  The tunnel restart code
    // is careful to remove it before returning control to the rest of this
    // class.
    connection_.reset(connection_->release_pending_http_proxy_connection());
    return result;
  }

  if (!ssl_started && result < 0 && original_url_.get()) {
    // Mark the alternate protocol as broken and fallback.
    session_->http_server_properties()->SetBrokenAlternateProtocol(
        HostPortPair::FromURL(*original_url_));
    return result;
  }

  if (result < 0 && !ssl_started)
    return ReconsiderProxyAfterError(result);
  establishing_tunnel_ = false;

  if (connection_->socket()) {
    LogHttpConnectedMetrics(*connection_);

    // We officially have a new connection.  Record the type.
    if (!connection_->is_reused()) {
      ConnectionType type = using_spdy_ ? CONNECTION_SPDY : CONNECTION_HTTP;
      UpdateConnectionTypeHistograms(type);
    }
  }

  // Handle SSL errors below.
  if (using_ssl_) {
    DCHECK(ssl_started);
    if (IsCertificateError(result)) {
      if (using_spdy_ && original_url_.get() &&
          original_url_->SchemeIs("http")) {
        // We ignore certificate errors for http over spdy.
        spdy_certificate_error_ = result;
        result = OK;
      } else {
        result = HandleCertificateError(result);
        if (result == OK && !connection_->socket()->IsConnectedAndIdle()) {
          ReturnToStateInitConnection(true /* close connection */);
          return result;
        }
      }
    }
    if (result < 0)
      return result;
  }

  next_state_ = STATE_CREATE_STREAM;
  return OK;
}

int HttpStreamFactoryImpl::Job::DoWaitingUserAction(int result) {
  // This state indicates that the stream request is in a partially
  // completed state, and we've called back to the delegate for more
  // information.

  // We're always waiting here for the delegate to call us back.
  return ERR_IO_PENDING;
}

int HttpStreamFactoryImpl::Job::DoCreateStream() {
  DCHECK(connection_->socket() || existing_spdy_session_ ||
         existing_available_pipeline_ || using_quic_);

  next_state_ = STATE_CREATE_STREAM_COMPLETE;

  // We only set the socket motivation if we're the first to use
  // this socket.  Is there a race for two SPDY requests?  We really
  // need to plumb this through to the connect level.
  if (connection_->socket() && !connection_->is_reused())
    SetSocketMotivation();

  const ProxyServer& proxy_server = proxy_info_.proxy_server();

#if defined(__LB_SHELL__)
  if (false) {
#else
  if (using_quic_) {
#endif
    return quic_request_.Request(HostPortProxyPair(origin_, proxy_server),
                                 net_log_, io_callback_);
  }

#if defined(COBALT_DISABLE_SPDY)
  bool using_proxy = (proxy_info_.is_http() || proxy_info_.is_https()) &&
                     request_info_.url.SchemeIs("http");
  stream_.reset(new HttpBasicStream(connection_.release(), NULL, using_proxy));
  return OK;
#else
  if (!using_spdy_) {
    bool using_proxy = (proxy_info_.is_http() || proxy_info_.is_https()) &&
        request_info_.url.SchemeIs("http");
    if (stream_factory_->http_pipelined_host_pool_.
            IsExistingPipelineAvailableForKey(*http_pipelining_key_.get())) {
      stream_.reset(stream_factory_->http_pipelined_host_pool_.
                    CreateStreamOnExistingPipeline(
                        *http_pipelining_key_.get()));
      CHECK(stream_.get());
    } else if (!using_proxy && IsRequestEligibleForPipelining()) {
      // TODO(simonjam): Support proxies.
      stream_.reset(
          stream_factory_->http_pipelined_host_pool_.CreateStreamOnNewPipeline(
              *http_pipelining_key_.get(),
              connection_.release(),
              server_ssl_config_,
              proxy_info_,
              net_log_,
              was_npn_negotiated_,
              protocol_negotiated_));
      CHECK(stream_.get());
    } else {
      stream_.reset(new HttpBasicStream(connection_.release(), NULL,
                                        using_proxy));
    }
    return OK;
  }

  CHECK(!stream_.get());

  bool direct = true;
  HostPortProxyPair pair(origin_, proxy_server);
  if (IsHttpsProxyAndHttpUrl()) {
    // If we don't have a direct SPDY session, and we're using an HTTPS
    // proxy, then we might have a SPDY session to the proxy.
    pair = HostPortProxyPair(proxy_server.host_port_pair(),
                             ProxyServer::Direct());
    direct = false;
  }

  scoped_refptr<SpdySession> spdy_session;
  if (existing_spdy_session_) {
    // We picked up an existing session, so we don't need our socket.
    if (connection_->socket())
      connection_->socket()->Disconnect();
    connection_->Reset();
    spdy_session.swap(existing_spdy_session_);
  } else {
    SpdySessionPool* spdy_pool = session_->spdy_session_pool();
    spdy_session = spdy_pool->GetIfExists(pair, net_log_);
    if (!spdy_session) {
      int error = spdy_pool->GetSpdySessionFromSocket(
          pair, connection_.release(), net_log_, spdy_certificate_error_,
          &new_spdy_session_, using_ssl_);
      if (error != OK)
        return error;
      const HostPortPair& host_port_pair = pair.first;
      HttpServerProperties* http_server_properties =
          session_->http_server_properties();
      if (http_server_properties)
        http_server_properties->SetSupportsSpdy(host_port_pair, true);
      spdy_session_direct_ = direct;
      return OK;
    }
  }

  if (spdy_session->IsClosed())
    return ERR_CONNECTION_CLOSED;

  // TODO(willchan): Delete this code, because eventually, the
  // HttpStreamFactoryImpl will be creating all the SpdyHttpStreams, since it
  // will know when SpdySessions become available.

  bool use_relative_url = direct || request_info_.url.SchemeIs("https");
  stream_.reset(new SpdyHttpStream(spdy_session, use_relative_url));
  return OK;
#endif  // defined(COBALT_DISABLE_SPDY)
}

int HttpStreamFactoryImpl::Job::DoCreateStreamComplete(int result) {
  if (result < 0)
    return result;

#if defined(__LB_SHELL__)
  if (false) {
#else
  if (using_quic_) {
#endif
    stream_ = quic_request_.ReleaseStream();
  }

  session_->proxy_service()->ReportSuccess(proxy_info_);
  next_state_ = STATE_NONE;
  return OK;
}

int HttpStreamFactoryImpl::Job::DoRestartTunnelAuth() {
  next_state_ = STATE_RESTART_TUNNEL_AUTH_COMPLETE;
  ProxyClientSocket* proxy_socket =
      static_cast<ProxyClientSocket*>(connection_->socket());
  return proxy_socket->RestartWithAuth(io_callback_);
}

int HttpStreamFactoryImpl::Job::DoRestartTunnelAuthComplete(int result) {
  if (result == ERR_PROXY_AUTH_REQUESTED)
    return result;

  if (result == OK) {
    // Now that we've got the HttpProxyClientSocket connected.  We have
    // to release it as an idle socket into the pool and start the connection
    // process from the beginning.  Trying to pass it in with the
    // SSLSocketParams might cause a deadlock since params are dispatched
    // interchangeably.  This request won't necessarily get this http proxy
    // socket, but there will be forward progress.
    establishing_tunnel_ = false;
    ReturnToStateInitConnection(false /* do not close connection */);
    return OK;
  }

  return ReconsiderProxyAfterError(result);
}

void HttpStreamFactoryImpl::Job::ReturnToStateInitConnection(
    bool close_connection) {
  if (close_connection && connection_->socket())
    connection_->socket()->Disconnect();
  connection_->Reset();

  if (request_) {
    request_->RemoveRequestFromSpdySessionRequestMap();
    request_->RemoveRequestFromHttpPipeliningRequestMap();
  }

  next_state_ = STATE_INIT_CONNECTION;
}

void HttpStreamFactoryImpl::Job::SetSocketMotivation() {
  if (request_info_.motivation == HttpRequestInfo::PRECONNECT_MOTIVATED)
    connection_->socket()->SetSubresourceSpeculation();
  else if (request_info_.motivation == HttpRequestInfo::OMNIBOX_MOTIVATED)
    connection_->socket()->SetOmniboxSpeculation();
  // TODO(mbelshe): Add other motivations (like EARLY_LOAD_MOTIVATED).
}

bool HttpStreamFactoryImpl::Job::IsHttpsProxyAndHttpUrl() const {
  if (!proxy_info_.is_https())
    return false;
  if (original_url_.get()) {
    // We currently only support Alternate-Protocol where the original scheme
    // is http.
    DCHECK(original_url_->SchemeIs("http"));
    return original_url_->SchemeIs("http");
  }
  return request_info_.url.SchemeIs("http");
}

// Sets several fields of ssl_config for the given origin_server based on the
// proxy info and other factors.
void HttpStreamFactoryImpl::Job::InitSSLConfig(
    const HostPortPair& origin_server,
    SSLConfig* ssl_config) const {
  if (proxy_info_.is_https() && ssl_config->send_client_cert) {
    // When connecting through an HTTPS proxy, disable TLS False Start so
    // that client authentication errors can be distinguished between those
    // originating from the proxy server (ERR_PROXY_CONNECTION_FAILED) and
    // those originating from the endpoint (ERR_SSL_PROTOCOL_ERROR /
    // ERR_BAD_SSL_CLIENT_AUTH_CERT).
    // TODO(rch): This assumes that the HTTPS proxy will only request a
    // client certificate during the initial handshake.
    // http://crbug.com/59292
    ssl_config->false_start_enabled = false;
  }

  enum {
    FALLBACK_NONE = 0,    // SSL version fallback did not occur.
    FALLBACK_SSL3 = 1,    // Fell back to SSL 3.0.
    FALLBACK_TLS1 = 2,    // Fell back to TLS 1.0.
    FALLBACK_TLS1_1 = 3,  // Fell back to TLS 1.1.
    FALLBACK_MAX
  };

  int fallback = FALLBACK_NONE;
  if (ssl_config->version_fallback) {
    switch (ssl_config->version_max) {
      case SSL_PROTOCOL_VERSION_SSL3:
        fallback = FALLBACK_SSL3;
        break;
      case SSL_PROTOCOL_VERSION_TLS1:
        fallback = FALLBACK_TLS1;
        break;
      case SSL_PROTOCOL_VERSION_TLS1_1:
        fallback = FALLBACK_TLS1_1;
        break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Net.ConnectionUsedSSLVersionFallback",
                            fallback, FALLBACK_MAX);

  if (request_info_.load_flags & LOAD_VERIFY_EV_CERT)
    ssl_config->verify_ev_cert = true;
}


int HttpStreamFactoryImpl::Job::ReconsiderProxyAfterError(int error) {
  DCHECK(!pac_request_);

  // A failure to resolve the hostname or any error related to establishing a
  // TCP connection could be grounds for trying a new proxy configuration.
  //
  // Why do this when a hostname cannot be resolved?  Some URLs only make sense
  // to proxy servers.  The hostname in those URLs might fail to resolve if we
  // are still using a non-proxy config.  We need to check if a proxy config
  // now exists that corresponds to a proxy server that could load the URL.
  //
  switch (error) {
    case ERR_PROXY_CONNECTION_FAILED:
    case ERR_NAME_NOT_RESOLVED:
    case ERR_INTERNET_DISCONNECTED:
    case ERR_ADDRESS_UNREACHABLE:
    case ERR_CONNECTION_CLOSED:
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_REFUSED:
    case ERR_CONNECTION_ABORTED:
    case ERR_TIMED_OUT:
    case ERR_TUNNEL_CONNECTION_FAILED:
    case ERR_SOCKS_CONNECTION_FAILED:
      break;
    case ERR_SOCKS_CONNECTION_HOST_UNREACHABLE:
      // Remap the SOCKS-specific "host unreachable" error to a more
      // generic error code (this way consumers like the link doctor
      // know to substitute their error page).
      //
      // Note that if the host resolving was done by the SOCSK5 proxy, we can't
      // differentiate between a proxy-side "host not found" versus a proxy-side
      // "address unreachable" error, and will report both of these failures as
      // ERR_ADDRESS_UNREACHABLE.
      return ERR_ADDRESS_UNREACHABLE;
    default:
      return error;
  }

  if (request_info_.load_flags & LOAD_BYPASS_PROXY) {
    return error;
  }

  if (proxy_info_.is_https() && proxy_ssl_config_.send_client_cert) {
    session_->ssl_client_auth_cache()->Remove(
        proxy_info_.proxy_server().host_port_pair().ToString());
  }

  int rv = session_->proxy_service()->ReconsiderProxyAfterError(
      request_info_.url, &proxy_info_, io_callback_, &pac_request_, net_log_);
  if (rv == OK || rv == ERR_IO_PENDING) {
    // If the error was during connection setup, there is no socket to
    // disconnect.
    if (connection_->socket())
      connection_->socket()->Disconnect();
    connection_->Reset();
    if (request_) {
      request_->RemoveRequestFromSpdySessionRequestMap();
      request_->RemoveRequestFromHttpPipeliningRequestMap();
    }
    next_state_ = STATE_RESOLVE_PROXY_COMPLETE;
  } else {
    // If ReconsiderProxyAfterError() failed synchronously, it means
    // there was nothing left to fall-back to, so fail the transaction
    // with the last connection error we got.
    // TODO(eroman): This is a confusing contract, make it more obvious.
    rv = error;
  }

  return rv;
}

int HttpStreamFactoryImpl::Job::HandleCertificateError(int error) {
  DCHECK(using_ssl_);
  DCHECK(IsCertificateError(error));

  SSLClientSocket* ssl_socket =
      static_cast<SSLClientSocket*>(connection_->socket());
  ssl_socket->GetSSLInfo(&ssl_info_);

  // Add the bad certificate to the set of allowed certificates in the
  // SSL config object. This data structure will be consulted after calling
  // RestartIgnoringLastError(). And the user will be asked interactively
  // before RestartIgnoringLastError() is ever called.
  SSLConfig::CertAndStatus bad_cert;

  // |ssl_info_.cert| may be NULL if we failed to create
  // X509Certificate for whatever reason, but normally it shouldn't
  // happen, unless this code is used inside sandbox.
  if (ssl_info_.cert == NULL ||
      !X509Certificate::GetDEREncoded(ssl_info_.cert->os_cert_handle(),
                                      &bad_cert.der_cert)) {
    return error;
  }
  bad_cert.cert_status = ssl_info_.cert_status;
  server_ssl_config_.allowed_bad_certs.push_back(bad_cert);

  int load_flags = request_info_.load_flags;
  if (session_->params().ignore_certificate_errors)
    load_flags |= LOAD_IGNORE_ALL_CERT_ERRORS;
  if (ssl_socket->IgnoreCertError(error, load_flags))
    return OK;
  return error;
}

void HttpStreamFactoryImpl::Job::SwitchToSpdyMode() {
  if (HttpStreamFactory::spdy_enabled())
    using_spdy_ = true;
}

// static
void HttpStreamFactoryImpl::Job::LogHttpConnectedMetrics(
    const ClientSocketHandle& handle) {
  UMA_HISTOGRAM_ENUMERATION("Net.HttpSocketType", handle.reuse_type(),
                            ClientSocketHandle::NUM_TYPES);

  switch (handle.reuse_type()) {
    case ClientSocketHandle::UNUSED:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.HttpConnectionLatency",
                                 handle.setup_time(),
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
      break;
    case ClientSocketHandle::UNUSED_IDLE:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SocketIdleTimeBeforeNextUse_UnusedSocket",
                                 handle.idle_time(),
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(6),
                                 100);
      break;
    case ClientSocketHandle::REUSED_IDLE:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SocketIdleTimeBeforeNextUse_ReusedSocket",
                                 handle.idle_time(),
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(6),
                                 100);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool HttpStreamFactoryImpl::Job::IsPreconnecting() const {
  DCHECK_GE(num_streams_, 0);
  return num_streams_ > 0;
}

bool HttpStreamFactoryImpl::Job::IsOrphaned() const {
  return !IsPreconnecting() && !request_;
}

bool HttpStreamFactoryImpl::Job::IsRequestEligibleForPipelining() {
#if defined(COBALT_DISABLE_SPDY)
  return false;
#else
  if (IsPreconnecting() || !request_) {
    return false;
  }
  if (session_->force_http_pipelining()) {
    return true;
  }
  if (!session_->params().http_pipelining_enabled) {
    return false;
  }
  if (using_ssl_) {
    return false;
  }
  if (request_info_.method != "GET" && request_info_.method != "HEAD") {
    return false;
  }
  if (request_info_.load_flags &
      (net::LOAD_MAIN_FRAME | net::LOAD_SUB_FRAME | net::LOAD_PREFETCH |
       net::LOAD_IS_DOWNLOAD)) {
    // Avoid pipelining resources that may be streamed for a long time.
    return false;
  }
  return stream_factory_->http_pipelined_host_pool_.IsKeyEligibleForPipelining(
      *http_pipelining_key_.get());
#endif  // defined(COBALT_DISABLE_SPDY)
}

}  // namespace net
