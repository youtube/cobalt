// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_transaction.h"

#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/field_trial.h"
#include "base/format_macros.h"
#include "base/histogram.h"
#include "base/scoped_ptr.h"
#include "base/stats_counters.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/string_number_conversions.h"
#include "build/build_config.h"
#include "googleurl/src/gurl.h"
#include "net/base/auth.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
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
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_stream_handle.h"
#include "net/http/http_stream_request.h"
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

void BuildRequestHeaders(const HttpRequestInfo* request_info,
                         const HttpRequestHeaders& authorization_headers,
                         const UploadDataStream* upload_data_stream,
                         bool using_proxy,
                         std::string* request_line,
                         HttpRequestHeaders* request_headers) {
  const std::string path = using_proxy ?
                           HttpUtil::SpecForRequest(request_info->url) :
                           HttpUtil::PathForRequest(request_info->url);
  *request_line = StringPrintf(
      "%s %s HTTP/1.1\r\n", request_info->method.c_str(), path.c_str());
  request_headers->SetHeader(HttpRequestHeaders::kHost,
                             GetHostAndOptionalPort(request_info->url));

  // For compat with HTTP/1.0 servers and proxies:
  if (using_proxy) {
    request_headers->SetHeader(HttpRequestHeaders::kProxyConnection,
                               "keep-alive");
  } else {
    request_headers->SetHeader(HttpRequestHeaders::kConnection, "keep-alive");
  }

  // Our consumer should have made sure that this is a safe referrer.  See for
  // instance WebCore::FrameLoader::HideReferrer.
  if (request_info->referrer.is_valid()) {
    request_headers->SetHeader(HttpRequestHeaders::kReferer,
                               request_info->referrer.spec());
  }

  // Add a content length header?
  if (upload_data_stream) {
    request_headers->SetHeader(
        HttpRequestHeaders::kContentLength,
        base::Uint64ToString(upload_data_stream->size()));
  } else if (request_info->method == "POST" || request_info->method == "PUT" ||
             request_info->method == "HEAD") {
    // An empty POST/PUT request still needs a content length.  As for HEAD,
    // IE and Safari also add a content length header.  Presumably it is to
    // support sending a HEAD request to an URL that only expects to be sent a
    // POST or some other method that normally would have a message body.
    request_headers->SetHeader(HttpRequestHeaders::kContentLength, "0");
  }

  // Honor load flags that impact proxy caches.
  if (request_info->load_flags & LOAD_BYPASS_CACHE) {
    request_headers->SetHeader(HttpRequestHeaders::kPragma, "no-cache");
    request_headers->SetHeader(HttpRequestHeaders::kCacheControl, "no-cache");
  } else if (request_info->load_flags & LOAD_VALIDATE_CACHE) {
    request_headers->SetHeader(HttpRequestHeaders::kCacheControl, "max-age=0");
  }

  request_headers->MergeFrom(authorization_headers);

  // Headers that will be stripped from request_info->extra_headers to prevent,
  // e.g., plugins from overriding headers that are controlled using other
  // means. Otherwise a plugin could set a referrer although sending the
  // referrer is inhibited.
  // TODO(jochen): check whether also other headers should be stripped.
  static const char* const kExtraHeadersToBeStripped[] = {
    "Referer"
  };

  HttpRequestHeaders stripped_extra_headers;
  stripped_extra_headers.CopyFrom(request_info->extra_headers);
  for (size_t i = 0; i < arraysize(kExtraHeadersToBeStripped); ++i)
    stripped_extra_headers.RemoveHeader(kExtraHeadersToBeStripped[i]);
  request_headers->MergeFrom(stripped_extra_headers);
}

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
      } else {
        // Otherwise, we try to drain the response body.
        // TODO(willchan): Consider moving this response body draining to the
        // stream implementation.  For SPDY, there's clearly no point.  For
        // HTTP, it can vary depending on whether or not we're pipelining.  It's
        // stream dependent, so the different subtypes should be implementing
        // their solutions.
        HttpUtil::DrainStreamBodyAndClose(stream_.release());
      }
    }
  }

  if (stream_request_.get()) {
    stream_request_->Cancel();
    stream_request_ = NULL;
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
  if (client_cert) {
    session_->ssl_client_auth_cache()->Add(GetHostAndPort(request_->url),
                                           client_cert);
  }
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
    if (keep_alive) {
      // We should call connection_->set_idle_time(), but this doesn't occur
      // often enough to be worth the trouble.
      stream_->SetConnectionReused();
    }
    stream_->Close(!keep_alive);
    next_state_ = STATE_CREATE_STREAM;
  }

  // Reset the other member variables.
  ResetStateForRestart();
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

  scoped_refptr<HttpResponseHeaders> headers = GetResponseHeaders();
  if (headers_valid_ && headers.get() && stream_request_.get()) {
    // We're trying to read the body of the response but we're still trying
    // to establish an SSL tunnel through the proxy.  We can't read these
    // bytes when establishing a tunnel because they might be controlled by
    // an active network attacker.  We don't worry about this for HTTP
    // because an active network attacker can already control HTTP sessions.
    // We reach this case when the user cancels a 407 proxy auth prompt.
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
  DCHECK(stream_->GetResponseInfo()->headers);

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

void HttpNetworkTransaction::OnStreamReady(HttpStreamHandle* stream) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK(stream_request_.get());

  stream_.reset(stream);
  response_.was_alternate_protocol_available =
      stream_request_->was_alternate_protocol_available();
  response_.was_npn_negotiated = stream_request_->was_npn_negotiated();
  response_.was_fetched_via_spdy = stream_request_->using_spdy();
  response_.was_fetched_via_proxy = !proxy_info_.is_direct();

  OnIOComplete(OK);
}

