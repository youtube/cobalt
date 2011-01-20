// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_proxy_client_socket.h"

#include <algorithm>  // min

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/auth.h"
#include "net/base/io_buffer.h"
#include "net/base/net_util.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_net_log_params.h"
#include "net/http/http_proxy_utils.h"
#include "net/http/http_response_headers.h"
#include "net/spdy/spdy_http_utils.h"

namespace net {

SpdyProxyClientSocket::SpdyProxyClientSocket(
    SpdyStream* spdy_stream,
    const std::string& user_agent,
    const HostPortPair& endpoint,
    const GURL& url,
    const HostPortPair& proxy_server,
    HttpAuthCache* auth_cache,
    HttpAuthHandlerFactory* auth_handler_factory)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &SpdyProxyClientSocket::OnIOComplete)),
      next_state_(STATE_DISCONNECTED),
      spdy_stream_(spdy_stream),
      read_callback_(NULL),
      write_callback_(NULL),
      endpoint_(endpoint),
      auth_(
          new HttpAuthController(HttpAuth::AUTH_PROXY,
                                 GURL("http://" + proxy_server.ToString()),
                                 auth_cache,
                                 auth_handler_factory)),
      user_buffer_(NULL),
      write_buffer_len_(0),
      write_bytes_outstanding_(0),
      eof_has_been_read_(false),
      net_log_(spdy_stream->net_log()) {
  request_.method = "CONNECT";
  request_.url = url;
  if (!user_agent.empty())
    request_.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                     user_agent);
  spdy_stream_->SetDelegate(this);
  was_ever_used_ = spdy_stream_->WasEverUsed();
}

SpdyProxyClientSocket::~SpdyProxyClientSocket() {
  Disconnect();
}

const HttpResponseInfo* SpdyProxyClientSocket::GetConnectResponseInfo() const {
  return response_.headers ? &response_ : NULL;
}

HttpStream* SpdyProxyClientSocket::CreateConnectResponseStream() {
  DCHECK(response_stream_.get());
  return response_stream_.release();
}

// Sends a SYN_STREAM frame to the proxy with a CONNECT request
// for the specified endpoint.  Waits for the server to send back
// a SYN_REPLY frame.  OK will be returned if the status is 200.
// ERR_TUNNEL_CONNECTION_FAILED will be returned for any other status.
// In any of these cases, Read() may be called to retrieve the HTTP
// response body.  Any other return values should be considered fatal.
// TODO(rch): handle 407 proxy auth requested correctly, perhaps
// by creating a new stream for the subsequent request.
// TODO(rch): create a more appropriate error code to disambiguate
// the HTTPS Proxy tunnel failure from an HTTP Proxy tunnel failure.
int SpdyProxyClientSocket::Connect(CompletionCallback* callback) {
  DCHECK(!read_callback_);
  if (next_state_ == STATE_OPEN)
    return OK;

  DCHECK_EQ(STATE_DISCONNECTED, next_state_);
  next_state_ = STATE_GENERATE_AUTH_TOKEN;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    read_callback_ = callback;
  return rv;
}

void SpdyProxyClientSocket::Disconnect() {
  read_buffer_.clear();
  user_buffer_ = NULL;
  read_callback_ = NULL;

  write_buffer_len_ = 0;
  write_bytes_outstanding_ = 0;
  write_callback_ = NULL;

  next_state_ = STATE_DISCONNECTED;

  if (spdy_stream_)
    // This will cause OnClose to be invoked, which takes care of
    // cleaning up all the internal state.
    spdy_stream_->Cancel();
}

bool SpdyProxyClientSocket::IsConnected() const {
  return next_state_ == STATE_OPEN || next_state_ == STATE_CLOSED;
}

bool SpdyProxyClientSocket::IsConnectedAndIdle() const {
  return IsConnected() && !spdy_stream_->is_idle();
}

const BoundNetLog& SpdyProxyClientSocket::NetLog() const {
  return net_log_;
}

void SpdyProxyClientSocket::SetSubresourceSpeculation() {
  // TODO(rch): what should this implementation be?
}

void SpdyProxyClientSocket::SetOmniboxSpeculation() {
  // TODO(rch): what should this implementation be?
}

