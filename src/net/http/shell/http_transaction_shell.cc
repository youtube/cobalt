// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/shell/http_transaction_shell.h"

#include "base/message_loop.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_response_headers.h"

namespace net {

HttpTransactionShell::HttpTransactionShell(
    const net::HttpNetworkSession::Params* params, HttpStreamShell* stream)
        : stream_(stream),
          state_(STATE_CREATED),
          result_(OK),
          pending_callback_(false),
          ALLOW_THIS_IN_INITIALIZER_LIST(resolve_proxy_callback_(
              base::Bind(&HttpTransactionShell::OnResolveProxyComplete,
                         base::Unretained(this)))),
          ALLOW_THIS_IN_INITIALIZER_LIST(init_stream_callback_(
              base::Bind(&HttpTransactionShell::OnInitStreamComplete,
                         base::Unretained(this)))),
          ALLOW_THIS_IN_INITIALIZER_LIST(init_upload_callback_(
              base::Bind(&HttpTransactionShell::OnInitUploadComplete,
                         base::Unretained(this)))),
          ALLOW_THIS_IN_INITIALIZER_LIST(send_request_callback_(
              base::Bind(&HttpTransactionShell::OnSendRequestComplete,
                         base::Unretained(this)))),
          ALLOW_THIS_IN_INITIALIZER_LIST(read_response_headers_callback_(
              base::Bind(&HttpTransactionShell::OnReadResponseHeadersComplete,
                         base::Unretained(this)))),
          ALLOW_THIS_IN_INITIALIZER_LIST(read_response_body_callback_(
              base::Bind(&HttpTransactionShell::OnReadResponseBodyComplete,
                         base::Unretained(this)))),
          read_buf_len_(0),
          request_(NULL),
          params_(params) {
  DCHECK(stream_);
  current_message_loop_ = base::MessageLoopProxy::current();
}

// Starts the HTTP transaction (i.e., sends the HTTP request).
// The process contains the steps below:
// - ResolveProxy
// - InitStream
// - InitUploadData
// - SendRequest
// - ReadResponseHeaders
int HttpTransactionShell::Start(const HttpRequestInfo* request_info,
                                const CompletionCallback& callback,
                                const BoundNetLog& net_log) {
  // Validate port
  int port = request_info->url.EffectiveIntPort();
  if (!IsPortAllowedByDefault(port) && !IsPortAllowedByOverride(port)) {
    return ERR_UNSAFE_PORT;
  }
  request_ = request_info;
  net_log_ = net_log;
  DCHECK(callback_.is_null());
  callback_ = callback;

  int result = ResolveProxy();
  // If not IO pending, continue.
  if (result != ERR_IO_PENDING) {
    OnResolveProxyComplete(result);
    return result_;
  }
  pending_callback_ = true;
  return result;
}

void HttpTransactionShell::OnResolveProxyComplete(int result) {
  DCHECK_EQ(state_, STATE_CREATED);
  DCHECK(!callback_.is_null());
  if (result != OK) {
    SetFailed(result);
    result_ = result;
    return;
  }

  // Send proxy information to stream.
  bool using_proxy = !proxy_info_.is_empty() &&
                     (proxy_info_.is_http() || proxy_info_.is_https());
  if (using_proxy) {
    // Remove unsupported proxies from the list.
    proxy_info_.RemoveProxiesWithoutScheme(ProxyServer::SCHEME_DIRECT |
        ProxyServer::SCHEME_HTTP | ProxyServer::SCHEME_HTTPS);
    if (proxy_info_.is_empty()) {
      // No proxies/direct to choose from. This happens when we don't support
      // any of the proxies in the returned list.
      result_ = ERR_NO_SUPPORTED_PROXIES;
      return;
    }
    stream_->SetProxy(&proxy_info_);
  } else {
    stream_->SetProxy(NULL);
  }
  result = stream_->InitializeStream(request_, net_log_, init_stream_callback_);
  // If not IO pending, continue.
  if (result != ERR_IO_PENDING) {
    OnInitStreamComplete(result);
    return;
  }
  pending_callback_ = true;
  result_ = result;
}

void HttpTransactionShell::OnInitStreamComplete(int result) {
  DCHECK_EQ(state_, STATE_CREATED);
  DCHECK(!callback_.is_null());
  if (result != OK) {
    SetFailed(result);
    result_ = result;
    return;
  }

  state_ = STATE_STREAM_INITIALIZED;
  BuildRequestHeaders();
  if (request_->upload_data_stream)
    result = request_->upload_data_stream->Init(init_upload_callback_);

  if (result != ERR_IO_PENDING) {
    OnInitUploadComplete(result);
    return;
  }

  pending_callback_ = true;
  result_ = result;
}

void HttpTransactionShell::OnInitUploadComplete(int result) {
  DCHECK_EQ(state_, STATE_STREAM_INITIALIZED);
  DCHECK(!callback_.is_null());
  if (result != OK) {
    SetFailed(result);
    result_ = result;
    return;
  }
  net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST);
  BuildRequestHeaders();
  result = stream_->SendRequest(request_headers_, &response_,
                                send_request_callback_);

