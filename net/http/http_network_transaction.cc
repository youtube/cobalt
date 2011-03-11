// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_transaction.h"

#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "build/build_config.h"
#include "googleurl/src/gurl.h"
#include "net/base/auth.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_delegate.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_chunked_decoder.h"
#include "net/http/http_net_log_params.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_util.h"
#include "net/http/url_security_manager.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"

using base::Time;

namespace net {

namespace {

void ProcessAlternateProtocol(HttpStreamFactory* factory,
                              HttpAlternateProtocols* alternate_protocols,
                              const HttpResponseHeaders& headers,
                              const HostPortPair& http_host_port_pair) {
  std::string alternate_protocol_str;

  if (!headers.EnumerateHeader(NULL, HttpAlternateProtocols::kHeader,
                               &alternate_protocol_str)) {
    // Header is not present.
    return;
  }

  factory->ProcessAlternateProtocol(alternate_protocols,
                                    alternate_protocol_str,
                                    http_host_port_pair);
}

// Returns true if |error| is a client certificate authentication error.
bool IsClientCertificateError(int error) {
  switch (error) {
    case ERR_BAD_SSL_CLIENT_AUTH_CERT:
    case ERR_SSL_CLIENT_AUTH_PRIVATE_KEY_ACCESS_DENIED:
    case ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY:
    case ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED:
      return true;
    default:
      return false;
  }
}

}  // namespace

//-----------------------------------------------------------------------------

HttpNetworkTransaction::HttpNetworkTransaction(HttpNetworkSession* session)
    : pending_auth_target_(HttpAuth::AUTH_NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &HttpNetworkTransaction::OnIOComplete)),
      user_callback_(NULL),
      session_(session),
      request_(NULL),
      headers_valid_(false),
      logged_response_time_(false),
      request_headers_(),
      read_buf_len_(0),
      next_state_(STATE_NONE),
      establishing_tunnel_(false) {
  session->ssl_config_service()->GetSSLConfig(&ssl_config_);
  if (session->http_stream_factory()->next_protos())
    ssl_config_.next_protos = *session->http_stream_factory()->next_protos();
}

HttpNetworkTransaction::~HttpNetworkTransaction() {
  if (stream_.get()) {
    HttpResponseHeaders* headers = GetResponseHeaders();
    // TODO(mbelshe): The stream_ should be able to compute whether or not the
    //                stream should be kept alive.  No reason to compute here
    //                and pass it in.
    bool try_to_keep_alive =
        next_state_ == STATE_NONE &&
        stream_->CanFindEndOfResponse() &&
        (!headers || headers->IsKeepAlive());
    if (!try_to_keep_alive) {
      stream_->Close(true /* not reusable */);
    } else {
      if (stream_->IsResponseBodyComplete()) {
        // If the response body is complete, we can just reuse the socket.
        stream_->Close(false /* reusable */);
      } else if (stream_->IsSpdyHttpStream()) {
        // Doesn't really matter for SpdyHttpStream. Just close it.
        stream_->Close(true /* not reusable */);
      } else {
        // Otherwise, we try to drain the response body.
        // TODO(willchan): Consider moving this response body draining to the
        // stream implementation.  For SPDY, there's clearly no point.  For
        // HTTP, it can vary depending on whether or not we're pipelining.  It's
        // stream dependent, so the different subtypes should be implementing
        // their solutions.
        HttpResponseBodyDrainer* drainer =
          new HttpResponseBodyDrainer(stream_.release());
        drainer->Start(session_);
        // |drainer| will delete itself.
      }
    }
  }
}

