// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl_job.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/http/http_request_info.h"
#include "net/http/http_stream_factory_impl_request.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

namespace {

GURL UpgradeUrlToHttps(const GURL& original_url) {
  GURL::Replacements replacements;
  // new_sheme and new_port need to be in scope here because GURL::Replacements
  // references the memory contained by them directly.
  const std::string new_scheme = "https";
  const std::string new_port = base::IntToString(443);
  replacements.SetSchemeStr(new_scheme);
  replacements.SetPortStr(new_port);
  return original_url.ReplaceComponents(replacements);
}

}  // namespace

HttpStreamFactoryImpl::Job::Job(HttpStreamFactoryImpl* stream_factory,
                                HttpNetworkSession* session)
    : request_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_callback_(this, &Job::OnIOComplete)),
      connection_(new ClientSocketHandle),
      session_(session),
      stream_factory_(stream_factory),
      next_state_(STATE_NONE),
      pac_request_(NULL),
      using_ssl_(false),
      using_spdy_(false),
      force_spdy_always_(HttpStreamFactory::force_spdy_always()),
      force_spdy_over_ssl_(HttpStreamFactory::force_spdy_over_ssl()),
      spdy_certificate_error_(OK),
      alternate_protocol_(HttpAlternateProtocols::UNINITIALIZED),
      establishing_tunnel_(false),
      was_alternate_protocol_available_(false),
      was_npn_negotiated_(false),
      num_streams_(0),
      spdy_session_direct_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  DCHECK(stream_factory);
  DCHECK(session);
  if (HttpStreamFactory::use_alternate_protocols())
    alternate_protocol_mode_ = kUnspecified;
  else
    alternate_protocol_mode_ = kDoNotUseAlternateProtocol;
}