bool SpdyProxyClientSocket::WasEverUsed() const {
  return was_ever_used_ || (spdy_stream_ && spdy_stream_->WasEverUsed());
}

bool SpdyProxyClientSocket::UsingTCPFastOpen() const {
  return false;
}

int SpdyProxyClientSocket::Read(IOBuffer* buf, int buf_len,
                                CompletionCallback* callback) {
  DCHECK(!read_callback_);
  DCHECK(!user_buffer_);

  if (next_state_ == STATE_DISCONNECTED)
    return ERR_SOCKET_NOT_CONNECTED;

  if (!spdy_stream_ && read_buffer_.empty()) {
    if (eof_has_been_read_)
      return ERR_CONNECTION_CLOSED;
    eof_has_been_read_ = true;
    return 0;
  }

  DCHECK(next_state_ == STATE_OPEN || next_state_ == STATE_CLOSED);
  DCHECK(buf);
  user_buffer_ = new DrainableIOBuffer(buf, buf_len);
  int result = PopulateUserReadBuffer();
  if (result == 0) {
    DCHECK(callback);
    read_callback_ = callback;
    return ERR_IO_PENDING;
  }
  user_buffer_ = NULL;
  return result;
}

int SpdyProxyClientSocket::PopulateUserReadBuffer() {
  if (!user_buffer_)
    return ERR_IO_PENDING;

  while (!read_buffer_.empty() && user_buffer_->BytesRemaining() > 0) {
    scoped_refptr<DrainableIOBuffer> data = read_buffer_.front();
    const int bytes_to_copy = std::min(user_buffer_->BytesRemaining(),
                                       data->BytesRemaining());
    memcpy(user_buffer_->data(), data->data(), bytes_to_copy);
    user_buffer_->DidConsume(bytes_to_copy);
    if (data->BytesRemaining() == bytes_to_copy) {
      // Consumed all data from this buffer
      read_buffer_.pop_front();
    } else {
      data->DidConsume(bytes_to_copy);
    }
  }

  return user_buffer_->BytesConsumed();
}

int SpdyProxyClientSocket::Write(IOBuffer* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK(!write_callback_);
  if (next_state_ == STATE_DISCONNECTED)
    return ERR_SOCKET_NOT_CONNECTED;

  if (!spdy_stream_)
    return ERR_CONNECTION_CLOSED;

  write_bytes_outstanding_= buf_len;
  if (buf_len <= kMaxSpdyFrameChunkSize) {
    int rv = spdy_stream_->WriteStreamData(buf, buf_len, spdy::DATA_FLAG_NONE);
    if (rv == ERR_IO_PENDING) {
      write_callback_ = callback;
      write_buffer_len_ = buf_len;
    }
    return rv;
  }

  // Since a SPDY Data frame can only include kMaxSpdyFrameChunkSize bytes
  // we need to send multiple data frames
  for (int i = 0; i < buf_len; i += kMaxSpdyFrameChunkSize) {
    int len = std::min(kMaxSpdyFrameChunkSize, buf_len - i);
    scoped_refptr<DrainableIOBuffer> iobuf(new DrainableIOBuffer(buf, i + len));
    iobuf->SetOffset(i);
    int rv = spdy_stream_->WriteStreamData(iobuf, len, spdy::DATA_FLAG_NONE);
    if (rv > 0) {
      write_bytes_outstanding_ -= rv;
    } else if (rv != ERR_IO_PENDING) {
      return rv;
    }
  }
  if (write_bytes_outstanding_ > 0) {
    write_callback_ = callback;
    write_buffer_len_ = buf_len;
    return ERR_IO_PENDING;
  } else {
    return buf_len;
  }
}

bool SpdyProxyClientSocket::SetReceiveBufferSize(int32 size) {
  // Since this ClientSocket sits on top of a shared SpdySession, it
  // is not safe for callers to set change this underlying socket.
  return false;
}

bool SpdyProxyClientSocket::SetSendBufferSize(int32 size) {
  // Since this ClientSocket sits on top of a shared SpdySession, it
  // is not safe for callers to set change this underlying socket.
  return false;
}

int SpdyProxyClientSocket::GetPeerAddress(AddressList* address) const {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  return spdy_stream_->GetPeerAddress(address);
}