int HttpNetworkTransaction::Start(const HttpRequestInfo* request_info,
                                  CompletionCallback* callback,
                                  const BoundNetLog& net_log) {
  SIMPLE_STATS_COUNTER("HttpNetworkTransaction.Count");

  net_log_ = net_log;
  request_ = request_info;
  start_time_ = base::Time::Now();

  next_state_ = STATE_CREATE_STREAM;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartIgnoringLastError(
    CompletionCallback* callback) {
  DCHECK(!stream_.get());
  DCHECK(!stream_request_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  next_state_ = STATE_CREATE_STREAM;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartWithCertificate(
    X509Certificate* client_cert,
    CompletionCallback* callback) {
  // In HandleCertificateRequest(), we always tear down existing stream
  // requests to force a new connection.  So we shouldn't have one here.
  DCHECK(!stream_request_.get());
  DCHECK(!stream_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  ssl_config_.client_cert = client_cert;
  session_->ssl_client_auth_cache()->Add(
      response_.cert_request_info->host_and_port, client_cert);
  ssl_config_.send_client_cert = true;
  // Reset the other member variables.
  // Note: this is necessary only with SSL renegotiation.
  ResetStateForRestart();
  next_state_ = STATE_CREATE_STREAM;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartWithAuth(
    const string16& username,
    const string16& password,
    CompletionCallback* callback) {
  HttpAuth::Target target = pending_auth_target_;
  if (target == HttpAuth::AUTH_NONE) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }
  pending_auth_target_ = HttpAuth::AUTH_NONE;

  auth_controllers_[target]->ResetAuth(username, password);

  DCHECK(user_callback_ == NULL);

  int rv = OK;
  if (target == HttpAuth::AUTH_PROXY && establishing_tunnel_) {
    // In this case, we've gathered credentials for use with proxy
    // authentication of a tunnel.
    DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
    DCHECK(stream_request_ != NULL);
    auth_controllers_[target] = NULL;
    ResetStateForRestart();
    rv = stream_request_->RestartTunnelWithProxyAuth(username, password);
  } else {
    // In this case, we've gathered credentials for the server or the proxy
    // but it is not during the tunneling phase.
    DCHECK(stream_request_ == NULL);
    PrepareForAuthRestart(target);
    rv = DoLoop(OK);
  }

  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

void HttpNetworkTransaction::PrepareForAuthRestart(HttpAuth::Target target) {
  DCHECK(HaveAuth(target));
  DCHECK(!stream_request_.get());

  bool keep_alive = false;
  // Even if the server says the connection is keep-alive, we have to be
  // able to find the end of each response in order to reuse the connection.
  if (GetResponseHeaders()->IsKeepAlive() &&
      stream_->CanFindEndOfResponse()) {
    // If the response body hasn't been completely read, we need to drain
    // it first.
    if (!stream_->IsResponseBodyComplete()) {
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
  DCHECK(!stream_request_.get());

  if (stream_.get()) {
    HttpStream* new_stream = NULL;
    if (keep_alive) {
      // We should call connection_->set_idle_time(), but this doesn't occur
      // often enough to be worth the trouble.
      stream_->SetConnectionReused();
      new_stream = stream_->RenewStreamForAuth();
    }

    if (!new_stream) {
      stream_->Close(!keep_alive);
      next_state_ = STATE_CREATE_STREAM;
    } else {
      next_state_ = STATE_INIT_STREAM;
    }
    stream_.reset(new_stream);
  }

  // Reset the other member variables.
  ResetStateForAuthRestart();
}

bool HttpNetworkTransaction::IsReadyToRestartForAuth() {
  return pending_auth_target_ != HttpAuth::AUTH_NONE &&
      HaveAuth(pending_auth_target_);
}

int HttpNetworkTransaction::Read(IOBuffer* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK_LT(0, buf_len);

  State next_state = STATE_NONE;

  scoped_refptr<HttpResponseHeaders> headers(GetResponseHeaders());
  if (headers_valid_ && headers.get() && stream_request_.get()) {
    // We're trying to read the body of the response but we're still trying
    // to establish an SSL tunnel through an HTTP proxy.  We can't read these
    // bytes when establishing a tunnel because they might be controlled by
    // an active network attacker.  We don't worry about this for HTTP
    // because an active network attacker can already control HTTP sessions.
    // We reach this case when the user cancels a 407 proxy auth prompt.  We
    // also don't worry about this for an HTTPS Proxy, because the
    // communication with the proxy is secure.
    // See http://crbug.com/8473.
    DCHECK(proxy_info_.is_http() || proxy_info_.is_https());
    DCHECK_EQ(headers->response_code(), 407);
    LOG(WARNING) << "Blocked proxy response with status "
                 << headers->response_code() << " to CONNECT request for "
                 << GetHostAndPort(request_->url) << ".";
    return ERR_TUNNEL_CONNECTION_FAILED;
  }

  // Are we using SPDY or HTTP?
  next_state = STATE_READ_BODY;

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
    case STATE_CREATE_STREAM_COMPLETE:
      return stream_request_->GetLoadState();
    case STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE:
    case STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE:
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
  if (!stream_.get())
    return 0;

  return stream_->GetUploadProgress();
}

void HttpNetworkTransaction::OnStreamReady(const SSLConfig& used_ssl_config,
                                           const ProxyInfo& used_proxy_info,
                                           HttpStream* stream) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK(stream_request_.get());

  stream_.reset(stream);
  ssl_config_ = used_ssl_config;
  proxy_info_ = used_proxy_info;
  response_.was_alternate_protocol_available =
      stream_request_->was_alternate_protocol_available();
  response_.was_npn_negotiated = stream_request_->was_npn_negotiated();
  response_.was_fetched_via_spdy = stream_request_->using_spdy();
  response_.was_fetched_via_proxy = !proxy_info_.is_direct();

  OnIOComplete(OK);
}

void HttpNetworkTransaction::OnStreamFailed(int result,
                                            const SSLConfig& used_ssl_config) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK_NE(OK, result);
  DCHECK(stream_request_.get());
  DCHECK(!stream_.get());
  ssl_config_ = used_ssl_config;

  OnIOComplete(result);
}

void HttpNetworkTransaction::OnCertificateError(
    int result,
    const SSLConfig& used_ssl_config,
    const SSLInfo& ssl_info) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK_NE(OK, result);
  DCHECK(stream_request_.get());
  DCHECK(!stream_.get());

  response_.ssl_info = ssl_info;
  ssl_config_ = used_ssl_config;

  // TODO(mbelshe):  For now, we're going to pass the error through, and that
  // will close the stream_request in all cases.  This means that we're always
  // going to restart an entire STATE_CREATE_STREAM, even if the connection is
  // good and the user chooses to ignore the error.  This is not ideal, but not
  // the end of the world either.

  OnIOComplete(result);
}

void HttpNetworkTransaction::OnNeedsProxyAuth(
    const HttpResponseInfo& proxy_response,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpAuthController* auth_controller) {
  DCHECK(stream_request_.get());
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  establishing_tunnel_ = true;
  response_.headers = proxy_response.headers;
  response_.auth_challenge = proxy_response.auth_challenge;
  headers_valid_ = true;
  ssl_config_ = used_ssl_config;
  proxy_info_ = used_proxy_info;

  auth_controllers_[HttpAuth::AUTH_PROXY] = auth_controller;
  pending_auth_target_ = HttpAuth::AUTH_PROXY;

  DoCallback(OK);
}

void HttpNetworkTransaction::OnNeedsClientAuth(
    const SSLConfig& used_ssl_config,
    SSLCertRequestInfo* cert_info) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  ssl_config_ = used_ssl_config;
  response_.cert_request_info = cert_info;
  OnIOComplete(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void HttpNetworkTransaction::OnHttpsProxyTunnelResponse(
    const HttpResponseInfo& response_info,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpStream* stream) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  headers_valid_ = true;
  response_ = response_info;
  ssl_config_ = used_ssl_config;
  proxy_info_ = used_proxy_info;
  stream_.reset(stream);
  stream_request_.reset();  // we're done with the stream request
  OnIOComplete(ERR_HTTPS_PROXY_TUNNEL_RESPONSE);
}

bool HttpNetworkTransaction::is_https_request() const {
  return request_->url.SchemeIs("https");
}

void HttpNetworkTransaction::DoCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
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
      case STATE_CREATE_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoCreateStream();
        break;
      case STATE_CREATE_STREAM_COMPLETE:
        rv = DoCreateStreamComplete(rv);
        break;
      case STATE_INIT_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoInitStream();
        break;
      case STATE_INIT_STREAM_COMPLETE:
        rv = DoInitStreamComplete(rv);
        break;
      case STATE_GENERATE_PROXY_AUTH_TOKEN:
        DCHECK_EQ(OK, rv);
        rv = DoGenerateProxyAuthToken();
        break;
      case STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE:
        rv = DoGenerateProxyAuthTokenComplete(rv);
        break;
      case STATE_GENERATE_SERVER_AUTH_TOKEN:
        DCHECK_EQ(OK, rv);
        rv = DoGenerateServerAuthToken();
        break;
      case STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE:
        rv = DoGenerateServerAuthTokenComplete(rv);
        break;
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST, NULL);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST, rv);
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS, NULL);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS, rv);
        break;
      case STATE_READ_BODY:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_READ_BODY, NULL);
        rv = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        rv = DoReadBodyComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_HTTP_TRANSACTION_READ_BODY, rv);
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(
            NetLog::TYPE_HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART, NULL);
        rv = DoDrainBodyForAuthRestart();
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE:
        rv = DoDrainBodyForAuthRestartComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART, rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int HttpNetworkTransaction::DoCreateStream() {
  next_state_ = STATE_CREATE_STREAM_COMPLETE;

  stream_request_.reset(
      session_->http_stream_factory()->RequestStream(
          *request_,
          ssl_config_,
          this,
          net_log_));
  DCHECK(stream_request_.get());
  return ERR_IO_PENDING;
}