HttpStreamFactoryImpl::Job::~Job() {
  net_log_.EndEvent(NetLog::TYPE_HTTP_STREAM_JOB, NULL);

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

void HttpStreamFactoryImpl::Job::Start(Request* request,
                                       const HttpRequestInfo& request_info,
                                       const SSLConfig& ssl_config,
                                       const BoundNetLog& net_log) {
  DCHECK(request);
  request_ = request;
  StartInternal(request_info, ssl_config, net_log);
}

int HttpStreamFactoryImpl::Job::Preconnect(int num_streams,
                                           const HttpRequestInfo& request_info,
                                           const SSLConfig& ssl_config,
                                           const BoundNetLog& net_log) {
  DCHECK_GT(num_streams, 0);
  num_streams_ = num_streams;
  return StartInternal(request_info, ssl_config, net_log);
}

int HttpStreamFactoryImpl::Job::RestartTunnelWithProxyAuth(
    const string16& username, const string16& password) {
  DCHECK(establishing_tunnel_);
  next_state_ = STATE_RESTART_TUNNEL_AUTH;
  stream_.reset();
  return RunLoop(OK);
}

LoadState HttpStreamFactoryImpl::Job::GetLoadState() const {
  switch (next_state_) {
    case STATE_RESOLVE_PROXY_COMPLETE:
      return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
    case STATE_CREATE_STREAM_COMPLETE:
      return connection_->GetLoadState();
    case STATE_INIT_CONNECTION_COMPLETE:
      return LOAD_STATE_SENDING_REQUEST;
    default:
      return LOAD_STATE_IDLE;
  }
}

bool HttpStreamFactoryImpl::Job::was_alternate_protocol_available() const {
  return was_alternate_protocol_available_;
}

bool HttpStreamFactoryImpl::Job::was_npn_negotiated() const {
  return was_npn_negotiated_;
}

bool HttpStreamFactoryImpl::Job::using_spdy() const {
  return using_spdy_;
}

const SSLConfig& HttpStreamFactoryImpl::Job::ssl_config() const {
  return ssl_config_;
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

void HttpStreamFactoryImpl::Job::OnStreamReadyCallback() {
  DCHECK(stream_.get());
  request_->Complete(was_alternate_protocol_available(),
                    was_npn_negotiated(),
                    using_spdy(),
                    net_log_.source());
  request_->OnStreamReady(ssl_config_, proxy_info_, stream_.release());
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnSpdySessionReadyCallback() {
  DCHECK(!stream_.get());
  DCHECK(using_spdy());
  DCHECK(new_spdy_session_);
  scoped_refptr<SpdySession> spdy_session = new_spdy_session_;
  new_spdy_session_ = NULL;
  stream_factory_->OnSpdySessionReady(this, spdy_session, spdy_session_direct_);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnStreamFailedCallback(int result) {
  request_->OnStreamFailed(result, ssl_config_);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnCertificateErrorCallback(
    int result, const SSLInfo& ssl_info) {
  request_->OnCertificateError(result, ssl_config_, ssl_info);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnNeedsProxyAuthCallback(
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller) {
  request_->OnNeedsProxyAuth(
      response, ssl_config_, proxy_info_, auth_controller);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnNeedsClientAuthCallback(
    SSLCertRequestInfo* cert_info) {
  request_->OnNeedsClientAuth(ssl_config_, cert_info);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnHttpsProxyTunnelResponseCallback(
    const HttpResponseInfo& response_info,
    HttpStream* stream) {
  request_->OnHttpsProxyTunnelResponse(
      response_info, ssl_config_, proxy_info_, stream);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnPreconnectsComplete() {
  stream_factory_->OnPreconnectsComplete(this);
  // |this| may be deleted after this call.
}

void HttpStreamFactoryImpl::Job::OnIOComplete(int result) {
  RunLoop(result);
}

int HttpStreamFactoryImpl::Job::RunLoop(int result) {
  result = DoLoop(result);

  if (result == ERR_IO_PENDING)
    return result;

  if (IsPreconnecting()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
            &HttpStreamFactoryImpl::Job::OnPreconnectsComplete));
    return ERR_IO_PENDING;
  }

  if (IsCertificateError(result)) {
    // Retrieve SSL information from the socket.
    GetSSLInfo();

    next_state_ = STATE_WAITING_USER_ACTION;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
            &HttpStreamFactoryImpl::Job::OnCertificateErrorCallback,
            result, ssl_info_));
    return ERR_IO_PENDING;
  }

  switch (result) {
    case ERR_PROXY_AUTH_REQUESTED:
      {
        DCHECK(connection_.get());
        DCHECK(connection_->socket());
        DCHECK(establishing_tunnel_);

        HttpProxyClientSocket* http_proxy_socket =
            static_cast<HttpProxyClientSocket*>(connection_->socket());
        const HttpResponseInfo* tunnel_auth_response =
            http_proxy_socket->GetConnectResponseInfo();

        next_state_ = STATE_WAITING_USER_ACTION;
        MessageLoop::current()->PostTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &HttpStreamFactoryImpl::Job::OnNeedsProxyAuthCallback,
                *tunnel_auth_response,
                http_proxy_socket->auth_controller()));
      }
      return ERR_IO_PENDING;

    case ERR_SSL_CLIENT_AUTH_CERT_NEEDED:
      MessageLoop::current()->PostTask(
          FROM_HERE,
          method_factory_.NewRunnableMethod(
              &HttpStreamFactoryImpl::Job::OnNeedsClientAuthCallback,
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
            method_factory_.NewRunnableMethod(
                &HttpStreamFactoryImpl::Job::OnHttpsProxyTunnelResponseCallback,
                *proxy_socket->GetConnectResponseInfo(),
                proxy_socket->CreateConnectResponseStream()));
        return ERR_IO_PENDING;
      }

    case OK:
      next_state_ = STATE_DONE;
      if (new_spdy_session_) {
        MessageLoop::current()->PostTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &HttpStreamFactoryImpl::Job::OnSpdySessionReadyCallback));
      } else {
        MessageLoop::current()->PostTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &HttpStreamFactoryImpl::Job::OnStreamReadyCallback));
      }
      return ERR_IO_PENDING;

    default:
      MessageLoop::current()->PostTask(
          FROM_HERE,
          method_factory_.NewRunnableMethod(
              &HttpStreamFactoryImpl::Job::OnStreamFailedCallback,
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
      case STATE_RESOLVE_PROXY:
        DCHECK_EQ(OK, rv);
        rv = DoResolveProxy();
        break;
      case STATE_RESOLVE_PROXY_COMPLETE:
        rv = DoResolveProxyComplete(rv);
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

int HttpStreamFactoryImpl::Job::StartInternal(
    const HttpRequestInfo& request_info,
    const SSLConfig& ssl_config,
    const BoundNetLog& net_log) {
  CHECK_EQ(STATE_NONE, next_state_);
  request_info_ = request_info;
  ssl_config_ = ssl_config;
  net_log_ = BoundNetLog::Make(net_log.net_log(),
                               NetLog::SOURCE_HTTP_STREAM_JOB);
  net_log_.BeginEvent(NetLog::TYPE_HTTP_STREAM_JOB,
                      make_scoped_refptr(new NetLogStringParameter(
                          "url", request_info.url.GetOrigin().spec())));
  next_state_ = STATE_RESOLVE_PROXY;
  int rv = RunLoop(OK);
  DCHECK_EQ(ERR_IO_PENDING, rv);
  return rv;
}

int HttpStreamFactoryImpl::Job::DoResolveProxy() {
  DCHECK(!pac_request_);

  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;

  // |endpoint_| indicates the final destination endpoint.
  endpoint_ = HostPortPair(request_info_.url.HostNoBrackets(),
                           request_info_.url.EffectiveIntPort());

  // Extra URL we might be attempting to resolve to.
  GURL alternate_endpoint_url = request_info_.url;

  // Tracks whether we are using |request_.url| or |alternate_endpoint_url|.
  const GURL *curr_endpoint_url = &request_info_.url;

  alternate_endpoint_url =
      HttpStreamFactory::ApplyHostMappingRules(
          alternate_endpoint_url, &endpoint_);

  const HttpAlternateProtocols& alternate_protocols =
      session_->alternate_protocols();
  if (HttpStreamFactory::spdy_enabled() &&
      alternate_protocols.HasAlternateProtocolFor(endpoint_)) {
    was_alternate_protocol_available_ = true;
    if (alternate_protocol_mode_ == kUnspecified) {
      HttpAlternateProtocols::PortProtocolPair alternate =
          alternate_protocols.GetAlternateProtocolFor(endpoint_);
      if (alternate.protocol != HttpAlternateProtocols::BROKEN) {
        DCHECK_LE(HttpAlternateProtocols::NPN_SPDY_1, alternate.protocol);
        DCHECK_GT(HttpAlternateProtocols::NUM_ALTERNATE_PROTOCOLS,
                  alternate.protocol);
        endpoint_.set_port(alternate.port);
        alternate_protocol_ = alternate.protocol;
        alternate_protocol_mode_ = kUsingAlternateProtocol;
        alternate_endpoint_url = UpgradeUrlToHttps(*curr_endpoint_url);
        curr_endpoint_url = &alternate_endpoint_url;
      }
    }
  }

  if (request_info_.load_flags & LOAD_BYPASS_PROXY) {
    proxy_info_.UseDirect();
    return OK;
  }

  return session_->proxy_service()->ResolveProxy(
      *curr_endpoint_url, &proxy_info_, &io_callback_, &pac_request_,
      net_log_);
}

int HttpStreamFactoryImpl::Job::DoResolveProxyComplete(int result) {
  pac_request_ = NULL;

  if (result != OK)
    return result;

  // TODO(mbelshe): consider retrying ResolveProxy if we came here via use of
  // AlternateProtocol.

  // Remove unsupported proxies from the list.
  proxy_info_.RemoveProxiesWithoutScheme(
      ProxyServer::SCHEME_DIRECT |
      ProxyServer::SCHEME_HTTP | ProxyServer::SCHEME_HTTPS |
      ProxyServer::SCHEME_SOCKS4 | ProxyServer::SCHEME_SOCKS5);

  if (proxy_info_.is_empty()) {
    // No proxies/direct to choose from. This happens when we don't support any
    // of the proxies in the returned list.
    return ERR_NO_SUPPORTED_PROXIES;
  }

  next_state_ = STATE_INIT_CONNECTION;
  return OK;
}

static bool HasSpdyExclusion(const HostPortPair& endpoint) {
  std::list<HostPortPair>* exclusions =
      HttpStreamFactory::forced_spdy_exclusions();
  if (!exclusions)
    return false;

  std::list<HostPortPair>::const_iterator it;
  for (it = exclusions->begin(); it != exclusions->end(); it++)
    if (it->Equals(endpoint))
      return true;
  return false;
}

bool HttpStreamFactoryImpl::Job::ShouldForceSpdySSL() {
  bool rv = force_spdy_always_ && force_spdy_over_ssl_;
  return rv && !HasSpdyExclusion(endpoint_);
}

bool HttpStreamFactoryImpl::Job::ShouldForceSpdyWithoutSSL() {
   bool rv = force_spdy_always_ && !force_spdy_over_ssl_;
  return rv && !HasSpdyExclusion(endpoint_);
}

int HttpStreamFactoryImpl::Job::DoInitConnection() {
  DCHECK(!connection_->is_initialized());
  DCHECK(proxy_info_.proxy_server().is_valid());
  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  bool want_spdy_over_npn =
      alternate_protocol_mode_ == kUsingAlternateProtocol &&
      alternate_protocol_ == HttpAlternateProtocols::NPN_SPDY_2;
  using_ssl_ = request_info_.url.SchemeIs("https") ||
      ShouldForceSpdySSL() || want_spdy_over_npn;
  using_spdy_ = false;

  // If spdy has been turned off on-the-fly, then there may be SpdySessions
  // still active.  But don't use them unless spdy is currently on.
  if (HttpStreamFactory::spdy_enabled() && !HasSpdyExclusion(endpoint_)) {
    // Check first if we have a spdy session for this group.  If so, then go
    // straight to using that.
    HostPortProxyPair spdy_session_key;
    if (IsHttpsProxyAndHttpUrl()) {
      spdy_session_key =
          HostPortProxyPair(proxy_info_.proxy_server().host_port_pair(),
                            ProxyServer::Direct());
    } else {
      spdy_session_key =
          HostPortProxyPair(endpoint_, proxy_info_.proxy_server());
    }
    if (session_->spdy_session_pool()->HasSession(spdy_session_key)) {
      // If we're preconnecting, but we already have a SpdySession, we don't
      // actually need to preconnect any sockets, so we're done.
      if (IsPreconnecting())
        return OK;
      using_spdy_ = true;
      next_state_ = STATE_CREATE_STREAM;
      return OK;
    } else if (!IsPreconnecting()) {
      // Update the spdy session key for the request that launched this job.
      request_->SetSpdySessionKey(spdy_session_key);
    }
  }

  // Build the string used to uniquely identify connections of this type.
  // Determine the host and port to connect to.
  std::string connection_group = endpoint_.ToString();
  DCHECK(!connection_group.empty());

  if (using_ssl_)
    connection_group = base::StringPrintf("ssl/%s", connection_group.c_str());

  // If the user is refreshing the page, bypass the host cache.
  bool disable_resolver_cache =
      request_info_.load_flags & LOAD_BYPASS_CACHE ||
      request_info_.load_flags & LOAD_VALIDATE_CACHE ||
      request_info_.load_flags & LOAD_DISABLE_CACHE;

  // Build up the connection parameters.
  scoped_refptr<TCPSocketParams> tcp_params;
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SOCKSSocketParams> socks_params;
  scoped_ptr<HostPortPair> proxy_host_port;

  if (proxy_info_.is_direct()) {
    tcp_params = new TCPSocketParams(endpoint_, request_info_.priority,
                                     request_info_.referrer,
                                     disable_resolver_cache);
  } else {
    ProxyServer proxy_server = proxy_info_.proxy_server();
    proxy_host_port.reset(new HostPortPair(proxy_server.host_port_pair()));
    scoped_refptr<TCPSocketParams> proxy_tcp_params(
        new TCPSocketParams(*proxy_host_port, request_info_.priority,
                            request_info_.referrer, disable_resolver_cache));

    if (proxy_info_.is_http() || proxy_info_.is_https()) {
      GURL authentication_url = request_info_.url;
      if (using_ssl_ && !authentication_url.SchemeIs("https")) {
        // If a proxy tunnel connection needs to be established due to
        // an Alternate-Protocol, the URL needs to be changed to indicate
        // https or digest authentication attempts will fail.
        // For example, suppose the initial request was for
        // "http://www.example.com/index.html". If this is an SSL
        // upgrade due to alternate protocol, the digest authorization
        // should have a uri="www.example.com:443" field rather than a
        // "/index.html" entry, even though the original request URL has not
        // changed.
        authentication_url = UpgradeUrlToHttps(authentication_url);
      }
      establishing_tunnel_ = using_ssl_;
      std::string user_agent;
      request_info_.extra_headers.GetHeader(HttpRequestHeaders::kUserAgent,
                                             &user_agent);
      scoped_refptr<SSLSocketParams> ssl_params;
      if (proxy_info_.is_https()) {
        // Set ssl_params, and unset proxy_tcp_params
        ssl_params = GenerateSSLParams(proxy_tcp_params, NULL, NULL,
                                       ProxyServer::SCHEME_DIRECT,
                                       *proxy_host_port.get(),
                                       want_spdy_over_npn);
        proxy_tcp_params = NULL;
      }

      http_proxy_params =
          new HttpProxySocketParams(proxy_tcp_params,
                                    ssl_params,
                                    authentication_url,
                                    user_agent,
                                    endpoint_,
                                    session_->http_auth_cache(),
                                    session_->http_auth_handler_factory(),
                                    session_->spdy_session_pool(),
                                    using_ssl_);
    } else {
      DCHECK(proxy_info_.is_socks());
      char socks_version;
      if (proxy_server.scheme() == ProxyServer::SCHEME_SOCKS5)
        socks_version = '5';
      else
        socks_version = '4';
      connection_group = base::StringPrintf(
          "socks%c/%s", socks_version, connection_group.c_str());

      socks_params = new SOCKSSocketParams(proxy_tcp_params,
                                           socks_version == '5',
                                           endpoint_,
                                           request_info_.priority,
                                           request_info_.referrer);
    }
  }

  // Deal with SSL - which layers on top of any given proxy.
  if (using_ssl_) {
    scoped_refptr<SSLSocketParams> ssl_params =
        GenerateSSLParams(tcp_params, http_proxy_params, socks_params,
                          proxy_info_.proxy_server().scheme(),
                          HostPortPair::FromURL(request_info_.url),
                          want_spdy_over_npn);
    SSLClientSocketPool* ssl_pool = NULL;
    if (proxy_info_.is_direct())
      ssl_pool = session_->ssl_socket_pool();
    else
      ssl_pool = session_->GetSocketPoolForSSLWithProxy(*proxy_host_port);

    if (IsPreconnecting()) {
      RequestSocketsForPool(ssl_pool, connection_group, ssl_params,
                            num_streams_, net_log_);
      return OK;
    }

    return connection_->Init(connection_group, ssl_params,
                             request_info_.priority, &io_callback_, ssl_pool,
                             net_log_);
  }

  // Finally, get the connection started.
  if (proxy_info_.is_http() || proxy_info_.is_https()) {
    HttpProxyClientSocketPool* pool =
        session_->GetSocketPoolForHTTPProxy(*proxy_host_port);
    if (IsPreconnecting()) {
      RequestSocketsForPool(pool, connection_group, http_proxy_params,
                            num_streams_, net_log_);
      return OK;
    }

    return connection_->Init(connection_group, http_proxy_params,
                             request_info_.priority, &io_callback_,
                             pool, net_log_);
  }

  if (proxy_info_.is_socks()) {
    SOCKSClientSocketPool* pool =
        session_->GetSocketPoolForSOCKSProxy(*proxy_host_port);
    if (IsPreconnecting()) {
      RequestSocketsForPool(pool, connection_group, socks_params,
                            num_streams_, net_log_);
      return OK;
    }

    return connection_->Init(connection_group, socks_params,
                             request_info_.priority, &io_callback_, pool,
                             net_log_);
  }

  DCHECK(proxy_info_.is_direct());

  TCPClientSocketPool* pool = session_->tcp_socket_pool();
  if (IsPreconnecting()) {
    RequestSocketsForPool(pool, connection_group, tcp_params,
                          num_streams_, net_log_);
    return OK;
  }

  return connection_->Init(connection_group, tcp_params,
                           request_info_.priority, &io_callback_,
                           pool, net_log_);
}

int HttpStreamFactoryImpl::Job::DoInitConnectionComplete(int result) {
  if (IsPreconnecting()) {
    DCHECK_EQ(OK, result);
    return OK;
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
    if (ssl_socket->was_npn_negotiated()) {
      was_npn_negotiated_ = true;
      if (ssl_socket->was_spdy_negotiated())
        SwitchToSpdyMode();
    }
    if (ShouldForceSpdySSL())
      SwitchToSpdyMode();
  } else if (proxy_info_.is_https() && connection_->socket() &&
        result == OK) {
    HttpProxyClientSocket* proxy_socket =
      static_cast<HttpProxyClientSocket*>(connection_->socket());
    if (proxy_socket->using_spdy()) {
      was_npn_negotiated_ = true;
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

  if ((!ssl_started && result < 0 &&
       alternate_protocol_mode_ == kUsingAlternateProtocol) ||
      result == ERR_NPN_NEGOTIATION_FAILED) {
    // Mark the alternate protocol as broken and fallback.
    MarkBrokenAlternateProtocolAndFallback();
    return OK;
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
      if (using_spdy_ && request_info_.url.SchemeIs("http")) {
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
  next_state_ = STATE_CREATE_STREAM_COMPLETE;

  // We only set the socket motivation if we're the first to use
  // this socket.  Is there a race for two SPDY requests?  We really
  // need to plumb this through to the connect level.
  if (connection_->socket() && !connection_->is_reused())
    SetSocketMotivation();

  const ProxyServer& proxy_server = proxy_info_.proxy_server();

  if (!using_spdy_) {
    bool using_proxy = (proxy_info_.is_http() || proxy_info_.is_https()) &&
        request_info_.url.SchemeIs("http");
    stream_.reset(new HttpBasicStream(connection_.release(), NULL,
                                      using_proxy));
    return OK;
  }

  CHECK(!stream_.get());

  bool direct = true;
  SpdySessionPool* spdy_pool = session_->spdy_session_pool();
  scoped_refptr<SpdySession> spdy_session;

  HostPortProxyPair pair(endpoint_, proxy_server);
  if (spdy_pool->HasSession(pair)) {
    // We have a SPDY session to the origin server.  This might be a direct
    // connection, or it might be a SPDY session through an HTTP or HTTPS proxy.
    spdy_session = spdy_pool->Get(pair, net_log_);
  } else if (IsHttpsProxyAndHttpUrl()) {
    // If we don't have a direct SPDY session, and we're using an HTTPS
    // proxy, then we might have a SPDY session to the proxy.
    pair = HostPortProxyPair(proxy_server.host_port_pair(),
                             ProxyServer::Direct());
    if (spdy_pool->HasSession(pair)) {
      spdy_session = spdy_pool->Get(pair, net_log_);
    }
    direct = false;
  }

  if (spdy_session.get()) {
    // We picked up an existing session, so we don't need our socket.
    if (connection_->socket())
      connection_->socket()->Disconnect();
    connection_->Reset();
  } else {
    // SPDY can be negotiated using the TLS next protocol negotiation (NPN)
    // extension, or just directly using SSL. Either way, |connection_| must
    // contain an SSLClientSocket.
    CHECK(connection_->socket());
    int error = spdy_pool->GetSpdySessionFromSocket(
        pair, connection_.release(), net_log_, spdy_certificate_error_,
        &spdy_session, using_ssl_);
    if (error != OK)
      return error;
    new_spdy_session_ = spdy_session;
    spdy_session_direct_ = direct;
    return OK;
  }

  if (spdy_session->IsClosed())
    return ERR_CONNECTION_CLOSED;

  // TODO(willchan): Delete this code, because eventually, the
  // HttpStreamFactoryImpl will be creating all the SpdyHttpStreams, since it
  // will know when SpdySessions become available. The above HasSession() checks
  // will be able to be deleted too.

  bool use_relative_url = direct || request_info_.url.SchemeIs("https");
  stream_.reset(new SpdyHttpStream(spdy_session, use_relative_url));
  return OK;
}

int HttpStreamFactoryImpl::Job::DoCreateStreamComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_NONE;
  return OK;
}

int HttpStreamFactoryImpl::Job::DoRestartTunnelAuth() {
  next_state_ = STATE_RESTART_TUNNEL_AUTH_COMPLETE;
  HttpProxyClientSocket* http_proxy_socket =
      static_cast<HttpProxyClientSocket*>(connection_->socket());
  return http_proxy_socket->RestartWithAuth(&io_callback_);
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

  if (request_)
    request_->RemoveRequestFromSpdySessionRequestMap();

  next_state_ = STATE_INIT_CONNECTION;
}

void HttpStreamFactoryImpl::Job::SetSocketMotivation() {
  if (request_info_.motivation == HttpRequestInfo::PRECONNECT_MOTIVATED)
    connection_->socket()->SetSubresourceSpeculation();
  else if (request_info_.motivation == HttpRequestInfo::OMNIBOX_MOTIVATED)
    connection_->socket()->SetOmniboxSpeculation();
  // TODO(mbelshe): Add other motivations (like EARLY_LOAD_MOTIVATED).
}

bool HttpStreamFactoryImpl::Job::IsHttpsProxyAndHttpUrl() {
  return proxy_info_.is_https() && request_info_.url.SchemeIs("http");
}

// Returns a newly create SSLSocketParams, and sets several
// fields of ssl_config_.
scoped_refptr<SSLSocketParams> HttpStreamFactoryImpl::Job::GenerateSSLParams(
    scoped_refptr<TCPSocketParams> tcp_params,
    scoped_refptr<HttpProxySocketParams> http_proxy_params,
    scoped_refptr<SOCKSSocketParams> socks_params,
    ProxyServer::Scheme proxy_scheme,
    const HostPortPair& host_and_port,
    bool want_spdy_over_npn) {

  if (stream_factory_->IsTLSIntolerantServer(request_info_.url)) {
    LOG(WARNING) << "Falling back to SSLv3 because host is TLS intolerant: "
        << GetHostAndPort(request_info_.url);
    ssl_config_.ssl3_fallback = true;
    ssl_config_.tls1_enabled = false;
  }

  if (proxy_info_.is_https() && ssl_config_.send_client_cert) {
    // When connecting through an HTTPS proxy, disable TLS False Start so
    // that client authentication errors can be distinguished between those
    // originating from the proxy server (ERR_PROXY_CONNECTION_FAILED) and
    // those originating from the endpoint (ERR_SSL_PROTOCOL_ERROR /
    // ERR_BAD_SSL_CLIENT_AUTH_CERT).
    // TODO(rch): This assumes that the HTTPS proxy will only request a
    // client certificate during the initial handshake.
    // http://crbug.com/59292
    ssl_config_.false_start_enabled = false;
  }

  UMA_HISTOGRAM_ENUMERATION("Net.ConnectionUsedSSLv3Fallback",
                            static_cast<int>(ssl_config_.ssl3_fallback), 2);

  int load_flags = request_info_.load_flags;
  if (HttpStreamFactory::ignore_certificate_errors())
    load_flags |= LOAD_IGNORE_ALL_CERT_ERRORS;
  if (request_info_.load_flags & LOAD_VERIFY_EV_CERT)
    ssl_config_.verify_ev_cert = true;

  if (proxy_info_.proxy_server().scheme() == ProxyServer::SCHEME_HTTP ||
      proxy_info_.proxy_server().scheme() == ProxyServer::SCHEME_HTTPS) {
    ssl_config_.mitm_proxies_allowed = true;
  }

  scoped_refptr<SSLSocketParams> ssl_params(
      new SSLSocketParams(tcp_params, socks_params, http_proxy_params,
                          proxy_scheme, host_and_port,
                          ssl_config_, load_flags,
                          ShouldForceSpdySSL(),
                          want_spdy_over_npn));

  return ssl_params;
}


void HttpStreamFactoryImpl::Job::MarkBrokenAlternateProtocolAndFallback() {
  // We have to:
  // * Reset the endpoint to be the unmodified URL specified destination.
  // * Mark the endpoint as broken so we don't try again.
  // * Set the alternate protocol mode to kDoNotUseAlternateProtocol so we
  // ignore future Alternate-Protocol headers from the HostPortPair.
  // * Reset the connection and go back to STATE_INIT_CONNECTION.

  endpoint_ = HostPortPair(request_info_.url.HostNoBrackets(),
                           request_info_.url.EffectiveIntPort());

  session_->mutable_alternate_protocols()->MarkBrokenAlternateProtocolFor(
      endpoint_);

  alternate_protocol_mode_ = kDoNotUseAlternateProtocol;
  ReturnToStateInitConnection(false /* close connection */);
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

  if (proxy_info_.is_https() && ssl_config_.send_client_cert) {
    session_->ssl_client_auth_cache()->Remove(
        proxy_info_.proxy_server().host_port_pair().ToString());
  }

  int rv = session_->proxy_service()->ReconsiderProxyAfterError(
      request_info_.url, &proxy_info_, &io_callback_, &pac_request_,
      net_log_);
  if (rv == OK || rv == ERR_IO_PENDING) {
    // If the error was during connection setup, there is no socket to
    // disconnect.
    if (connection_->socket())
      connection_->socket()->Disconnect();
    connection_->Reset();
    if (request_)
      request_->RemoveRequestFromSpdySessionRequestMap();
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
  // SSL info object. This data structure will be consulted after calling
  // RestartIgnoringLastError(). And the user will be asked interactively
  // before RestartIgnoringLastError() is ever called.
  SSLConfig::CertAndStatus bad_cert;
  bad_cert.cert = ssl_info_.cert;
  bad_cert.cert_status = ssl_info_.cert_status;
  ssl_config_.allowed_bad_certs.push_back(bad_cert);

  int load_flags = request_info_.load_flags;
  if (HttpStreamFactory::ignore_certificate_errors())
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

}  // namespace net
