// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_transaction.h"

#include "base/format_macros.h"
#include "base/scoped_ptr.h"
#include "base/compiler_specific.h"
#include "base/field_trial.h"
#include "base/histogram.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "build/build_config.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/upload_data_stream.h"
#include "net/flip/flip_session.h"
#include "net/flip/flip_session_pool.h"
#include "net/flip/flip_stream.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_chunked_decoder.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socks5_client_socket.h"
#include "net/socket/socks_client_socket.h"
#include "net/socket/ssl_client_socket.h"

using base::Time;

namespace net {

namespace {

void BuildRequestHeaders(const HttpRequestInfo* request_info,
                         const std::string& authorization_headers,
                         const UploadDataStream* upload_data_stream,
                         bool using_proxy,
                         std::string* request_headers) {
  const std::string path = using_proxy ?
      HttpUtil::SpecForRequest(request_info->url) :
      HttpUtil::PathForRequest(request_info->url);
  *request_headers =
      StringPrintf("%s %s HTTP/1.1\r\nHost: %s\r\n",
                   request_info->method.c_str(), path.c_str(),
                   GetHostAndOptionalPort(request_info->url).c_str());

  // For compat with HTTP/1.0 servers and proxies:
  if (using_proxy)
    *request_headers += "Proxy-";
  *request_headers += "Connection: keep-alive\r\n";

  if (!request_info->user_agent.empty()) {
    StringAppendF(request_headers, "User-Agent: %s\r\n",
                  request_info->user_agent.c_str());
  }

  // Our consumer should have made sure that this is a safe referrer.  See for
  // instance WebCore::FrameLoader::HideReferrer.
  if (request_info->referrer.is_valid())
    StringAppendF(request_headers, "Referer: %s\r\n",
                  request_info->referrer.spec().c_str());

  // Add a content length header?
  if (upload_data_stream) {
    StringAppendF(request_headers, "Content-Length: %" PRIu64 "\r\n",
                  upload_data_stream->size());
  } else if (request_info->method == "POST" || request_info->method == "PUT" ||
             request_info->method == "HEAD") {
    // An empty POST/PUT request still needs a content length.  As for HEAD,
    // IE and Safari also add a content length header.  Presumably it is to
    // support sending a HEAD request to an URL that only expects to be sent a
    // POST or some other method that normally would have a message body.
    *request_headers += "Content-Length: 0\r\n";
  }

  // Honor load flags that impact proxy caches.
  if (request_info->load_flags & LOAD_BYPASS_CACHE) {
    *request_headers += "Pragma: no-cache\r\nCache-Control: no-cache\r\n";
  } else if (request_info->load_flags & LOAD_VALIDATE_CACHE) {
    *request_headers += "Cache-Control: max-age=0\r\n";
  }

  if (!authorization_headers.empty()) {
    *request_headers += authorization_headers;
  }

  // TODO(darin): Need to prune out duplicate headers.

  *request_headers += request_info->extra_headers;
  *request_headers += "\r\n";
}

// The HTTP CONNECT method for establishing a tunnel connection is documented
// in draft-luotonen-web-proxy-tunneling-01.txt and RFC 2817, Sections 5.2 and
// 5.3.
void BuildTunnelRequest(const HttpRequestInfo* request_info,
                        const std::string& authorization_headers,
                        std::string* request_headers) {
  // RFC 2616 Section 9 says the Host request-header field MUST accompany all
  // HTTP/1.1 requests.  Add "Proxy-Connection: keep-alive" for compat with
  // HTTP/1.0 proxies such as Squid (required for NTLM authentication).
  *request_headers = StringPrintf(
      "CONNECT %s HTTP/1.1\r\nHost: %s\r\nProxy-Connection: keep-alive\r\n",
      GetHostAndPort(request_info->url).c_str(),
      GetHostAndOptionalPort(request_info->url).c_str());

  if (!request_info->user_agent.empty())
    StringAppendF(request_headers, "User-Agent: %s\r\n",
                  request_info->user_agent.c_str());

  if (!authorization_headers.empty()) {
    *request_headers += authorization_headers;
  }

  *request_headers += "\r\n";
}

}  // namespace

//-----------------------------------------------------------------------------

std::string* HttpNetworkTransaction::g_next_protos = NULL;

HttpNetworkTransaction::HttpNetworkTransaction(HttpNetworkSession* session)
    : pending_auth_target_(HttpAuth::AUTH_NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &HttpNetworkTransaction::OnIOComplete)),
      user_callback_(NULL),
      session_(session),
      request_(NULL),
      pac_request_(NULL),
      connection_(new ClientSocketHandle),
      reused_socket_(false),
      headers_valid_(false),
      logged_response_time(false),
      using_ssl_(false),
      proxy_mode_(kDirectConnection),
      establishing_tunnel_(false),
      embedded_identity_used_(false),
      read_buf_len_(0),
      next_state_(STATE_NONE) {
  session->ssl_config_service()->GetSSLConfig(&ssl_config_);
  if (g_next_protos)
    ssl_config_.next_protos = *g_next_protos;
}

// static
void HttpNetworkTransaction::SetNextProtos(const std::string& next_protos) {
  delete g_next_protos;
  g_next_protos = new std::string(next_protos);
}