int HttpNetworkTransaction::DoCreateStreamComplete(int result) {
  if (result == OK) {
    next_state_ = STATE_INIT_STREAM;
    DCHECK(stream_.get());
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    result = HandleCertificateRequest(result);
  } else if (result == ERR_HTTPS_PROXY_TUNNEL_RESPONSE) {
    // Return OK and let the caller read the proxy's error page
    next_state_ = STATE_NONE;
    return OK;
  }

  // Handle possible handshake errors that may have occurred if the stream
  // used SSL for one or more of the layers.
  result = HandleSSLHandshakeError(result);

  // At this point we are done with the stream_request_.
  stream_request_.reset();
  return result;
}

int HttpNetworkTransaction::DoInitStream() {
  DCHECK(stream_.get());
  next_state_ = STATE_INIT_STREAM_COMPLETE;
  return stream_->InitializeStream(request_, net_log_, &io_callback_);
}

int HttpNetworkTransaction::DoInitStreamComplete(int result) {
  if (result == OK) {
    next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN;
  } else {
    if (result < 0)
      result = HandleIOError(result);

    // The stream initialization failed, so this stream will never be useful.
    stream_.reset();
  }

  return result;
}

int HttpNetworkTransaction::DoGenerateProxyAuthToken() {
  next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE;
  if (!ShouldApplyProxyAuth())
    return OK;
  HttpAuth::Target target = HttpAuth::AUTH_PROXY;
  if (!auth_controllers_[target].get())
    auth_controllers_[target] =
        new HttpAuthController(target,
                               AuthURL(target),
                               session_->http_auth_cache(),
                               session_->http_auth_handler_factory());
  return auth_controllers_[target]->MaybeGenerateAuthToken(request_,
                                                           &io_callback_,
                                                           net_log_);
}