  if (result != ERR_IO_PENDING) {
    OnSendRequestComplete(result);
    return;
  }
  pending_callback_ = true;
  result_ = result;
}

void HttpTransactionShell::OnSendRequestComplete(int result) {
  DCHECK_EQ(state_, STATE_STREAM_INITIALIZED);
  DCHECK(!callback_.is_null());
  state_ = STATE_REQUEST_SENT;
  net_log_.EndEventWithNetErrorCode(
      NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST, result);
  if (result != OK) {
    SetFailed(result);
    result_ = result;
    return;
  }
  // Read header
  net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS);
  result = stream_->ReadResponseHeaders(read_response_headers_callback_);
  if (result != ERR_IO_PENDING) {
    OnReadResponseHeadersComplete(result);
    return;
  }
  pending_callback_ = true;
  result_ = result;
}

void HttpTransactionShell::OnReadResponseHeadersComplete(int result) {
  DCHECK_EQ(state_, STATE_REQUEST_SENT);
  DCHECK(!callback_.is_null());
  state_ = STATE_RESPONSE_HEADERS_RECEIVED;
  net_log_.EndEventWithNetErrorCode(
      NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS, result);
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
  }


  if (result != OK) {
    SetFailed(result);
    return;
  }

  net_log_.AddEvent(
      NetLog::TYPE_HTTP_TRANSACTION_READ_RESPONSE_HEADERS,
      base::Bind(&HttpResponseHeaders::NetLogCallback, response_.headers));

  // This is the last step we need for Start(), Perform callback
  // if needed, and reset it.
  DoCallback(result);
}

// Restarts the HTTP transaction, ignoring the last error.
int HttpTransactionShell::RestartIgnoringLastError(
    const CompletionCallback& callback) {
  // StreamLoader objects should handle this case
  NOTREACHED();
  return ERR_UNEXPECTED;
}

// Restarts the HTTP transaction with a client certificate.
int HttpTransactionShell::RestartWithCertificate(X509Certificate* client_cert,
    const CompletionCallback& callback) {
  // StreamLoader objects should handle this case
  NOTREACHED();
  return ERR_UNEXPECTED;
}

// Restarts the HTTP transaction with authentication credentials.
int HttpTransactionShell::RestartWithAuth(const AuthCredentials& credentials,
                                          const CompletionCallback& callback) {
  // StreamLoader objects should handle this case
  NOTREACHED();
  return ERR_UNEXPECTED;
}

// Returns true if auth is ready to be continued.
bool HttpTransactionShell::IsReadyToRestartForAuth() {
  // No need to restart, hence always returning false here.
  return false;
}

// Once response info is available for the transaction, response data may be
// read by calling this method.
int HttpTransactionShell::Read(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) {
  // No data to return
  if (state_ == STATE_DONE_READING)
    return OK;

  DCHECK_EQ(state_, STATE_RESPONSE_HEADERS_RECEIVED);

  DCHECK(stream_.get());
  net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_READ_BODY);
  int result = stream_->ReadResponseBody(buf, buf_len,
                                         read_response_body_callback_);
  // If not IO pending, continue
  if (result != ERR_IO_PENDING) {
    OnReadResponseBodyComplete(result);
    return result_;
  }
  callback_ = callback;
  pending_callback_ = true;
  return result;  // IO_PENDING
}

void HttpTransactionShell::OnReadResponseBodyComplete(int result) {
  DCHECK_EQ(state_, STATE_RESPONSE_HEADERS_RECEIVED);
  net_log_.EndEventWithNetErrorCode(
      NetLog::TYPE_HTTP_TRANSACTION_READ_BODY, result);
  result_ =  result;
  if (result < 0) {
    DCHECK_NE(result, ERR_IO_PENDING);
    SetFailed(result);
    return;
  }
  // If reading is done, update state before callback.
  if (stream_->IsResponseBodyComplete()) {
    DoneReading();
  }

  DoCallback(result);
}