int HttpNetworkTransaction::Start(const HttpRequestInfo* request_info,
                                  CompletionCallback* callback,
                                  LoadLog* load_log) {
  SIMPLE_STATS_COUNTER("HttpNetworkTransaction.Count");

  load_log_ = load_log;
  request_ = request_info;
  start_time_ = base::Time::Now();

  next_state_ = STATE_RESOLVE_PROXY;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartIgnoringLastError(
    CompletionCallback* callback) {
  if (connection_->socket()->IsConnectedAndIdle()) {
    next_state_ = STATE_SEND_REQUEST;
  } else {
    connection_->socket()->Disconnect();
    connection_->Reset();
    next_state_ = STATE_INIT_CONNECTION;
  }
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartWithCertificate(
    X509Certificate* client_cert,
    CompletionCallback* callback) {
  ssl_config_.client_cert = client_cert;
  if (client_cert) {
    session_->ssl_client_auth_cache()->Add(GetHostAndPort(request_->url),
                                           client_cert);
  }
  ssl_config_.send_client_cert = true;
  next_state_ = STATE_INIT_CONNECTION;
  // Reset the other member variables.
  // Note: this is necessary only with SSL renegotiation.
  ResetStateForRestart();
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartWithAuth(
    const std::wstring& username,
    const std::wstring& password,
    CompletionCallback* callback) {
  HttpAuth::Target target = pending_auth_target_;
  if (target == HttpAuth::AUTH_NONE) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  pending_auth_target_ = HttpAuth::AUTH_NONE;

  DCHECK(auth_identity_[target].invalid ||
         (username.empty() && password.empty()));

  if (auth_identity_[target].invalid) {
    // Update the username/password.
    auth_identity_[target].source = HttpAuth::IDENT_SRC_EXTERNAL;
    auth_identity_[target].invalid = false;
    auth_identity_[target].username = username;
    auth_identity_[target].password = password;
  }

  PrepareForAuthRestart(target);

  DCHECK(user_callback_ == NULL);
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;

  return rv;
}

void HttpNetworkTransaction::PrepareForAuthRestart(HttpAuth::Target target) {
  DCHECK(HaveAuth(target));
  DCHECK(auth_identity_[target].source != HttpAuth::IDENT_SRC_PATH_LOOKUP);

  // Add the auth entry to the cache before restarting. We don't know whether
  // the identity is valid yet, but if it is valid we want other transactions
  // to know about it. If an entry for (origin, handler->realm()) already
  // exists, we update it.
  //
  // If auth_identity_[target].source is HttpAuth::IDENT_SRC_NONE,
  // auth_identity_[target] contains no identity because identity is not
  // required yet.
  //
  // TODO(wtc): For NTLM_SSPI, we add the same auth entry to the cache in
  // round 1 and round 2, which is redundant but correct.  It would be nice
  // to add an auth entry to the cache only once, preferrably in round 1.
  // See http://crbug.com/21015.
  bool has_auth_identity =
      auth_identity_[target].source != HttpAuth::IDENT_SRC_NONE;
  if (has_auth_identity) {
    session_->auth_cache()->Add(AuthOrigin(target), auth_handler_[target],
        auth_identity_[target].username, auth_identity_[target].password,
        AuthPath(target));
  }

  bool keep_alive = false;
  // Even if the server says the connection is keep-alive, we have to be
  // able to find the end of each response in order to reuse the connection.
  if (GetResponseHeaders()->IsKeepAlive() &&
      http_stream_->CanFindEndOfResponse()) {
    // If the response body hasn't been completely read, we need to drain
    // it first.
    if (!http_stream_->IsResponseBodyComplete()) {
      next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART;
      read_buf_ = new IOBuffer(kDrainBodyBufferSize);  // A bit bucket.
      read_buf_len_ = kDrainBodyBufferSize;
      return;
    }
    keep_alive = true;
  }

  // We don't need to drain the response body, so we act as if we had drained
  // the response body.
  DidDrainBodyForAuthRestart(keep_alive);
}

void HttpNetworkTransaction::DidDrainBodyForAuthRestart(bool keep_alive) {
  if (keep_alive && connection_->socket()->IsConnectedAndIdle()) {
    // We should call connection_->set_idle_time(), but this doesn't occur
    // often enough to be worth the trouble.
    next_state_ = STATE_SEND_REQUEST;
    connection_->set_is_reused(true);
    reused_socket_ = true;
  } else {
    next_state_ = STATE_INIT_CONNECTION;
    connection_->socket()->Disconnect();
    connection_->Reset();
  }

  // Reset the other member variables.
  ResetStateForRestart();
}

int HttpNetworkTransaction::Read(IOBuffer* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK_LT(0, buf_len);

  State next_state = STATE_NONE;

  // Are we using SPDY or HTTP?
  if (spdy_stream_.get()) {
    DCHECK(!http_stream_.get());
    DCHECK(spdy_stream_->GetResponseInfo()->headers);
    next_state = STATE_SPDY_READ_BODY;
  } else {
    scoped_refptr<HttpResponseHeaders> headers = GetResponseHeaders();
    DCHECK(headers.get());
    next_state = STATE_READ_BODY;

    if (!connection_->is_initialized())
      return 0;  // connection_->has been reset.  Treat like EOF.

    if (establishing_tunnel_) {
      // We're trying to read the body of the response but we're still trying
      // to establish an SSL tunnel through the proxy.  We can't read these
      // bytes when establishing a tunnel because they might be controlled by
      // an active network attacker.  We don't worry about this for HTTP
      // because an active network attacker can already control HTTP sessions.
      // We reach this case when the user cancels a 407 proxy auth prompt.
      // See http://crbug.com/8473.
      DCHECK_EQ(407, headers->response_code());
      LogBlockedTunnelResponse(headers->response_code());
      return ERR_TUNNEL_CONNECTION_FAILED;
    }
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;

  next_state_ = next_state;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

const HttpResponseInfo* HttpNetworkTransaction::GetResponseInfo() const {
  return ((headers_valid_ && response_.headers) || response_.ssl_info.cert ||
          response_.cert_request_info) ? &response_ : NULL;
}

LoadState HttpNetworkTransaction::GetLoadState() const {
  // TODO(wtc): Define a new LoadState value for the
  // STATE_INIT_CONNECTION_COMPLETE state, which delays the HTTP request.
  switch (next_state_) {
    case STATE_RESOLVE_PROXY_COMPLETE:
      return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
    case STATE_INIT_CONNECTION_COMPLETE:
      return connection_->GetLoadState();
    case STATE_SEND_REQUEST_COMPLETE:
      return LOAD_STATE_SENDING_REQUEST;
    case STATE_READ_HEADERS_COMPLETE:
      return LOAD_STATE_WAITING_FOR_RESPONSE;
    case STATE_READ_BODY_COMPLETE:
      return LOAD_STATE_READING_RESPONSE;
    default:
      return LOAD_STATE_IDLE;
  }
}

uint64 HttpNetworkTransaction::GetUploadProgress() const {
  if (!http_stream_.get())
    return 0;

  return http_stream_->GetUploadProgress();
}

HttpNetworkTransaction::~HttpNetworkTransaction() {
  // If we still have an open socket, then make sure to disconnect it so it
  // won't call us back and we don't try to reuse it later on.
  if (connection_.get() && connection_->is_initialized())
    connection_->socket()->Disconnect();

  if (pac_request_)
    session_->proxy_service()->CancelPacRequest(pac_request_);

  if (spdy_stream_.get())
    spdy_stream_->Cancel();
}

void HttpNetworkTransaction::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);

  // Since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

void HttpNetworkTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int HttpNetworkTransaction::DoLoop(int result) {
  DCHECK(next_state_ != STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_PROXY:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.resolve_proxy", request_, request_->url.spec());
        rv = DoResolveProxy();
        break;
      case STATE_RESOLVE_PROXY_COMPLETE:
        rv = DoResolveProxyComplete(rv);
        TRACE_EVENT_END("http.resolve_proxy", request_, request_->url.spec());
        break;
      case STATE_INIT_CONNECTION:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.init_conn", request_, request_->url.spec());
        rv = DoInitConnection();
        break;
      case STATE_INIT_CONNECTION_COMPLETE:
        rv = DoInitConnectionComplete(rv);
        TRACE_EVENT_END("http.init_conn", request_, request_->url.spec());
        break;
      case STATE_SOCKS_CONNECT:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.socks_connect", request_, request_->url.spec());
        rv = DoSOCKSConnect();
        break;
      case STATE_SOCKS_CONNECT_COMPLETE:
        rv = DoSOCKSConnectComplete(rv);
        TRACE_EVENT_END("http.socks_connect", request_, request_->url.spec());
        break;
      case STATE_SSL_CONNECT:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.ssl_connect", request_, request_->url.spec());
        rv = DoSSLConnect();
        break;
      case STATE_SSL_CONNECT_COMPLETE:
        rv = DoSSLConnectComplete(rv);
        TRACE_EVENT_END("http.ssl_connect", request_, request_->url.spec());
        break;
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.send_request", request_, request_->url.spec());
        LoadLog::BeginEvent(load_log_,
                            LoadLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        TRACE_EVENT_END("http.send_request", request_, request_->url.spec());
        LoadLog::EndEvent(load_log_,
                          LoadLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST);
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.read_headers", request_, request_->url.spec());
        LoadLog::BeginEvent(load_log_,
                            LoadLog::TYPE_HTTP_TRANSACTION_READ_HEADERS);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        TRACE_EVENT_END("http.read_headers", request_, request_->url.spec());
        LoadLog::EndEvent(load_log_,
                          LoadLog::TYPE_HTTP_TRANSACTION_READ_HEADERS);
        break;
      case STATE_READ_BODY:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.read_body", request_, request_->url.spec());
        LoadLog::BeginEvent(load_log_,
                            LoadLog::TYPE_HTTP_TRANSACTION_READ_BODY);
        rv = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        rv = DoReadBodyComplete(rv);
        TRACE_EVENT_END("http.read_body", request_, request_->url.spec());
        LoadLog::EndEvent(load_log_,
                          LoadLog::TYPE_HTTP_TRANSACTION_READ_BODY);
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.drain_body_for_auth_restart",
                          request_, request_->url.spec());
        LoadLog::BeginEvent(
            load_log_,
            LoadLog::TYPE_HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART);
        rv = DoDrainBodyForAuthRestart();
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE:
        rv = DoDrainBodyForAuthRestartComplete(rv);
        TRACE_EVENT_END("http.drain_body_for_auth_restart",
                        request_, request_->url.spec());
        LoadLog::EndEvent(
            load_log_,
            LoadLog::TYPE_HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART);
        break;
      case STATE_SPDY_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.send_request", request_, request_->url.spec());
        LoadLog::BeginEvent(load_log_,
                            LoadLog::TYPE_FLIP_TRANSACTION_SEND_REQUEST);
        rv = DoSpdySendRequest();
        break;
      case STATE_SPDY_SEND_REQUEST_COMPLETE:
        rv = DoSpdySendRequestComplete(rv);
        TRACE_EVENT_END("http.send_request", request_, request_->url.spec());
        LoadLog::EndEvent(load_log_,
                          LoadLog::TYPE_FLIP_TRANSACTION_SEND_REQUEST);
        break;
      case STATE_SPDY_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.read_headers", request_, request_->url.spec());
        LoadLog::BeginEvent(load_log_,
                            LoadLog::TYPE_FLIP_TRANSACTION_READ_HEADERS);
        rv = DoSpdyReadHeaders();
        break;
      case STATE_SPDY_READ_HEADERS_COMPLETE:
        rv = DoSpdyReadHeadersComplete(rv);
        TRACE_EVENT_END("http.read_headers", request_, request_->url.spec());
        LoadLog::EndEvent(load_log_,
                          LoadLog::TYPE_FLIP_TRANSACTION_READ_HEADERS);
        break;
      case STATE_SPDY_READ_BODY:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.read_body", request_, request_->url.spec());
        LoadLog::BeginEvent(load_log_,
                            LoadLog::TYPE_FLIP_TRANSACTION_READ_BODY);
        rv = DoSpdyReadBody();
        break;
      case STATE_SPDY_READ_BODY_COMPLETE:
        rv = DoSpdyReadBodyComplete(rv);
        TRACE_EVENT_END("http.read_body", request_, request_->url.spec());
        LoadLog::EndEvent(load_log_,
                          LoadLog::TYPE_FLIP_TRANSACTION_READ_BODY);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int HttpNetworkTransaction::DoResolveProxy() {
  DCHECK(!pac_request_);

  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;

  if (request_->load_flags & LOAD_BYPASS_PROXY) {
    proxy_info_.UseDirect();
    return OK;
  }

  return session_->proxy_service()->ResolveProxy(
      request_->url, &proxy_info_, &io_callback_, &pac_request_, load_log_);
}

int HttpNetworkTransaction::DoResolveProxyComplete(int result) {

  pac_request_ = NULL;

  // Remove unsupported proxies from the list.
  proxy_info_.RemoveProxiesWithoutScheme(
      ProxyServer::SCHEME_DIRECT | ProxyServer::SCHEME_HTTP |
      ProxyServer::SCHEME_SOCKS4 | ProxyServer::SCHEME_SOCKS5);

  // There are four possible outcomes of having run the ProxyService:
  //   (1) The ProxyService decided we should connect through a proxy.
  //   (2) The ProxyService decided we should direct-connect.
  //   (3) The ProxyService decided we should give up, as there are no more
  //       proxies to try (this is more likely to happen during
  //       ReconsiderProxyAfterError()).
  //   (4) The ProxyService failed (which can happen if the PAC script
  //       we were configured with threw a runtime exception).
  //
  // It is important that we fail the connection in case (3) rather than
  // falling-back to a direct connection, since sending traffic through
  // a proxy may be integral to the user's privacy/security model.
  //
  // For example if a user had configured traffic to go through the TOR
  // anonymizing proxy to protect their privacy, it would be bad if we
  // silently fell-back to direct connect if the proxy server were to
  // become unreachable.
  //
  // In case (4) it is less obvious what the right thing to do is. On the
  // one hand, for consistency it would be natural to hard-fail as well.
  // However, both Firefox 3.5 and Internet Explorer 8 will silently fall-back
  // to DIRECT in this case, so we will do the same for compatibility.
  //
  // For more information, see:
  // http://www.chromium.org/developers/design-documents/proxy-settings-fallback

  if (proxy_info_.is_empty()) {
    // No proxies/direct to choose from. This happens when we don't support any
    // of the proxies in the returned list.
    return ERR_NO_SUPPORTED_PROXIES;
  }

  if (result != OK) {
    DLOG(ERROR) << "Failed to resolve proxy: " << result;
    // Fall-back to direct when there were runtime errors in the PAC script,
    // or some other failure with the settings.
    proxy_info_.UseDirect();
  }

  next_state_ = STATE_INIT_CONNECTION;
  return OK;
}

int HttpNetworkTransaction::DoInitConnection() {
  DCHECK(!connection_->is_initialized());
  DCHECK(proxy_info_.proxy_server().is_valid());

  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  using_ssl_ = request_->url.SchemeIs("https");

  if (proxy_info_.is_direct())
    proxy_mode_ = kDirectConnection;
  else if (proxy_info_.proxy_server().is_socks())
    proxy_mode_ = kSOCKSProxy;
  else if (using_ssl_)
    proxy_mode_ = kHTTPProxyUsingTunnel;
  else
    proxy_mode_ = kHTTPProxy;

  // Build the string used to uniquely identify connections of this type.
  // Determine the host and port to connect to.
  std::string connection_group;
  std::string host;
  int port;
  if (proxy_mode_ != kDirectConnection) {
    ProxyServer proxy_server = proxy_info_.proxy_server();
    connection_group = "proxy/" + proxy_server.ToURI() + "/";
    host = proxy_server.HostNoBrackets();
    port = proxy_server.port();
  } else {
    host = request_->url.HostNoBrackets();
    port = request_->url.EffectiveIntPort();
  }

  // Use the fixed testing ports if they've been provided.
  if (using_ssl_) {
    if (session_->fixed_https_port() != 0)
      port = session_->fixed_https_port();
  } else if (session_->fixed_http_port() != 0) {
    port = session_->fixed_http_port();
  }

  // For a connection via HTTP proxy not using CONNECT, the connection
  // is to the proxy server only. For all other cases
  // (direct, HTTP proxy CONNECT, SOCKS), the connection is upto the
  // url endpoint. Hence we append the url data into the connection_group.
  if (proxy_mode_ != kHTTPProxy)
    connection_group.append(request_->url.GetOrigin().spec());

  DCHECK(!connection_group.empty());

  HostResolver::RequestInfo resolve_info(host, port);

  // The referrer is used by the DNS prefetch system to correlate resolutions
  // with the page that triggered them. It doesn't impact the actual addresses
  // that we resolve to.
  resolve_info.set_referrer(request_->referrer);

  // If the user is refreshing the page, bypass the host cache.
  if (request_->load_flags & LOAD_BYPASS_CACHE ||
      request_->load_flags & LOAD_DISABLE_CACHE) {
    resolve_info.set_allow_cached_response(false);
  }

  // Check first if we have a flip session for this group.  If so, then go
  // straight to using that.
  if (session_->flip_session_pool()->HasSession(resolve_info))
    return OK;

  int rv = connection_->Init(connection_group, resolve_info, request_->priority,
                             &io_callback_, session_->tcp_socket_pool(),
                             load_log_);
  return rv;
}

int HttpNetworkTransaction::DoInitConnectionComplete(int result) {
  if (result < 0) {
    UpdateConnectionTypeHistograms(CONNECTION_HTTP, false);
    return ReconsiderProxyAfterError(result);
  }

  DCHECK_EQ(OK, result);

  // If we don't have an initialized connection, that means we have a flip
  // connection waiting for us.
  if (!connection_->is_initialized()) {
    next_state_ = STATE_SPDY_SEND_REQUEST;
    return OK;
  }

  LogTCPConnectedMetrics(*connection_);

  // Set the reused_socket_ flag to indicate that we are using a keep-alive
  // connection.  This flag is used to handle errors that occur while we are
  // trying to reuse a keep-alive connection.
  reused_socket_ = connection_->is_reused();
  if (reused_socket_) {
    next_state_ = STATE_SEND_REQUEST;
  } else {
    // Now we have a TCP connected socket.  Perform other connection setup as
    // needed.
    UpdateConnectionTypeHistograms(CONNECTION_HTTP, true);
    if (proxy_mode_ == kSOCKSProxy)
      next_state_ = STATE_SOCKS_CONNECT;
    else if (using_ssl_ && proxy_mode_ == kDirectConnection) {
      next_state_ = STATE_SSL_CONNECT;
    } else {
      next_state_ = STATE_SEND_REQUEST;
      if (proxy_mode_ == kHTTPProxyUsingTunnel)
        establishing_tunnel_ = true;
    }
  }

  return OK;
}

int HttpNetworkTransaction::DoSOCKSConnect() {
  DCHECK_EQ(kSOCKSProxy, proxy_mode_);

  next_state_ = STATE_SOCKS_CONNECT_COMPLETE;

  // Add a SOCKS connection on top of our existing transport socket.
  ClientSocket* s = connection_->release_socket();
  HostResolver::RequestInfo req_info(request_->url.HostNoBrackets(),
                                     request_->url.EffectiveIntPort());
  req_info.set_referrer(request_->referrer);

  if (proxy_info_.proxy_server().scheme() == ProxyServer::SCHEME_SOCKS5)
    s = new SOCKS5ClientSocket(s, req_info);
  else
    s = new SOCKSClientSocket(s, req_info, session_->host_resolver());
  connection_->set_socket(s);
  return connection_->socket()->Connect(&io_callback_, load_log_);
}

int HttpNetworkTransaction::DoSOCKSConnectComplete(int result) {
  DCHECK_EQ(kSOCKSProxy, proxy_mode_);

  if (result == OK) {
    if (using_ssl_) {
      next_state_ = STATE_SSL_CONNECT;
    } else {
      next_state_ = STATE_SEND_REQUEST;
    }
  } else {
    result = ReconsiderProxyAfterError(result);
  }
  return result;
}

int HttpNetworkTransaction::DoSSLConnect() {
  next_state_ = STATE_SSL_CONNECT_COMPLETE;

  if (request_->load_flags & LOAD_VERIFY_EV_CERT)
    ssl_config_.verify_ev_cert = true;

  ssl_connect_start_time_ = base::TimeTicks::Now();

  // Add a SSL socket on top of our existing transport socket.
  ClientSocket* s = connection_->release_socket();
  s = session_->socket_factory()->CreateSSLClientSocket(
      s, request_->url.HostNoBrackets(), ssl_config_);
  connection_->set_socket(s);
  return connection_->socket()->Connect(&io_callback_, load_log_);
}

int HttpNetworkTransaction::DoSSLConnectComplete(int result) {
  SSLClientSocket* ssl_socket =
      reinterpret_cast<SSLClientSocket*>(connection_->socket());

  SSLClientSocket::NextProtoStatus status =
      SSLClientSocket::kNextProtoUnsupported;
  std::string proto;
  // GetNextProto will fail and and trigger a NOTREACHED if we pass in a socket
  // that hasn't had SSL_ImportFD called on it. If we get a certificate error
  // here, then we know that we called SSL_ImportFD.
  if (result == OK || IsCertificateError(result))
    status = ssl_socket->GetNextProto(&proto);
  static const char kSpdyProto[] = "spdy";
  const bool use_spdy = (status == SSLClientSocket::kNextProtoNegotiated &&
                         proto == kSpdyProto);

  if (IsCertificateError(result)) {
    if (use_spdy) {
      // TODO(agl/willchan/wtc): We currently ignore certificate errors for
      // spdy but we shouldn't. http://crbug.com/32020
      result = OK;
    } else {
      result = HandleCertificateError(result);
    }
  }

  if (result == OK) {
    DCHECK(ssl_connect_start_time_ != base::TimeTicks());
    base::TimeDelta connect_duration =
        base::TimeTicks::Now() - ssl_connect_start_time_;

    if (use_spdy) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SpdyConnectionLatency",
          connect_duration,
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMinutes(10),
          100);

      UpdateConnectionTypeHistograms(CONNECTION_SPDY, true);
      next_state_ = STATE_SPDY_SEND_REQUEST;
    } else {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency",
          connect_duration,
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMinutes(10),
          100);

      next_state_ = STATE_SEND_REQUEST;
    }
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    result = HandleCertificateRequest(result);
  } else {
    result = HandleSSLHandshakeError(result);
  }
  return result;
}

int HttpNetworkTransaction::DoSendRequest() {
  next_state_ = STATE_SEND_REQUEST_COMPLETE;

  UploadDataStream* request_body = NULL;
  if (!establishing_tunnel_ && request_->upload_data)
    request_body = new UploadDataStream(request_->upload_data);

  // This is constructed lazily (instead of within our Start method), so that
  // we have proxy info available.
  if (request_headers_.empty()) {
    // Figure out if we can/should add Proxy-Authentication & Authentication
    // headers.
    bool have_proxy_auth =
        ShouldApplyProxyAuth() &&
        (HaveAuth(HttpAuth::AUTH_PROXY) ||
         SelectPreemptiveAuth(HttpAuth::AUTH_PROXY));
    bool have_server_auth =
        ShouldApplyServerAuth() &&
        (HaveAuth(HttpAuth::AUTH_SERVER) ||
         SelectPreemptiveAuth(HttpAuth::AUTH_SERVER));

    std::string authorization_headers;

    // TODO(wtc): If BuildAuthorizationHeader fails (returns an authorization
    // header with no credentials), we should return an error to prevent
    // entering an infinite auth restart loop.  See http://crbug.com/21050.
    if (have_proxy_auth)
      authorization_headers.append(
          BuildAuthorizationHeader(HttpAuth::AUTH_PROXY));
    if (have_server_auth)
      authorization_headers.append(
          BuildAuthorizationHeader(HttpAuth::AUTH_SERVER));

    if (establishing_tunnel_) {
      BuildTunnelRequest(request_, authorization_headers, &request_headers_);
    } else {
      BuildRequestHeaders(request_, authorization_headers, request_body,
                          proxy_mode_ == kHTTPProxy, &request_headers_);
    }
  }

  headers_valid_ = false;
  http_stream_.reset(new HttpBasicStream(connection_.get(), load_log_));

  return http_stream_->SendRequest(request_, request_headers_,
                                   request_body, &response_, &io_callback_);
}

int HttpNetworkTransaction::DoSendRequestComplete(int result) {
  if (result < 0)
    return HandleIOError(result);

  next_state_ = STATE_READ_HEADERS;

  return OK;
}

int HttpNetworkTransaction::DoReadHeaders() {
  next_state_ = STATE_READ_HEADERS_COMPLETE;

  return http_stream_->ReadResponseHeaders(&io_callback_);
}

int HttpNetworkTransaction::HandleConnectionClosedBeforeEndOfHeaders() {
  if (establishing_tunnel_) {
    // The connection was closed before the tunnel could be established.
    return ERR_TUNNEL_CONNECTION_FAILED;
  }

  if (!response_.headers) {
    // The connection was closed before any data was sent. Likely an error
    // rather than empty HTTP/0.9 response.
    return ERR_EMPTY_RESPONSE;
  }

  return OK;
}

int HttpNetworkTransaction::DoReadHeadersComplete(int result) {
  // We can get a certificate error or ERR_SSL_CLIENT_AUTH_CERT_NEEDED here
  // due to SSL renegotiation.
  if (using_ssl_) {
    if (IsCertificateError(result)) {
      // We don't handle a certificate error during SSL renegotiation, so we
      // have to return an error that's not in the certificate error range
      // (-2xx).
      LOG(ERROR) << "Got a server certificate with error " << result
                 << " during SSL renegotiation";
      result = ERR_CERT_ERROR_IN_SSL_RENEGOTIATION;
    } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
      result = HandleCertificateRequest(result);
      if (result == OK)
        return result;
    }
  }

  if (result < 0 && result != ERR_CONNECTION_CLOSED)
    return HandleIOError(result);

  if (result == ERR_CONNECTION_CLOSED && ShouldResendRequest(result)) {
    ResetConnectionAndRequestForResend();
    return OK;
  }

  // After we call RestartWithAuth a new response_time will be recorded, and
  // we need to be cautious about incorrectly logging the duration across the
  // authentication activity.
  if (!logged_response_time) {
    LogTransactionConnectedMetrics();
    logged_response_time = true;
  }

  if (result == ERR_CONNECTION_CLOSED) {
    int rv = HandleConnectionClosedBeforeEndOfHeaders();
    if (rv != OK)
      return rv;
    // TODO(wtc): Traditionally this code has returned 0 when reading a closed
    // socket.  That is partially corrected in classes that we call, but
    // callers need to be updated.
    result = 0;
  }

  if (response_.headers->GetParsedHttpVersion() < HttpVersion(1, 0)) {
    // Require the "HTTP/1.x" status line for SSL CONNECT.
    if (establishing_tunnel_)
      return ERR_TUNNEL_CONNECTION_FAILED;

    // HTTP/0.9 doesn't support the PUT method, so lack of response headers
    // indicates a buggy server.  See:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=193921
    if (request_->method == "PUT")
      return ERR_METHOD_NOT_SUPPORTED;
  }

  if (establishing_tunnel_) {
    switch (response_.headers->response_code()) {
      case 200:  // OK
        if (http_stream_->IsMoreDataBuffered()) {
          // The proxy sent extraneous data after the headers.
          return ERR_TUNNEL_CONNECTION_FAILED;
        }
        next_state_ = STATE_SSL_CONNECT;
        // Reset for the real request and response headers.
        request_headers_.clear();
        http_stream_.reset(new HttpBasicStream(connection_.get(), load_log_));
        headers_valid_ = false;
        establishing_tunnel_ = false;
        return OK;

      // We aren't able to CONNECT to the remote host through the proxy.  We
      // need to be very suspicious about the response because an active network
      // attacker can force us into this state by masquerading as the proxy.
      // The only safe thing to do here is to fail the connection because our
      // client is expecting an SSL protected response.
      // See http://crbug.com/7338.
      case 407:  // Proxy Authentication Required
        // We need this status code to allow proxy authentication.  Our
        // authentication code is smart enough to avoid being tricked by an
        // active network attacker.
        break;
      default:
        // For all other status codes, we conservatively fail the CONNECT
        // request.
        // We lose something by doing this.  We have seen proxy 403, 404, and
        // 501 response bodies that contain a useful error message.  For
        // example, Squid uses a 404 response to report the DNS error: "The
        // domain name does not exist."
        LogBlockedTunnelResponse(response_.headers->response_code());
        return ERR_TUNNEL_CONNECTION_FAILED;
    }
  }

  // Check for an intermediate 100 Continue response.  An origin server is
  // allowed to send this response even if we didn't ask for it, so we just
  // need to skip over it.
  // We treat any other 1xx in this same way (although in practice getting
  // a 1xx that isn't a 100 is rare).
  if (response_.headers->response_code() / 100 == 1) {
    response_.headers = new HttpResponseHeaders("");
    next_state_ = STATE_READ_HEADERS;
    return OK;
  }

  int rv = HandleAuthChallenge();
  if (rv != OK)
    return rv;

  if (using_ssl_ && !establishing_tunnel_) {
    SSLClientSocket* ssl_socket =
        reinterpret_cast<SSLClientSocket*>(connection_->socket());
    ssl_socket->GetSSLInfo(&response_.ssl_info);
  }

  headers_valid_ = true;
  return OK;
}

int HttpNetworkTransaction::DoReadBody() {
  DCHECK(read_buf_);
  DCHECK_GT(read_buf_len_, 0);
  DCHECK(connection_->is_initialized());

  next_state_ = STATE_READ_BODY_COMPLETE;
  return http_stream_->ReadResponseBody(read_buf_, read_buf_len_,
                                        &io_callback_);
}

int HttpNetworkTransaction::DoReadBodyComplete(int result) {
  // We are done with the Read call.
  DCHECK(!establishing_tunnel_) <<
      "We should never read a response body of a tunnel.";

  bool done = false, keep_alive = false;
  if (result < 0) {
    // Error or closed connection while reading the socket.
    done = true;
    // TODO(wtc): Traditionally this code has returned 0 when reading a closed
    // socket.  That is partially corrected in classes that we call, but
    // callers need to be updated.
    if (result == ERR_CONNECTION_CLOSED)
      result = 0;
  } else if (http_stream_->IsResponseBodyComplete()) {
    done = true;
    keep_alive = GetResponseHeaders()->IsKeepAlive();
  }

  // Clean up connection_->if we are done.
  if (done) {
    LogTransactionMetrics();
    if (!keep_alive)
      connection_->socket()->Disconnect();
    connection_->Reset();
    // The next Read call will return 0 (EOF).
  }

  // Clear these to avoid leaving around old state.
  read_buf_ = NULL;
  read_buf_len_ = 0;

  return result;
}

int HttpNetworkTransaction::DoDrainBodyForAuthRestart() {
  // This method differs from DoReadBody only in the next_state_.  So we just
  // call DoReadBody and override the next_state_.  Perhaps there is a more
  // elegant way for these two methods to share code.
  int rv = DoReadBody();
  DCHECK(next_state_ == STATE_READ_BODY_COMPLETE);
  next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE;
  return rv;
}

// TODO(wtc): This method and the DoReadBodyComplete method are almost
// the same.  Figure out a good way for these two methods to share code.
int HttpNetworkTransaction::DoDrainBodyForAuthRestartComplete(int result) {
  // keep_alive defaults to true because the very reason we're draining the
  // response body is to reuse the connection for auth restart.
  bool done = false, keep_alive = true;
  if (result < 0) {
    // Error or closed connection while reading the socket.
    done = true;
    keep_alive = false;
  } else if (http_stream_->IsResponseBodyComplete()) {
    done = true;
  }

  if (done) {
    DidDrainBodyForAuthRestart(keep_alive);
  } else {
    // Keep draining.
    next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART;
  }

  return OK;
}

int HttpNetworkTransaction::DoSpdySendRequest() {
  next_state_ = STATE_SPDY_SEND_REQUEST_COMPLETE;
  CHECK(!spdy_stream_.get());

  // First we get a SPDY session.  Theoretically, we've just negotiated one, but
  // if one already exists, then screw it, use the existing one!  Otherwise,
  // use the existing TCP socket.

  HostResolver::RequestInfo req_info(request_->url.HostNoBrackets(),
                                     request_->url.EffectiveIntPort());
  const scoped_refptr<FlipSessionPool> spdy_pool =
      session_->flip_session_pool();
  scoped_refptr<FlipSession> spdy_session;

  if (spdy_pool->HasSession(req_info)) {
    spdy_session = spdy_pool->Get(req_info, session_);
  } else {
    spdy_session = spdy_pool->GetFlipSessionFromSocket(
        req_info, session_, connection_.release());
  }

  CHECK(spdy_session.get());

  UploadDataStream* upload_data = request_->upload_data ?
      new UploadDataStream(request_->upload_data) : NULL;
  headers_valid_ = false;
  spdy_stream_ = spdy_session->GetOrCreateStream(
      *request_, upload_data, load_log_);
  return spdy_stream_->SendRequest(upload_data, &response_, &io_callback_);
}

int HttpNetworkTransaction::DoSpdySendRequestComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_SPDY_READ_HEADERS;
  return OK;
}

int HttpNetworkTransaction::DoSpdyReadHeaders() {
  next_state_ = STATE_SPDY_READ_HEADERS_COMPLETE;
  return spdy_stream_->ReadResponseHeaders(&io_callback_);
}

int HttpNetworkTransaction::DoSpdyReadHeadersComplete(int result) {
  // TODO(willchan): Flesh out the support for HTTP authentication here.
  if (result == OK)
    headers_valid_ = true;
  return result;
}

int HttpNetworkTransaction::DoSpdyReadBody() {
  next_state_ = STATE_SPDY_READ_BODY_COMPLETE;

  return spdy_stream_->ReadResponseBody(
      read_buf_, read_buf_len_, &io_callback_);
}

int HttpNetworkTransaction::DoSpdyReadBodyComplete(int result) {
  read_buf_ = NULL;
  read_buf_len_ = 0;

  if (result <= 0)
    spdy_stream_ = NULL;

  return result;
}

void HttpNetworkTransaction::LogTCPConnectedMetrics(
    const ClientSocketHandle& handle) {
  const base::TimeDelta time_to_obtain_connected_socket =
      base::TimeTicks::Now() - handle.init_time();

  static const bool use_late_binding_histogram =
      !FieldTrial::MakeName("", "SocketLateBinding").empty();

  if (handle.reuse_type() == ClientSocketHandle::UNUSED) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.HttpConnectionLatency",
        time_to_obtain_connected_socket,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  }

  UMA_HISTOGRAM_ENUMERATION("Net.TCPSocketType", handle.reuse_type(),
      ClientSocketHandle::NUM_TYPES);

  if (use_late_binding_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        FieldTrial::MakeName("Net.TCPSocketType", "SocketLateBinding"),
        handle.reuse_type(), ClientSocketHandle::NUM_TYPES);
  }

  UMA_HISTOGRAM_CLIPPED_TIMES(
      "Net.TransportSocketRequestTime",
      time_to_obtain_connected_socket,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
      100);

  if (use_late_binding_histogram) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        FieldTrial::MakeName("Net.TransportSocketRequestTime",
                             "SocketLateBinding").data(),
        time_to_obtain_connected_socket,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  }

  switch (handle.reuse_type()) {
    case ClientSocketHandle::UNUSED:
      break;
    case ClientSocketHandle::UNUSED_IDLE:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Net.SocketIdleTimeBeforeNextUse_UnusedSocket",
          handle.idle_time(), base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMinutes(6), 100);
      if (use_late_binding_histogram) {
        UMA_HISTOGRAM_CUSTOM_TIMES(
            FieldTrial::MakeName("Net.SocketIdleTimeBeforeNextUse_UnusedSocket",
                                 "SocketLateBinding").data(),
            handle.idle_time(), base::TimeDelta::FromMilliseconds(1),
            base::TimeDelta::FromMinutes(6), 100);
      }
      break;
    case ClientSocketHandle::REUSED_IDLE:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Net.SocketIdleTimeBeforeNextUse_ReusedSocket",
          handle.idle_time(), base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMinutes(6), 100);
      if (use_late_binding_histogram) {
        UMA_HISTOGRAM_CUSTOM_TIMES(
            FieldTrial::MakeName("Net.SocketIdleTimeBeforeNextUse_ReusedSocket",
                                 "SocketLateBinding").data(),
            handle.idle_time(), base::TimeDelta::FromMilliseconds(1),
            base::TimeDelta::FromMinutes(6), 100);
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

void HttpNetworkTransaction::LogIOErrorMetrics(
    const ClientSocketHandle& handle) {
  static const bool use_late_binding_histogram =
      !FieldTrial::MakeName("", "SocketLateBinding").empty();

  UMA_HISTOGRAM_ENUMERATION("Net.IOError_SocketReuseType",
       handle.reuse_type(), ClientSocketHandle::NUM_TYPES);

  if (use_late_binding_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        FieldTrial::MakeName("Net.IOError_SocketReuseType",
                             "SocketLateBinding"),
        handle.reuse_type(), ClientSocketHandle::NUM_TYPES);
  }

  switch (handle.reuse_type()) {
    case ClientSocketHandle::UNUSED:
      break;
    case ClientSocketHandle::UNUSED_IDLE:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Net.SocketIdleTimeOnIOError2_UnusedSocket",
          handle.idle_time(), base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMinutes(6), 100);
      if (use_late_binding_histogram) {
        UMA_HISTOGRAM_CUSTOM_TIMES(
            FieldTrial::MakeName("Net.SocketIdleTimeOnIOError2_UnusedSocket",
                                 "SocketLateBinding").data(),
            handle.idle_time(), base::TimeDelta::FromMilliseconds(1),
            base::TimeDelta::FromMinutes(6), 100);
      }
      break;
    case ClientSocketHandle::REUSED_IDLE:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Net.SocketIdleTimeOnIOError2_ReusedSocket",
          handle.idle_time(), base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMinutes(6), 100);
      if (use_late_binding_histogram) {
        UMA_HISTOGRAM_CUSTOM_TIMES(
            FieldTrial::MakeName("Net.SocketIdleTimeOnIOError2_ReusedSocket",
                                 "SocketLateBinding").data(),
            handle.idle_time(), base::TimeDelta::FromMilliseconds(1),
            base::TimeDelta::FromMinutes(6), 100);
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

void HttpNetworkTransaction::LogTransactionConnectedMetrics() const {
  base::TimeDelta total_duration = response_.response_time - start_time_;

  UMA_HISTOGRAM_CLIPPED_TIMES(
      "Net.Transaction_Connected_Under_10",
      total_duration,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
      100);

  static const bool use_late_binding_histogram =
      !FieldTrial::MakeName("", "SocketLateBinding").empty();

  if (use_late_binding_histogram) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        FieldTrial::MakeName("Net.Transaction_Connected_Under_10",
                             "SocketLateBinding").data(),
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  }

  if (!reused_socket_) {
    UMA_HISTOGRAM_CLIPPED_TIMES(
        "Net.Transaction_Connected_New",
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  }

  // Currently, non-zero priority requests are frame or sub-frame resource
  // types.  This will change when we also prioritize certain subresources like
  // css, js, etc.
  if (request_->priority) {
    UMA_HISTOGRAM_CLIPPED_TIMES(
        "Net.Priority_High_Latency",
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  } else {
    UMA_HISTOGRAM_CLIPPED_TIMES(
        "Net.Priority_Low_Latency",
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  }
}

void HttpNetworkTransaction::LogTransactionMetrics() const {
  base::TimeDelta duration = base::Time::Now() -
      response_.request_time;
  if (60 < duration.InMinutes())
    return;

  base::TimeDelta total_duration = base::Time::Now() - start_time_;

  UMA_HISTOGRAM_LONG_TIMES("Net.Transaction_Latency", duration);
  UMA_HISTOGRAM_CLIPPED_TIMES("Net.Transaction_Latency_Under_10", duration,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
      100);
  UMA_HISTOGRAM_CLIPPED_TIMES("Net.Transaction_Latency_Total_Under_10",
      total_duration, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMinutes(10), 100);
  if (!reused_socket_) {
    UMA_HISTOGRAM_CLIPPED_TIMES(
        "Net.Transaction_Latency_Total_New_Connection_Under_10",
        total_duration, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10), 100);
  }
}

void HttpNetworkTransaction::LogBlockedTunnelResponse(
    int response_code) const {
  LOG(WARNING) << "Blocked proxy response with status " << response_code
               << " to CONNECT request for "
               << GetHostAndPort(request_->url) << ".";
}

int HttpNetworkTransaction::HandleCertificateError(int error) {
  DCHECK(using_ssl_);

  const int kCertFlags = LOAD_IGNORE_CERT_COMMON_NAME_INVALID |
                         LOAD_IGNORE_CERT_DATE_INVALID |
                         LOAD_IGNORE_CERT_AUTHORITY_INVALID |
                         LOAD_IGNORE_CERT_WRONG_USAGE;
  if (request_->load_flags & kCertFlags) {
    switch (error) {
      case ERR_CERT_COMMON_NAME_INVALID:
        if (request_->load_flags & LOAD_IGNORE_CERT_COMMON_NAME_INVALID)
          error = OK;
        break;
      case ERR_CERT_DATE_INVALID:
        if (request_->load_flags & LOAD_IGNORE_CERT_DATE_INVALID)
          error = OK;
        break;
      case ERR_CERT_AUTHORITY_INVALID:
        if (request_->load_flags & LOAD_IGNORE_CERT_AUTHORITY_INVALID)
          error = OK;
        break;
    }
  }

  if (error != OK) {
    SSLClientSocket* ssl_socket =
        reinterpret_cast<SSLClientSocket*>(connection_->socket());
    ssl_socket->GetSSLInfo(&response_.ssl_info);

    // Add the bad certificate to the set of allowed certificates in the
    // SSL info object. This data structure will be consulted after calling
    // RestartIgnoringLastError(). And the user will be asked interactively
    // before RestartIgnoringLastError() is ever called.
    SSLConfig::CertAndStatus bad_cert;
    bad_cert.cert = response_.ssl_info.cert;
    bad_cert.cert_status = response_.ssl_info.cert_status;
    ssl_config_.allowed_bad_certs.push_back(bad_cert);
  }
  return error;
}

int HttpNetworkTransaction::HandleCertificateRequest(int error) {
  // Assert that the socket did not send a client certificate.
  // Note: If we got a reused socket, it was created with some other
  // transaction's ssl_config_, so we need to disable this assertion.  We can
  // get a certificate request on a reused socket when the server requested
  // renegotiation (rehandshake).
  // TODO(wtc): add a GetSSLParams method to SSLClientSocket so we can query
  // the SSL parameters it was created with and get rid of the reused_socket_
  // test.
  DCHECK(reused_socket_ || !ssl_config_.send_client_cert);

  response_.cert_request_info = new SSLCertRequestInfo;
  SSLClientSocket* ssl_socket =
      reinterpret_cast<SSLClientSocket*>(connection_->socket());
  ssl_socket->GetSSLCertRequestInfo(response_.cert_request_info);

  // Close the connection while the user is selecting a certificate to send
  // to the server.
  connection_->socket()->Disconnect();
  connection_->Reset();

  // If the user selected one of the certificate in client_certs for this
  // server before, use it automatically.
  X509Certificate* client_cert = session_->ssl_client_auth_cache()->
      Lookup(GetHostAndPort(request_->url));
  if (client_cert) {
    const std::vector<scoped_refptr<X509Certificate> >& client_certs =
        response_.cert_request_info->client_certs;
    for (size_t i = 0; i < client_certs.size(); ++i) {
      if (client_cert->fingerprint().Equals(client_certs[i]->fingerprint())) {
        ssl_config_.client_cert = client_cert;
        ssl_config_.send_client_cert = true;
        next_state_ = STATE_INIT_CONNECTION;
        // Reset the other member variables.
        // Note: this is necessary only with SSL renegotiation.
        ResetStateForRestart();
        return OK;
      }
    }
  }
  return error;
}

int HttpNetworkTransaction::HandleSSLHandshakeError(int error) {
  if (ssl_config_.send_client_cert &&
     (error == ERR_SSL_PROTOCOL_ERROR ||
      error == ERR_BAD_SSL_CLIENT_AUTH_CERT)) {
    session_->ssl_client_auth_cache()->Remove(GetHostAndPort(request_->url));
  }

  switch (error) {
    case ERR_SSL_PROTOCOL_ERROR:
    case ERR_SSL_VERSION_OR_CIPHER_MISMATCH:
      if (ssl_config_.tls1_enabled) {
        // This could be a TLS-intolerant server or an SSL 3.0 server that
        // chose a TLS-only cipher suite.  Turn off TLS 1.0 and retry.
        ssl_config_.tls1_enabled = false;
        connection_->socket()->Disconnect();
        connection_->Reset();
        next_state_ = STATE_INIT_CONNECTION;
        error = OK;
      }
      break;
  }
  return error;
}

// This method determines whether it is safe to resend the request after an
// IO error.  It can only be called in response to request header or body
// write errors or response header read errors.  It should not be used in
// other cases, such as a Connect error.
int HttpNetworkTransaction::HandleIOError(int error) {
  switch (error) {
    // If we try to reuse a connection that the server is in the process of
    // closing, we may end up successfully writing out our request (or a
    // portion of our request) only to find a connection error when we try to
    // read from (or finish writing to) the socket.
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_CLOSED:
    case ERR_CONNECTION_ABORTED:
      LogIOErrorMetrics(*connection_);
      if (ShouldResendRequest(error)) {
        ResetConnectionAndRequestForResend();
        error = OK;
      }
      break;
  }
  return error;
}

void HttpNetworkTransaction::ResetStateForRestart() {
  pending_auth_target_ = HttpAuth::AUTH_NONE;
  read_buf_ = NULL;
  read_buf_len_ = 0;
  http_stream_.reset();
  headers_valid_ = false;
  request_headers_.clear();
  response_ = HttpResponseInfo();
}

HttpResponseHeaders* HttpNetworkTransaction::GetResponseHeaders() const {
  return response_.headers;
}

bool HttpNetworkTransaction::ShouldResendRequest(int error) const {
  // NOTE: we resend a request only if we reused a keep-alive connection.
  // This automatically prevents an infinite resend loop because we'll run
  // out of the cached keep-alive connections eventually.
  if (establishing_tunnel_ ||
      !connection_->ShouldResendFailedRequest(error) ||
      GetResponseHeaders()) {  // We have received some response headers.
    return false;
  }
  return true;
}

void HttpNetworkTransaction::ResetConnectionAndRequestForResend() {
  connection_->socket()->Disconnect();
  connection_->Reset();
  // We need to clear request_headers_ because it contains the real request
  // headers, but we may need to resend the CONNECT request first to recreate
  // the SSL tunnel.
  request_headers_.clear();
  next_state_ = STATE_INIT_CONNECTION;  // Resend the request.
}

int HttpNetworkTransaction::ReconsiderProxyAfterError(int error) {
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
    case ERR_NAME_NOT_RESOLVED:
    case ERR_INTERNET_DISCONNECTED:
    case ERR_ADDRESS_UNREACHABLE:
    case ERR_CONNECTION_CLOSED:
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_REFUSED:
    case ERR_CONNECTION_ABORTED:
    case ERR_TIMED_OUT:
    case ERR_TUNNEL_CONNECTION_FAILED:
      break;
    default:
      return error;
  }

  if (request_->load_flags & LOAD_BYPASS_PROXY) {
    return error;
  }

  int rv = session_->proxy_service()->ReconsiderProxyAfterError(
      request_->url, &proxy_info_, &io_callback_, &pac_request_, load_log_);
  if (rv == OK || rv == ERR_IO_PENDING) {
    // If the error was during connection setup, there is no socket to
    // disconnect.
    if (connection_->socket())
      connection_->socket()->Disconnect();
    connection_->Reset();
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

bool HttpNetworkTransaction::ShouldApplyProxyAuth() const {
  return (proxy_mode_ == kHTTPProxy) || establishing_tunnel_;
}

bool HttpNetworkTransaction::ShouldApplyServerAuth() const {
  return !establishing_tunnel_ &&
      !(request_->load_flags & LOAD_DO_NOT_SEND_AUTH_DATA);
}

std::string HttpNetworkTransaction::BuildAuthorizationHeader(
    HttpAuth::Target target) const {
  DCHECK(HaveAuth(target));

  // Add a Authorization/Proxy-Authorization header line.
  std::string credentials = auth_handler_[target]->GenerateCredentials(
      auth_identity_[target].username,
      auth_identity_[target].password,
      request_,
      &proxy_info_);

  return HttpAuth::GetAuthorizationHeaderName(target) +
      ": "  + credentials + "\r\n";
}

GURL HttpNetworkTransaction::AuthOrigin(HttpAuth::Target target) const {
  return target == HttpAuth::AUTH_PROXY ?
      GURL("http://" + proxy_info_.proxy_server().host_and_port()) :
      request_->url.GetOrigin();
}

std::string HttpNetworkTransaction::AuthPath(HttpAuth::Target target)
    const {
  // Proxy authentication realms apply to all paths. So we will use
  // empty string in place of an absolute path.
  return target == HttpAuth::AUTH_PROXY ?
      std::string() : request_->url.path();
}

// static
std::string HttpNetworkTransaction::AuthTargetString(
    HttpAuth::Target target) {
  return target == HttpAuth::AUTH_PROXY ? "proxy" : "server";
}

void HttpNetworkTransaction::InvalidateRejectedAuthFromCache(
    HttpAuth::Target target,
    const GURL& auth_origin) {
  DCHECK(HaveAuth(target));

  // TODO(eroman): this short-circuit can be relaxed. If the realm of
  // the preemptively used auth entry matches the realm of the subsequent
  // challenge, then we can invalidate the preemptively used entry.
  // Otherwise as-is we may send the failed credentials one extra time.
  if (auth_identity_[target].source == HttpAuth::IDENT_SRC_PATH_LOOKUP)
    return;

  // Clear the cache entry for the identity we just failed on.
  // Note: we require the username/password to match before invalidating
  // since the entry in the cache may be newer than what we used last time.
  session_->auth_cache()->Remove(auth_origin,
                                 auth_handler_[target]->realm(),
                                 auth_identity_[target].username,
                                 auth_identity_[target].password);
}

bool HttpNetworkTransaction::SelectPreemptiveAuth(HttpAuth::Target target) {
  DCHECK(!HaveAuth(target));

  // Don't do preemptive authorization if the URL contains a username/password,
  // since we must first be challenged in order to use the URL's identity.
  if (request_->url.has_username())
    return false;

  // SelectPreemptiveAuth() is on the critical path for each request, so it
  // is expected to be fast. LookupByPath() is fast in the common case, since
  // the number of http auth cache entries is expected to be very small.
  // (For most users in fact, it will be 0.)

  HttpAuthCache::Entry* entry = session_->auth_cache()->LookupByPath(
      AuthOrigin(target), AuthPath(target));

  // We don't support preemptive authentication for connection-based
  // authentication schemes because they can't reuse entry->handler().
  // Hopefully we can remove this limitation in the future.
  if (entry && !entry->handler()->is_connection_based()) {
    auth_identity_[target].source = HttpAuth::IDENT_SRC_PATH_LOOKUP;
    auth_identity_[target].invalid = false;
    auth_identity_[target].username = entry->username();
    auth_identity_[target].password = entry->password();
    auth_handler_[target] = entry->handler();
    return true;
  }
  return false;
}

bool HttpNetworkTransaction::SelectNextAuthIdentityToTry(
    HttpAuth::Target target,
    const GURL& auth_origin) {
  DCHECK(auth_handler_[target]);
  DCHECK(auth_identity_[target].invalid);

  // Try to use the username/password encoded into the URL first.
  if (target == HttpAuth::AUTH_SERVER && request_->url.has_username() &&
      !embedded_identity_used_) {
    auth_identity_[target].source = HttpAuth::IDENT_SRC_URL;
    auth_identity_[target].invalid = false;
    // Extract the username:password from the URL.
    GetIdentityFromURL(request_->url,
                       &auth_identity_[target].username,
                       &auth_identity_[target].password);
    embedded_identity_used_ = true;
    // TODO(eroman): If the password is blank, should we also try combining
    // with a password from the cache?
    return true;
  }

  // Check the auth cache for a realm entry.
  HttpAuthCache::Entry* entry = session_->auth_cache()->LookupByRealm(
      auth_origin, auth_handler_[target]->realm());

  if (entry) {
    // Disallow re-using of identity if the scheme of the originating challenge
    // does not match. This protects against the following situation:
    // 1. Browser prompts user to sign into DIGEST realm="Foo".
    // 2. Since the auth-scheme is not BASIC, the user is reasured that it
    //    will not be sent over the wire in clear text. So they use their
    //    most trusted password.
    // 3. Next, the browser receives a challenge for BASIC realm="Foo". This
    //    is the same realm that we have a cached identity for. However if
    //    we use that identity, it would get sent over the wire in
    //    clear text (which isn't what the user agreed to when entering it).
    if (entry->handler()->scheme() != auth_handler_[target]->scheme()) {
      LOG(WARNING) << "The scheme of realm " << auth_handler_[target]->realm()
                   << " has changed from " << entry->handler()->scheme()
                   << " to " << auth_handler_[target]->scheme();
      return false;
    }

    auth_identity_[target].source = HttpAuth::IDENT_SRC_REALM_LOOKUP;
    auth_identity_[target].invalid = false;
    auth_identity_[target].username = entry->username();
    auth_identity_[target].password = entry->password();
    return true;
  }
  return false;
}

std::string HttpNetworkTransaction::AuthChallengeLogMessage() const {
  std::string msg;
  std::string header_val;
  void* iter = NULL;
  scoped_refptr<HttpResponseHeaders> headers = GetResponseHeaders();
  while (headers->EnumerateHeader(&iter, "proxy-authenticate", &header_val)) {
    msg.append("\n  Has header Proxy-Authenticate: ");
    msg.append(header_val);
  }

  iter = NULL;
  while (headers->EnumerateHeader(&iter, "www-authenticate", &header_val)) {
    msg.append("\n  Has header WWW-Authenticate: ");
    msg.append(header_val);
  }

  // RFC 4559 requires that a proxy indicate its support of NTLM/Negotiate
  // authentication with a "Proxy-Support: Session-Based-Authentication"
  // response header.
  iter = NULL;
  while (headers->EnumerateHeader(&iter, "proxy-support", &header_val)) {
    msg.append("\n  Has header Proxy-Support: ");
    msg.append(header_val);
  }

  return msg;
}

int HttpNetworkTransaction::HandleAuthChallenge() {
  scoped_refptr<HttpResponseHeaders> headers = GetResponseHeaders();
  DCHECK(headers);

  int status = headers->response_code();
  if (status != 401 && status != 407)
    return OK;
  HttpAuth::Target target = status == 407 ?
      HttpAuth::AUTH_PROXY : HttpAuth::AUTH_SERVER;
  GURL auth_origin = AuthOrigin(target);

  LOG(INFO) << "The " << AuthTargetString(target) << " "
            << auth_origin << " requested auth"
            << AuthChallengeLogMessage();

  if (target == HttpAuth::AUTH_PROXY && proxy_info_.is_direct())
    return ERR_UNEXPECTED_PROXY_AUTH;

  // The auth we tried just failed, hence it can't be valid. Remove it from
  // the cache so it won't be used again.
  // TODO(wtc): IsFinalRound is not the right condition.  In a multi-round
  // auth sequence, the server may fail the auth in round 1 if our first
  // authorization header is broken.  We should inspect response_.headers to
  // determine if the server already failed the auth or wants us to continue.
  // See http://crbug.com/21015.
  if (HaveAuth(target) && auth_handler_[target]->IsFinalRound()) {
    InvalidateRejectedAuthFromCache(target, auth_origin);
    auth_handler_[target] = NULL;
    auth_identity_[target] = HttpAuth::Identity();
  }

  auth_identity_[target].invalid = true;

  if (target != HttpAuth::AUTH_SERVER ||
      !(request_->load_flags & LOAD_DO_NOT_SEND_AUTH_DATA)) {
    // Find the best authentication challenge that we support.
    HttpAuth::ChooseBestChallenge(headers, target, auth_origin,
                                  &auth_handler_[target]);
  }

  if (!auth_handler_[target]) {
    if (establishing_tunnel_) {
      LOG(ERROR) << "Can't perform auth to the " << AuthTargetString(target)
                 << " " << auth_origin << " when establishing a tunnel"
                 << AuthChallengeLogMessage();

      // We are establishing a tunnel, we can't show the error page because an
      // active network attacker could control its contents.  Instead, we just
      // fail to establish the tunnel.
      DCHECK(target == HttpAuth::AUTH_PROXY);
      return ERR_PROXY_AUTH_REQUESTED;
    }
    // We found no supported challenge -- let the transaction continue
    // so we end up displaying the error page.
    return OK;
  }

  if (auth_handler_[target]->NeedsIdentity()) {
    // Pick a new auth identity to try, by looking to the URL and auth cache.
    // If an identity to try is found, it is saved to auth_identity_[target].
    SelectNextAuthIdentityToTry(target, auth_origin);
  } else {
    // Proceed with the existing identity or a null identity.
    //
    // TODO(wtc): Add a safeguard against infinite transaction restarts, if
    // the server keeps returning "NTLM".
    auth_identity_[target].invalid = false;
  }

  // Make a note that we are waiting for auth. This variable is inspected
  // when the client calls RestartWithAuth() to pick up where we left off.
  pending_auth_target_ = target;

  if (auth_identity_[target].invalid) {
    // We have exhausted all identity possibilities, all we can do now is
    // pass the challenge information back to the client.
    PopulateAuthChallenge(target, auth_origin);
  }
  return OK;
}

void HttpNetworkTransaction::PopulateAuthChallenge(HttpAuth::Target target,
                                                   const GURL& auth_origin) {
  // Populates response_.auth_challenge with the authentication challenge info.
  // This info is consumed by URLRequestHttpJob::GetAuthChallengeInfo().

  AuthChallengeInfo* auth_info = new AuthChallengeInfo;
  auth_info->is_proxy = target == HttpAuth::AUTH_PROXY;
  auth_info->host_and_port = ASCIIToWide(GetHostAndPort(auth_origin));
  auth_info->scheme = ASCIIToWide(auth_handler_[target]->scheme());
  // TODO(eroman): decode realm according to RFC 2047.
  auth_info->realm = ASCIIToWide(auth_handler_[target]->realm());
  response_.auth_challenge = auth_info;
}

}  // namespace net