int HttpNetworkTransaction::DoGenerateProxyAuthTokenComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv == OK)
    next_state_ = STATE_GENERATE_SERVER_AUTH_TOKEN;
  return rv;
}

int HttpNetworkTransaction::DoGenerateServerAuthToken() {
  next_state_ = STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE;
  HttpAuth::Target target = HttpAuth::AUTH_SERVER;
  if (!auth_controllers_[target].get())
    auth_controllers_[target] =
        new HttpAuthController(target,
                               AuthURL(target),
                               session_->http_auth_cache(),
                               session_->http_auth_handler_factory());
  if (!ShouldApplyServerAuth())
    return OK;
  return auth_controllers_[target]->MaybeGenerateAuthToken(request_,
                                                           &io_callback_,
                                                           net_log_);
}

int HttpNetworkTransaction::DoGenerateServerAuthTokenComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv == OK)
    next_state_ = STATE_SEND_REQUEST;
  return rv;
}

int HttpNetworkTransaction::DoSendRequest() {
  next_state_ = STATE_SEND_REQUEST_COMPLETE;

  UploadDataStream* request_body = NULL;
  if (request_->upload_data) {
    int error_code;
    request_body = UploadDataStream::Create(request_->upload_data, &error_code);
    if (!request_body)
      return error_code;
  }

  // This is constructed lazily (instead of within our Start method), so that
  // we have proxy info available.
  if (request_headers_.IsEmpty()) {
    bool using_proxy = (proxy_info_.is_http()|| proxy_info_.is_https()) &&
                        !is_https_request();
    HttpUtil::BuildRequestHeaders(request_, request_body, auth_controllers_,
                                  ShouldApplyServerAuth(),
                                  ShouldApplyProxyAuth(), using_proxy,
                                  &request_headers_);

    if (session_->network_delegate())
      session_->network_delegate()->NotifySendHttpRequest(&request_headers_);
  }

  headers_valid_ = false;
  return stream_->SendRequest(request_headers_, request_body, &response_,
                              &io_callback_);
}