// Stops further caching of this request by the HTTP cache, if there is any.
void HttpTransactionShell::StopCaching() {
  NOTIMPLEMENTED();
}

// Called to tell the transaction that we have successfully reached the end
// of the stream.
void HttpTransactionShell::DoneReading() {
  if (state_ != STATE_FAILED && state_ != STATE_DONE_READING) {
    state_ = STATE_DONE_READING;
    CloseStream();
    read_buf_ = NULL;
    read_buf_len_ = 0;
  }
}

// Returns the response info for this transaction or NULL if the response
// info is not available.
const HttpResponseInfo* HttpTransactionShell::GetResponseInfo() const {
  return &response_;
}

// Returns the load state for this transaction.
LoadState HttpTransactionShell::GetLoadState() const {
  switch (state_) {
    case STATE_CREATED:
      return LOAD_STATE_IDLE;
    case STATE_STREAM_INITIALIZED:
      return LOAD_STATE_SENDING_REQUEST;
    case STATE_REQUEST_SENT:
      return LOAD_STATE_WAITING_FOR_RESPONSE;
    case STATE_RESPONSE_HEADERS_RECEIVED:
      return LOAD_STATE_READING_RESPONSE;
    case STATE_DONE_READING:
      return LOAD_STATE_IDLE;
    case STATE_FAILED:
      return LOAD_STATE_IDLE;
  }
  return LOAD_STATE_IDLE;
}

// Returns the upload progress in bytes.  If there is no upload data,
// zero will be returned.  This does not include the request headers.
UploadProgress HttpTransactionShell::GetUploadProgress() const {
  if (!request_->upload_data_stream)
    return UploadProgress();

  return UploadProgress(request_->upload_data_stream->position(),
                        request_->upload_data_stream->size());
}

int HttpTransactionShell::ResolveProxy() {
  if (request_->load_flags & LOAD_BYPASS_PROXY) {
    proxy_info_.UseDirect();
    return OK;
  }

  return params_->proxy_service->ResolveProxy(request_->url, &proxy_info_,
      resolve_proxy_callback_, NULL, net_log_);
}

void HttpTransactionShell::BuildRequestHeaders() {
  request_headers_.SetHeader(HttpRequestHeaders::kHost,
                             GetHostAndOptionalPort(request_->url));

  request_headers_.SetHeader(HttpRequestHeaders::kConnection, "keep-alive");

  // Add a content length header?
  if (request_->upload_data_stream) {
    if (request_->upload_data_stream->is_chunked()) {
      request_headers_.SetHeader(
          HttpRequestHeaders::kTransferEncoding, "chunked");
    } else {
      request_headers_.SetHeader(
          HttpRequestHeaders::kContentLength,
          base::Uint64ToString(request_->upload_data_stream->size()));
    }
  } else if (request_->method == "POST" || request_->method == "PUT" ||
             request_->method == "HEAD") {
    // An empty POST/PUT request still needs a content length.  As for HEAD,
    // IE and Safari also add a content length header.  Presumably it is to
    // support sending a HEAD request to an URL that only expects to be sent a
    // POST or some other method that normally would have a message body.
    request_headers_.SetHeader(HttpRequestHeaders::kContentLength, "0");
  }

  // Honor load flags that impact proxy caches.
  if (request_->load_flags & LOAD_BYPASS_CACHE) {
    request_headers_.SetHeader(HttpRequestHeaders::kPragma, "no-cache");
    request_headers_.SetHeader(HttpRequestHeaders::kCacheControl, "no-cache");
  } else if (request_->load_flags & LOAD_VALIDATE_CACHE) {
    request_headers_.SetHeader(HttpRequestHeaders::kCacheControl, "max-age=0");
  }
  request_headers_.MergeFrom(request_->extra_headers);
}

void HttpTransactionShell::SetFailed(int result) {
  // Real Error happens
  state_ = STATE_FAILED;
  CloseStream();
  DoCallback(result);
}

void HttpTransactionShell::CloseStream() {
  stream_->Close(true);
  stream_.reset();
}

void HttpTransactionShell::DoCallback(int result) {
  if (pending_callback_) {
    DCHECK(current_message_loop_->BelongsToCurrentThread());

    DCHECK(!callback_.is_null());
    CompletionCallback c = callback_;
    callback_.Reset();
    pending_callback_ = false;
    c.Run(result);
  }
}

}  // namespace net