void HttpNetworkTransaction::OnStreamFailed(int result) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK_NE(OK, result);
  DCHECK(stream_request_.get());
  DCHECK(!stream_.get());

  OnIOComplete(result);
}

void HttpNetworkTransaction::OnCertificateError(int result,
                                                const SSLInfo& ssl_info) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK_NE(OK, result);
  DCHECK(stream_request_.get());
  DCHECK(!stream_.get());

  response_.ssl_info = ssl_info;

  // TODO(mbelshe):  For now, we're going to pass the error through, and that
  // will close the stream_request in all cases.  This means that we're always
  // going to restart an entire STATE_CREATE_STREAM, even if the connection is
  // good and the user chooses to ignore the error.  This is not ideal, but not
  // the end of the world either.

  OnIOComplete(result);
}

void HttpNetworkTransaction::OnNeedsProxyAuth(
    const HttpResponseInfo& proxy_response,
    HttpAuthController* auth_controller) {
  DCHECK(stream_request_.get());
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  establishing_tunnel_ = true;
  response_.headers = proxy_response.headers;
  response_.auth_challenge = proxy_response.auth_challenge;
  headers_valid_ = true;

  auth_controllers_[HttpAuth::AUTH_PROXY] = auth_controller;
  pending_auth_target_ = HttpAuth::AUTH_PROXY;

  DoCallback(OK);
}

void HttpNetworkTransaction::OnNeedsClientAuth(
    SSLCertRequestInfo* cert_info) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  response_.cert_request_info = cert_info;
  OnIOComplete(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
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
        net_log_.EndEvent(NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST, NULL);
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS, NULL);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        net_log_.EndEvent(NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS, NULL);
        break;
      case STATE_READ_BODY:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_READ_BODY, NULL);
        rv = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        rv = DoReadBodyComplete(rv);
        net_log_.EndEvent(NetLog::TYPE_HTTP_TRANSACTION_READ_BODY, NULL);
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(
            NetLog::TYPE_HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART, NULL);
        rv = DoDrainBodyForAuthRestart();
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE:
        rv = DoDrainBodyForAuthRestartComplete(rv);
        net_log_.EndEvent(
            NetLog::TYPE_HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART, NULL);
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

  session_->http_stream_factory()->RequestStream(request_,
                                                 &ssl_config_,
                                                 &proxy_info_,
                                                 this,
                                                 net_log_,
                                                 session_,
                                                 &stream_request_);
  return ERR_IO_PENDING;
}