int HttpNetworkTransaction::DoSendRequestComplete(int result) {
  if (result < 0)
    return HandleIOError(result);
  next_state_ = STATE_READ_HEADERS;
  return OK;
}

int HttpNetworkTransaction::DoReadHeaders() {
  next_state_ = STATE_READ_HEADERS_COMPLETE;
  return stream_->ReadResponseHeaders(&io_callback_);
}

int HttpNetworkTransaction::HandleConnectionClosedBeforeEndOfHeaders() {
  if (!response_.headers && !stream_->IsConnectionReused()) {
    // The connection was closed before any data was sent. Likely an error
    // rather than empty HTTP/0.9 response.
    return ERR_EMPTY_RESPONSE;
  }

  return OK;
}

int HttpNetworkTransaction::DoReadHeadersComplete(int result) {
  // We can get a certificate error or ERR_SSL_CLIENT_AUTH_CERT_NEEDED here
  // due to SSL renegotiation.
  if (IsCertificateError(result)) {
    // We don't handle a certificate error during SSL renegotiation, so we
    // have to return an error that's not in the certificate error range
    // (-2xx).
    LOG(ERROR) << "Got a server certificate with error " << result
               << " during SSL renegotiation";
    result = ERR_CERT_ERROR_IN_SSL_RENEGOTIATION;
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    // TODO(wtc): Need a test case for this code path!
    DCHECK(stream_.get());
    DCHECK(is_https_request());
    response_.cert_request_info = new SSLCertRequestInfo;
    stream_->GetSSLCertRequestInfo(response_.cert_request_info);
    result = HandleCertificateRequest(result);
    if (result == OK)
      return result;
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
  if (result == OK)
    LogTransactionConnectedMetrics();

  if (result == ERR_CONNECTION_CLOSED) {
    // For now, if we get at least some data, we do the best we can to make
    // sense of it and send it back up the stack.
    int rv = HandleConnectionClosedBeforeEndOfHeaders();
    if (rv != OK)
      return rv;
  }

  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        NetLog::TYPE_HTTP_TRANSACTION_READ_RESPONSE_HEADERS,
        make_scoped_refptr(new NetLogHttpResponseParameter(response_.headers)));
  }

  if (response_.headers->GetParsedHttpVersion() < HttpVersion(1, 0)) {
    // HTTP/0.9 doesn't support the PUT method, so lack of response headers
    // indicates a buggy server.  See:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=193921
    if (request_->method == "PUT")
      return ERR_METHOD_NOT_SUPPORTED;
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

  HostPortPair endpoint = HostPortPair(request_->url.HostNoBrackets(),
                                       request_->url.EffectiveIntPort());
  ProcessAlternateProtocol(session_->http_stream_factory(),
                           session_->mutable_alternate_protocols(),
                           *response_.headers,
                           endpoint);

  int rv = HandleAuthChallenge();
  if (rv != OK)
    return rv;

  if (is_https_request())
    stream_->GetSSLInfo(&response_.ssl_info);

  headers_valid_ = true;
  return OK;
}

int HttpNetworkTransaction::DoReadBody() {
  DCHECK(read_buf_);
  DCHECK_GT(read_buf_len_, 0);
  DCHECK(stream_ != NULL);

  next_state_ = STATE_READ_BODY_COMPLETE;
  return stream_->ReadResponseBody(read_buf_, read_buf_len_, &io_callback_);
}