void SpdyProxyClientSocket::OnIOComplete(int result) {
  DCHECK_NE(STATE_DISCONNECTED, next_state_);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    CompletionCallback* c = read_callback_;
    read_callback_ = NULL;
    c->Run(rv);
  }
}

int SpdyProxyClientSocket::DoLoop(int last_io_result) {
  DCHECK_NE(next_state_, STATE_DISCONNECTED);
  int rv = last_io_result;
  do {
    State state = next_state_;
    next_state_ = STATE_DISCONNECTED;
    switch (state) {
      case STATE_GENERATE_AUTH_TOKEN:
        DCHECK_EQ(OK, rv);
        rv = DoGenerateAuthToken();
        break;
      case STATE_GENERATE_AUTH_TOKEN_COMPLETE:
        rv = DoGenerateAuthTokenComplete(rv);
        break;
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_SEND_REQUEST,
                            NULL);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        net_log_.EndEvent(NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_SEND_REQUEST,
                          NULL);
        break;
      case STATE_READ_REPLY_COMPLETE:
        rv = DoReadReplyComplete(rv);
        net_log_.EndEvent(NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_READ_HEADERS,
                          NULL);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_DISCONNECTED &&
           next_state_ != STATE_OPEN);
  return rv;
}

int SpdyProxyClientSocket::DoGenerateAuthToken() {
  next_state_ = STATE_GENERATE_AUTH_TOKEN_COMPLETE;
  return auth_->MaybeGenerateAuthToken(&request_, &io_callback_, net_log_);
}

int SpdyProxyClientSocket::DoGenerateAuthTokenComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  if (result == OK)
    next_state_ = STATE_SEND_REQUEST;
  return result;
}

int SpdyProxyClientSocket::DoSendRequest() {
  next_state_ = STATE_SEND_REQUEST_COMPLETE;

  // Add Proxy-Authentication header if necessary.
  HttpRequestHeaders authorization_headers;
  if (auth_->HaveAuth()) {
    auth_->AddAuthorizationHeader(&authorization_headers);
  }

  std::string request_line;
  HttpRequestHeaders request_headers;
  BuildTunnelRequest(request_, authorization_headers, endpoint_, &request_line,
                     &request_headers);
  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        NetLog::TYPE_HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
        make_scoped_refptr(new NetLogHttpRequestParameter(
            request_line, request_headers)));
  }

  request_.extra_headers.MergeFrom(request_headers);
  linked_ptr<spdy::SpdyHeaderBlock> headers(new spdy::SpdyHeaderBlock());
  CreateSpdyHeadersFromHttpRequest(request_, request_headers, headers.get(),
                                   true);
  // Reset the URL to be the endpoint of the connection
  (*headers)["url"] = endpoint_.ToString();
  headers->erase("scheme");
  spdy_stream_->set_spdy_headers(headers);

  return spdy_stream_->SendRequest(true);
}

int SpdyProxyClientSocket::DoSendRequestComplete(int result) {
  if (result < 0)
    return result;

  // Wait for SYN_REPLY frame from the server
  next_state_ = STATE_READ_REPLY_COMPLETE;
  return ERR_IO_PENDING;
}

int SpdyProxyClientSocket::DoReadReplyComplete(int result) {
  // We enter this method directly from DoSendRequestComplete, since
  // we are notified by a callback when the SYN_REPLY frame arrives

  if (result < 0)
    return result;

  // Require the "HTTP/1.x" status line for SSL CONNECT.
  if (response_.headers->GetParsedHttpVersion() < HttpVersion(1, 0))
    return ERR_TUNNEL_CONNECTION_FAILED;

  next_state_ = STATE_OPEN;
  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        NetLog::TYPE_HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
        make_scoped_refptr(new NetLogHttpResponseParameter(response_.headers)));
  }

  if (response_.headers->response_code() == 200) {
    return OK;
  } else if (response_.headers->response_code() == 407) {
    return ERR_TUNNEL_CONNECTION_FAILED;
  } else {
    // Immediately hand off our SpdyStream to a newly created SpdyHttpStream
    // so that any subsequent SpdyFrames are processed in the context of
    // the HttpStream, not the socket.
    DCHECK(spdy_stream_);
    SpdyStream* stream = spdy_stream_;
    spdy_stream_ = NULL;
    response_stream_.reset(new SpdyHttpStream(NULL, false));
    response_stream_->InitializeWithExistingStream(stream);
    next_state_ = STATE_DISCONNECTED;
    return ERR_HTTPS_PROXY_TUNNEL_RESPONSE;
  }
}