int HttpNetworkTransaction::DoCreateStreamComplete(int result) {
  if (result == OK) {
    next_state_ = STATE_INIT_STREAM;
    DCHECK(stream_.get());
  }

  // At this point we are done with the stream_request_.
  stream_request_ = NULL;
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

    if (is_https_request())
      stream_->GetSSLInfo(&response_.ssl_info);
  } else {
    if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED)
      result = HandleCertificateRequest(result);

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
                               session_->auth_cache(),
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
                               session_->auth_cache(),
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
  if (request_headers_.empty() && !response_.was_fetched_via_spdy) {
    // Figure out if we can/should add Proxy-Authentication & Authentication
    // headers.
    HttpRequestHeaders authorization_headers;
    bool have_proxy_auth = (ShouldApplyProxyAuth() &&
                            HaveAuth(HttpAuth::AUTH_PROXY));
    bool have_server_auth = (ShouldApplyServerAuth() &&
                             HaveAuth(HttpAuth::AUTH_SERVER));
    if (have_proxy_auth)
      auth_controllers_[HttpAuth::AUTH_PROXY]->AddAuthorizationHeader(
          &authorization_headers);
    if (have_server_auth)
      auth_controllers_[HttpAuth::AUTH_SERVER]->AddAuthorizationHeader(
          &authorization_headers);
    std::string request_line;
    HttpRequestHeaders request_headers;

    BuildRequestHeaders(request_, authorization_headers, request_body,
                        !is_https_request() && (proxy_info_.is_http() ||
                                        proxy_info_.is_https()),
                        &request_line, &request_headers);

    if (session_->network_delegate())
      session_->network_delegate()->OnSendHttpRequest(&request_headers);

    if (net_log_.IsLoggingAll()) {
      net_log_.AddEvent(
          NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST_HEADERS,
          new NetLogHttpRequestParameter(request_line, request_headers));
    }

    request_headers_ = request_line + request_headers.ToString();
  } else {
    if (net_log_.IsLoggingAll()) {
      net_log_.AddEvent(
          NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST_HEADERS,
          new NetLogHttpRequestParameter(request_->url.spec(),
                                         request_->extra_headers));
    }
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
  } else if ((result == ERR_SSL_DECOMPRESSION_FAILURE_ALERT ||
              result == ERR_SSL_BAD_RECORD_MAC_ALERT) &&
             ssl_config_.tls1_enabled &&
             !SSLConfigService::IsKnownStrictTLSServer(request_->url.host())) {
    // Some buggy servers select DEFLATE compression when offered and then
    // fail to ever decompress anything. They will send a fatal alert telling
    // us this. Normally we would pick this up during the handshake because
    // our Finished message is compressed and we'll never get the server's
    // Finished if it fails to process ours.
    //
    // However, with False Start, we'll believe that the handshake is
    // complete as soon as we've /sent/ our Finished message. In this case,
    // we only find out that the server is buggy here, when we try to read
    // the initial reply.
    session_->http_stream_factory()->AddTLSIntolerantServer(request_->url);
    ResetConnectionAndRequestForResend();
    return OK;
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

  if (net_log_.IsLoggingAll()) {
    net_log_.AddEvent(
        NetLog::TYPE_HTTP_TRANSACTION_READ_RESPONSE_HEADERS,
        new NetLogHttpResponseParameter(response_.headers));
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
      FieldTrialList::Find("ConnCountImpact") &&
      !FieldTrialList::Find("ConnCountImpact")->group_name().empty());
  if (use_conn_impact_histogram) {
    UMA_HISTOGRAM_CLIPPED_TIMES(
        FieldTrial::MakeName("Net.Transaction_Connected_New",
            "ConnCountImpact"),
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
    }
  }

  static bool use_spdy_histogram(FieldTrialList::Find("SpdyImpact") &&
      !FieldTrialList::Find("SpdyImpact")->group_name().empty());
  if (use_spdy_histogram && response_.was_npn_negotiated) {
    UMA_HISTOGRAM_CLIPPED_TIMES(
      FieldTrial::MakeName("Net.Transaction_Connected_Under_10", "SpdyImpact"),
        total_duration, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10), 100);

    if (!reused_socket) {
      UMA_HISTOGRAM_CLIPPED_TIMES(
          FieldTrial::MakeName("Net.Transaction_Connected_New", "SpdyImpact"),
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

  if (stream_request_.get()) {
    // The server is asking for a client certificate during the initial
    // handshake.
    stream_request_->Cancel();
    stream_request_ = NULL;
  }

  // If the user selected one of the certificate in client_certs for this
  // server before, use it automatically.
  X509Certificate* client_cert = session_->ssl_client_auth_cache()->
                                 Lookup(GetHostAndPort(request_->url));
  if (client_cert) {
    const std::vector<scoped_refptr<X509Certificate> >& client_certs =
        response_.cert_request_info->client_certs;
    for (size_t i = 0; i < client_certs.size(); ++i) {
      if (client_cert->fingerprint().Equals(client_certs[i]->fingerprint())) {
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
    }
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
  stream_.reset();
  headers_valid_ = false;
  request_headers_.clear();
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
  request_headers_.clear();
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
  scoped_refptr<HttpResponseHeaders> headers = GetResponseHeaders();
  DCHECK(headers);

  int status = headers->response_code();
  if (status != 401 && status != 407)
    return OK;
  HttpAuth::Target target = status == 407 ?
                            HttpAuth::AUTH_PROXY : HttpAuth::AUTH_SERVER;
  if (target == HttpAuth::AUTH_PROXY && proxy_info_.is_direct())
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

#define STATE_CASE(s)  case s: \
                         description = StringPrintf("%s (0x%08X)", #s, s); \
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
      description = StringPrintf("Unknown state 0x%08X (%u)", state, state);
      break;
  }
  return description;
}

// TODO(gavinp): re-adjust this once SPDY v3 has three priority bits,
// eliminating the need for this folding.
int ConvertRequestPriorityToSpdyPriority(const RequestPriority priority) {
  DCHECK(HIGHEST <= priority && priority < NUM_PRIORITIES);
  switch (priority) {
    case LOWEST:
      return SPDY_PRIORITY_LOWEST-1;
    case IDLE:
      return SPDY_PRIORITY_LOWEST;
    default:
      return priority;
  }
}



#undef STATE_CASE

}  // namespace net