int HttpNetworkTransaction::DoReadBodyComplete(int result) {
  // We are done with the Read call.
  bool done = false;
  if (result <= 0) {
    DCHECK_NE(ERR_IO_PENDING, result);
    done = true;
  }

  bool keep_alive = false;
  if (stream_->IsResponseBodyComplete()) {
    // Note: Just because IsResponseBodyComplete is true, we're not
    // necessarily "done".  We're only "done" when it is the last
    // read on this HttpNetworkTransaction, which will be signified
    // by a zero-length read.
    // TODO(mbelshe): The keepalive property is really a property of
    //    the stream.  No need to compute it here just to pass back
    //    to the stream's Close function.
    if (stream_->CanFindEndOfResponse())
      keep_alive = GetResponseHeaders()->IsKeepAlive();
  }

  // Clean up connection if we are done.
  if (done) {
    LogTransactionMetrics();
    stream_->Close(!keep_alive);
    // Note: we don't reset the stream here.  We've closed it, but we still
    // need it around so that callers can call methods such as
    // GetUploadProgress() and have them be meaningful.
    // TODO(mbelshe): This means we closed the stream here, and we close it
    // again in ~HttpNetworkTransaction.  Clean that up.

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
  } else if (stream_->IsResponseBodyComplete()) {
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

void HttpNetworkTransaction::LogTransactionConnectedMetrics() {
  if (logged_response_time_)
    return;

  logged_response_time_ = true;

  base::TimeDelta total_duration = response_.response_time - start_time_;

  UMA_HISTOGRAM_CLIPPED_TIMES(
      "Net.Transaction_Connected_Under_10",
      total_duration,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
      100);

  bool reused_socket = stream_->IsConnectionReused();
  if (!reused_socket) {
    UMA_HISTOGRAM_CLIPPED_TIMES(
        "Net.Transaction_Connected_New",
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);

  static bool use_conn_impact_histogram(
      base::FieldTrialList::Find("ConnCountImpact") &&
      !base::FieldTrialList::Find("ConnCountImpact")->group_name().empty());
  if (use_conn_impact_histogram) {
    UMA_HISTOGRAM_CLIPPED_TIMES(
        base::FieldTrial::MakeName("Net.Transaction_Connected_New",
            "ConnCountImpact"),
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
    }
  }

  static bool use_spdy_histogram(base::FieldTrialList::Find("SpdyImpact") &&
      !base::FieldTrialList::Find("SpdyImpact")->group_name().empty());
  if (use_spdy_histogram && response_.was_npn_negotiated) {
    UMA_HISTOGRAM_CLIPPED_TIMES(
      base::FieldTrial::MakeName("Net.Transaction_Connected_Under_10",
                                 "SpdyImpact"),
        total_duration, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10), 100);

    if (!reused_socket) {
      UMA_HISTOGRAM_CLIPPED_TIMES(
          base::FieldTrial::MakeName("Net.Transaction_Connected_New",
                                     "SpdyImpact"),
          total_duration, base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMinutes(10), 100);
    }
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
                              base::TimeDelta::FromMilliseconds(1),
                              base::TimeDelta::FromMinutes(10),
                              100);
  UMA_HISTOGRAM_CLIPPED_TIMES("Net.Transaction_Latency_Total_Under_10",
                              total_duration,
                              base::TimeDelta::FromMilliseconds(1),
                              base::TimeDelta::FromMinutes(10), 100);
  if (!stream_->IsConnectionReused()) {
    UMA_HISTOGRAM_CLIPPED_TIMES(
        "Net.Transaction_Latency_Total_New_Connection_Under_10",
        total_duration, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10), 100);
  }
}

int HttpNetworkTransaction::HandleCertificateRequest(int error) {
  // There are two paths through which the server can request a certificate
  // from us.  The first is during the initial handshake, the second is
  // during SSL renegotiation.
  //
  // In both cases, we want to close the connection before proceeding.
  // We do this for two reasons:
  //   First, we don't want to keep the connection to the server hung for a
  //   long time while the user selects a certificate.
  //   Second, even if we did keep the connection open, NSS has a bug where
  //   restarting the handshake for ClientAuth is currently broken.
  DCHECK_EQ(error, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);

  if (stream_.get()) {
    // Since we already have a stream, we're being called as part of SSL
    // renegotiation.
    DCHECK(!stream_request_.get());
    stream_->Close(true);
    stream_.reset();
  }

  // The server is asking for a client certificate during the initial
  // handshake.
  stream_request_.reset();

  // If the user selected one of the certificates in client_certs or declined
  // to provide one for this server before, use the past decision
  // automatically.
  scoped_refptr<X509Certificate> client_cert;
  bool found_cached_cert = session_->ssl_client_auth_cache()->Lookup(
      response_.cert_request_info->host_and_port, &client_cert);
  if (!found_cached_cert)
    return error;

  // Check that the certificate selected is still a certificate the server
  // is likely to accept, based on the criteria supplied in the
  // CertificateRequest message.
  if (client_cert) {
    const std::vector<scoped_refptr<X509Certificate> >& client_certs =
        response_.cert_request_info->client_certs;
    bool cert_still_valid = false;
    for (size_t i = 0; i < client_certs.size(); ++i) {
      if (client_cert->Equals(client_certs[i])) {
        cert_still_valid = true;
        break;
      }
    }

    if (!cert_still_valid)
      return error;
  }

  // TODO(davidben): Add a unit test which covers this path; we need to be
  // able to send a legitimate certificate and also bypass/clear the
  // SSL session cache.
  ssl_config_.client_cert = client_cert;
  ssl_config_.send_client_cert = true;
  next_state_ = STATE_CREATE_STREAM;
  // Reset the other member variables.
  // Note: this is necessary only with SSL renegotiation.
  ResetStateForRestart();
  return OK;
}

// TODO(rch): This does not correctly handle errors when an SSL proxy is
// being used, as all of the errors are handled as if they were generated
// by the endpoint host, request_->url, rather than considering if they were
// generated by the SSL proxy. http://crbug.com/69329
int HttpNetworkTransaction::HandleSSLHandshakeError(int error) {
  DCHECK(request_);
  if (ssl_config_.send_client_cert &&
      (error == ERR_SSL_PROTOCOL_ERROR || IsClientCertificateError(error))) {
    session_->ssl_client_auth_cache()->Remove(
        GetHostAndPort(request_->url));
  }

  switch (error) {
    case ERR_SSL_PROTOCOL_ERROR:
    case ERR_SSL_VERSION_OR_CIPHER_MISMATCH:
    case ERR_SSL_DECOMPRESSION_FAILURE_ALERT:
    case ERR_SSL_BAD_RECORD_MAC_ALERT:
      if (ssl_config_.tls1_enabled &&
          !SSLConfigService::IsKnownStrictTLSServer(request_->url.host())) {
        // This could be a TLS-intolerant server, an SSL 3.0 server that
        // chose a TLS-only cipher suite or a server with buggy DEFLATE
        // support. Turn off TLS 1.0, DEFLATE support and retry.
        session_->http_stream_factory()->AddTLSIntolerantServer(request_->url);
        ResetConnectionAndRequestForResend();
        error = OK;
      }
      break;
    case ERR_SSL_SNAP_START_NPN_MISPREDICTION:
      // This means that we tried to Snap Start a connection, but we
      // mispredicted the NPN result. This isn't a problem from the point of
      // view of the SSL layer because the server will ignore the application
      // data in the Snap Start extension. However, at the HTTP layer, we have
      // already decided that it's a HTTP or SPDY connection and it's easier to
      // abort and start again.
      ResetConnectionAndRequestForResend();
      error = OK;
      break;
  }
  return error;
}

// This method determines whether it is safe to resend the request after an
// IO error.  It can only be called in response to request header or body
// write errors or response header read errors.  It should not be used in
// other cases, such as a Connect error.
int HttpNetworkTransaction::HandleIOError(int error) {
  // SSL errors may happen at any time during the stream and indicate issues
  // with the underlying connection. Because the peer may request
  // renegotiation at any time, check and handle any possible SSL handshake
  // related errors. In addition to renegotiation, TLS False/Snap Start may
  // cause SSL handshake errors to be delayed until the first or second Write
  // (Snap Start) or the first Read (False & Snap Start) on the underlying
  // connection.
  error = HandleSSLHandshakeError(error);

  switch (error) {
    // If we try to reuse a connection that the server is in the process of
    // closing, we may end up successfully writing out our request (or a
    // portion of our request) only to find a connection error when we try to
    // read from (or finish writing to) the socket.
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_CLOSED:
    case ERR_CONNECTION_ABORTED:
      if (ShouldResendRequest(error)) {
        ResetConnectionAndRequestForResend();
        error = OK;
      }
      break;
  }
  return error;
}

void HttpNetworkTransaction::ResetStateForRestart() {
  ResetStateForAuthRestart();
  stream_.reset();
}

void HttpNetworkTransaction::ResetStateForAuthRestart() {
  pending_auth_target_ = HttpAuth::AUTH_NONE;
  read_buf_ = NULL;
  read_buf_len_ = 0;
  headers_valid_ = false;
  request_headers_.Clear();
  response_ = HttpResponseInfo();
  establishing_tunnel_ = false;
}

HttpResponseHeaders* HttpNetworkTransaction::GetResponseHeaders() const {
  return response_.headers;
}

bool HttpNetworkTransaction::ShouldResendRequest(int error) const {
  bool connection_is_proven = stream_->IsConnectionReused();
  bool has_received_headers = GetResponseHeaders() != NULL;

  // NOTE: we resend a request only if we reused a keep-alive connection.
  // This automatically prevents an infinite resend loop because we'll run
  // out of the cached keep-alive connections eventually.
  if (connection_is_proven && !has_received_headers)
    return true;
  return false;
}

void HttpNetworkTransaction::ResetConnectionAndRequestForResend() {
  if (stream_.get()) {
    stream_->Close(true);
    stream_.reset();
  }

  // We need to clear request_headers_ because it contains the real request
  // headers, but we may need to resend the CONNECT request first to recreate
  // the SSL tunnel.
  request_headers_.Clear();
  next_state_ = STATE_CREATE_STREAM;  // Resend the request.
}

bool HttpNetworkTransaction::ShouldApplyProxyAuth() const {
  return !is_https_request() &&
      (proxy_info_.is_https() || proxy_info_.is_http());
}

bool HttpNetworkTransaction::ShouldApplyServerAuth() const {
  return !(request_->load_flags & LOAD_DO_NOT_SEND_AUTH_DATA);
}

int HttpNetworkTransaction::HandleAuthChallenge() {
  scoped_refptr<HttpResponseHeaders> headers(GetResponseHeaders());
  DCHECK(headers);

  int status = headers->response_code();
  if (status != 401 && status != 407)
    return OK;
  HttpAuth::Target target = status == 407 ?
                            HttpAuth::AUTH_PROXY : HttpAuth::AUTH_SERVER;
  if (target == HttpAuth::AUTH_PROXY && proxy_info_.is_direct())
    return ERR_UNEXPECTED_PROXY_AUTH;

  // This case can trigger when an HTTPS server responds with a 407 status
  // code through a non-authenticating proxy.
  if (!auth_controllers_[target].get())
    return ERR_UNEXPECTED_PROXY_AUTH;

  int rv = auth_controllers_[target]->HandleAuthChallenge(
      headers, (request_->load_flags & LOAD_DO_NOT_SEND_AUTH_DATA) != 0, false,
      net_log_);
  if (auth_controllers_[target]->HaveAuthHandler())
      pending_auth_target_ = target;

  scoped_refptr<AuthChallengeInfo> auth_info =
      auth_controllers_[target]->auth_info();
  if (auth_info.get())
      response_.auth_challenge = auth_info;

  return rv;
}

bool HttpNetworkTransaction::HaveAuth(HttpAuth::Target target) const {
  return auth_controllers_[target].get() &&
      auth_controllers_[target]->HaveAuth();
}


GURL HttpNetworkTransaction::AuthURL(HttpAuth::Target target) const {
  switch (target) {
    case HttpAuth::AUTH_PROXY: {
      if (!proxy_info_.proxy_server().is_valid() ||
          proxy_info_.proxy_server().is_direct()) {
        return GURL();  // There is no proxy server.
      }
      const char* scheme = proxy_info_.is_https() ? "https://" : "http://";
      return GURL(scheme +
                  proxy_info_.proxy_server().host_port_pair().ToString());
    }
    case HttpAuth::AUTH_SERVER:
      return request_->url;
    default:
     return GURL();
  }
}

#define STATE_CASE(s) \
  case s: \
    description = base::StringPrintf("%s (0x%08X)", #s, s); \
    break

std::string HttpNetworkTransaction::DescribeState(State state) {
  std::string description;
  switch (state) {
    STATE_CASE(STATE_CREATE_STREAM);
    STATE_CASE(STATE_CREATE_STREAM_COMPLETE);
    STATE_CASE(STATE_SEND_REQUEST);
    STATE_CASE(STATE_SEND_REQUEST_COMPLETE);
    STATE_CASE(STATE_READ_HEADERS);
    STATE_CASE(STATE_READ_HEADERS_COMPLETE);
    STATE_CASE(STATE_READ_BODY);
    STATE_CASE(STATE_READ_BODY_COMPLETE);
    STATE_CASE(STATE_DRAIN_BODY_FOR_AUTH_RESTART);
    STATE_CASE(STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE);
    STATE_CASE(STATE_NONE);
    default:
      description = base::StringPrintf("Unknown state 0x%08X (%u)", state,
                                       state);
      break;
  }
  return description;
}

#undef STATE_CASE

}  // namespace net