// SpdyStream::Delegate methods:
// Called when SYN frame has been sent.
// Returns true if no more data to be sent after SYN frame.
bool SpdyProxyClientSocket::OnSendHeadersComplete(int status) {
  DCHECK_EQ(next_state_, STATE_SEND_REQUEST_COMPLETE);

  OnIOComplete(status);

  // We return true here so that we send |spdy_stream_| into
  // STATE_OPEN (ala WebSockets).
  return true;
}

int SpdyProxyClientSocket::OnSendBody() {
  // Because we use |spdy_stream_| via STATE_OPEN (ala WebSockets)
  // OnSendBody() should never be called.
  NOTREACHED();
  return ERR_UNEXPECTED;
}

bool SpdyProxyClientSocket::OnSendBodyComplete(int status) {
  // Because we use |spdy_stream_| via STATE_OPEN (ala WebSockets)
  // OnSendBodyComplete() should never be called.
  NOTREACHED();
  return false;
}

int SpdyProxyClientSocket::OnResponseReceived(
    const spdy::SpdyHeaderBlock& response,
    base::Time response_time,
    int status) {
  // If we've already received the reply, existing headers are too late.
  // TODO(mbelshe): figure out a way to make HEADERS frames useful after the
  //                initial response.
  if (next_state_ != STATE_READ_REPLY_COMPLETE)
    return OK;

  // Save the response
  int rv = SpdyHeadersToHttpResponse(response, &response_);
  if (rv == ERR_INCOMPLETE_SPDY_HEADERS)
    return rv;  // More headers are coming.

  OnIOComplete(status);
  return OK;
}

// Called when data is received.
void SpdyProxyClientSocket::OnDataReceived(const char* data, int length) {
  if (length > 0) {
    // Save the received data.
    scoped_refptr<IOBuffer> io_buffer(new IOBuffer(length));
    memcpy(io_buffer->data(), data, length);
    read_buffer_.push_back(
        make_scoped_refptr(new DrainableIOBuffer(io_buffer, length)));
  }

  if (read_callback_) {
    int rv = PopulateUserReadBuffer();
    CompletionCallback* c = read_callback_;
    read_callback_ = NULL;
    user_buffer_ = NULL;
    c->Run(rv);
  }
}

void SpdyProxyClientSocket::OnDataSent(int length)  {
  DCHECK(write_callback_);

  write_bytes_outstanding_ -= length;

  DCHECK_GE(write_bytes_outstanding_, 0);

  if (write_bytes_outstanding_ == 0) {
    int rv = write_buffer_len_;
    write_buffer_len_ = 0;
    write_bytes_outstanding_ = 0;
    CompletionCallback* c = write_callback_;
    write_callback_ = NULL;
    c->Run(rv);
  }
}

void SpdyProxyClientSocket::OnClose(int status)  {
  DCHECK(spdy_stream_);
  was_ever_used_ = spdy_stream_->WasEverUsed();
  spdy_stream_ = NULL;

  bool connecting = next_state_ != STATE_DISCONNECTED &&
      next_state_ < STATE_OPEN;
  if (next_state_ == STATE_OPEN)
    next_state_ = STATE_CLOSED;
  else
    next_state_ = STATE_DISCONNECTED;

  CompletionCallback* write_callback = write_callback_;
  write_callback_ = NULL;
  write_buffer_len_ = 0;
  write_bytes_outstanding_ = 0;

  // If we're in the middle of connecting, we need to make sure
  // we invoke the connect callback.
  if (connecting) {
    DCHECK(read_callback_);
    CompletionCallback* read_callback = read_callback_;
    read_callback_ = NULL;
    read_callback->Run(status);
  } else if (read_callback_) {
    // If we have a read_callback, the we need to make sure we call it back
    OnDataReceived(NULL, 0);
  }
  if (write_callback)
    write_callback->Run(ERR_CONNECTION_CLOSED);
}

}  // namespace net
