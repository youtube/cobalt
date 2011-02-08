// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "googleurl/src/gurl.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_net_log_params.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_utils.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_stream_parser.h"
#include "net/socket/client_socket_handle.h"

namespace net {

HttpProxyClientSocket::HttpProxyClientSocket(
    ClientSocketHandle* transport_socket,
    const GURL& request_url,
    const std::string& user_agent,
    const HostPortPair& endpoint,
    const HostPortPair& proxy_server,
    HttpAuthCache* http_auth_cache,
    HttpAuthHandlerFactory* http_auth_handler_factory,
    bool tunnel,
    bool using_spdy,
    bool is_https_proxy)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &HttpProxyClientSocket::OnIOComplete)),
      next_state_(STATE_NONE),
      user_callback_(NULL),
      transport_(transport_socket),
      endpoint_(endpoint),
      auth_(tunnel ?
          new HttpAuthController(HttpAuth::AUTH_PROXY,
                                 GURL("http://" + proxy_server.ToString()),
                                 http_auth_cache,
                                 http_auth_handler_factory)
          : NULL),
      tunnel_(tunnel),
      using_spdy_(using_spdy),
      is_https_proxy_(is_https_proxy),
      net_log_(transport_socket->socket()->NetLog()) {
  // Synthesize the bits of a request that we actually use.
  request_.url = request_url;
  request_.method = "GET";
  if (!user_agent.empty())
    request_.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                     user_agent);
}

HttpProxyClientSocket::~HttpProxyClientSocket() {
  Disconnect();
}

int HttpProxyClientSocket::RestartWithAuth(CompletionCallback* callback) {
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(!user_callback_);

  int rv = PrepareForAuthRestart();
  if (rv != OK)
    return rv;

  rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

const HttpResponseInfo* HttpProxyClientSocket::GetConnectResponseInfo() const {
  return response_.headers ? &response_ : NULL;
}

HttpStream* HttpProxyClientSocket::CreateConnectResponseStream() {
  return new HttpBasicStream(transport_.release(),
                             http_stream_parser_.release(), false);
}


int HttpProxyClientSocket::Connect(CompletionCallback* callback) {
  DCHECK(transport_.get());
  DCHECK(transport_->socket());
  DCHECK(!user_callback_);

  // TODO(rch): figure out the right way to set up a tunnel with SPDY.
  // This approach sends the complete HTTPS request to the proxy
  // which allows the proxy to see "private" data.  Instead, we should
  // create an SSL tunnel to the origin server using the CONNECT method
  // inside a single SPDY stream.
  if (using_spdy_ || !tunnel_)
    next_state_ = STATE_DONE;
  if (next_state_ == STATE_DONE)
    return OK;

  DCHECK_EQ(STATE_NONE, next_state_);
  next_state_ = STATE_GENERATE_AUTH_TOKEN;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

void HttpProxyClientSocket::Disconnect() {
  if (transport_.get())
    transport_->socket()->Disconnect();

  // Reset other states to make sure they aren't mistakenly used later.
  // These are the states initialized by Connect().
  next_state_ = STATE_NONE;
  user_callback_ = NULL;
}

bool HttpProxyClientSocket::IsConnected() const {
  return next_state_ == STATE_DONE && transport_->socket()->IsConnected();
}

bool HttpProxyClientSocket::IsConnectedAndIdle() const {
  return next_state_ == STATE_DONE &&
    transport_->socket()->IsConnectedAndIdle();
}

const BoundNetLog& HttpProxyClientSocket::NetLog() const {
  return net_log_;
}

void HttpProxyClientSocket::SetSubresourceSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetSubresourceSpeculation();
  } else {
    NOTREACHED();
  }
}

void HttpProxyClientSocket::SetOmniboxSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetOmniboxSpeculation();
  } else {
    NOTREACHED();
  }
}

bool HttpProxyClientSocket::WasEverUsed() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->WasEverUsed();
  }
  NOTREACHED();
  return false;
}

bool HttpProxyClientSocket::UsingTCPFastOpen() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->UsingTCPFastOpen();
  }
  NOTREACHED();
  return false;
}

int HttpProxyClientSocket::Read(IOBuffer* buf, int buf_len,
                                CompletionCallback* callback) {
  DCHECK(!user_callback_);
  if (next_state_ != STATE_DONE) {
    // We're trying to read the body of the response but we're still trying
    // to establish an SSL tunnel through the proxy.  We can't read these
    // bytes when establishing a tunnel because they might be controlled by
    // an active network attacker.  We don't worry about this for HTTP
    // because an active network attacker can already control HTTP sessions.
    // We reach this case when the user cancels a 407 proxy auth prompt.
    // See http://crbug.com/8473.
    DCHECK_EQ(407, response_.headers->response_code());
    LogBlockedTunnelResponse(response_.headers->response_code());

    return ERR_TUNNEL_CONNECTION_FAILED;
  }

  return transport_->socket()->Read(buf, buf_len, callback);
}

int HttpProxyClientSocket::Write(IOBuffer* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK_EQ(STATE_DONE, next_state_);
  DCHECK(!user_callback_);

  return transport_->socket()->Write(buf, buf_len, callback);
}

bool HttpProxyClientSocket::SetReceiveBufferSize(int32 size) {
  return transport_->socket()->SetReceiveBufferSize(size);
}

bool HttpProxyClientSocket::SetSendBufferSize(int32 size) {
  return transport_->socket()->SetSendBufferSize(size);
}

int HttpProxyClientSocket::GetPeerAddress(AddressList* address) const {
  return transport_->socket()->GetPeerAddress(address);
}

int HttpProxyClientSocket::PrepareForAuthRestart() {
  if (!response_.headers.get())
    return ERR_CONNECTION_RESET;

  bool keep_alive = false;
  if (response_.headers->IsKeepAlive() &&
      http_stream_parser_->CanFindEndOfResponse()) {
    if (!http_stream_parser_->IsResponseBodyComplete()) {
      next_state_ = STATE_DRAIN_BODY;
      drain_buf_ = new IOBuffer(kDrainBodyBufferSize);
      return OK;
    }
    keep_alive = true;
  }

  // We don't need to drain the response body, so we act as if we had drained
  // the response body.
  return DidDrainBodyForAuthRestart(keep_alive);
}

int HttpProxyClientSocket::DidDrainBodyForAuthRestart(bool keep_alive) {
  if (keep_alive && transport_->socket()->IsConnectedAndIdle()) {
    next_state_ = STATE_GENERATE_AUTH_TOKEN;
    transport_->set_is_reused(true);
  } else {
    // This assumes that the underlying transport socket is a TCP socket,
    // since only TCP sockets are restartable.
    next_state_ = STATE_TCP_RESTART;
    transport_->socket()->Disconnect();
  }

  // Reset the other member variables.
  drain_buf_ = NULL;
  parser_buf_ = NULL;
  http_stream_parser_.reset();
  request_line_.clear();
  request_headers_.Clear();
  response_ = HttpResponseInfo();
  return OK;
}

int HttpProxyClientSocket::HandleAuthChallenge() {
  DCHECK(response_.headers);

  int rv = auth_->HandleAuthChallenge(response_.headers, false, true, net_log_);
  response_.auth_challenge = auth_->auth_info();
  if (rv == OK)
    return ERR_PROXY_AUTH_REQUESTED;

  return rv;
}

void HttpProxyClientSocket::LogBlockedTunnelResponse(int response_code) const {
  LOG(WARNING) << "Blocked proxy response with status " << response_code
               << " to CONNECT request for "
               << GetHostAndPort(request_.url) << ".";
}

void HttpProxyClientSocket::DoCallback(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(user_callback_);

  // Since Run() may result in Read being called,
  // clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(result);
}

void HttpProxyClientSocket::OnIOComplete(int result) {
  DCHECK_NE(STATE_NONE, next_state_);
  DCHECK_NE(STATE_DONE, next_state_);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int HttpProxyClientSocket::DoLoop(int last_io_result) {
  DCHECK_NE(next_state_, STATE_NONE);
  DCHECK_NE(next_state_, STATE_DONE);
  int rv = last_io_result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
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
        net_log_.BeginEvent(
            NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_SEND_REQUEST, NULL);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_SEND_REQUEST, rv);
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(
            NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_READ_HEADERS, NULL);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_HTTP_TRANSACTION_TUNNEL_READ_HEADERS, rv);
        break;
      case STATE_DRAIN_BODY:
        DCHECK_EQ(OK, rv);
        rv = DoDrainBody();
        break;
      case STATE_DRAIN_BODY_COMPLETE:
        rv = DoDrainBodyComplete(rv);
        break;
      case STATE_TCP_RESTART:
        DCHECK_EQ(OK, rv);
        rv = DoTCPRestart();
        break;
      case STATE_TCP_RESTART_COMPLETE:
        rv = DoTCPRestartComplete(rv);
        break;
      case STATE_DONE:
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE &&
           next_state_ != STATE_DONE);
  return rv;
}

int HttpProxyClientSocket::DoGenerateAuthToken() {
  next_state_ = STATE_GENERATE_AUTH_TOKEN_COMPLETE;
  return auth_->MaybeGenerateAuthToken(&request_, &io_callback_, net_log_);
}

int HttpProxyClientSocket::DoGenerateAuthTokenComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  if (result == OK)
    next_state_ = STATE_SEND_REQUEST;
  return result;
}

int HttpProxyClientSocket::DoSendRequest() {
  next_state_ = STATE_SEND_REQUEST_COMPLETE;

  // This is constructed lazily (instead of within our Start method), so that
  // we have proxy info available.
  if (request_line_.empty()) {
    DCHECK(request_headers_.IsEmpty());
    HttpRequestHeaders authorization_headers;
    if (auth_->HaveAuth())
      auth_->AddAuthorizationHeader(&authorization_headers);
    BuildTunnelRequest(request_, authorization_headers, endpoint_,
                       &request_line_, &request_headers_);
    if (net_log_.IsLoggingAllEvents()) {
      net_log_.AddEvent(
          NetLog::TYPE_HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
          make_scoped_refptr(new NetLogHttpRequestParameter(
              request_line_, request_headers_)));
    }
  }

  parser_buf_ = new GrowableIOBuffer();
  http_stream_parser_.reset(
      new HttpStreamParser(transport_.get(), &request_, parser_buf_, net_log_));
  return http_stream_parser_->SendRequest(request_line_, request_headers_, NULL,
                                          &response_, &io_callback_);
}

int HttpProxyClientSocket::DoSendRequestComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_READ_HEADERS;
  return OK;
}

int HttpProxyClientSocket::DoReadHeaders() {
  next_state_ = STATE_READ_HEADERS_COMPLETE;
  return http_stream_parser_->ReadResponseHeaders(&io_callback_);
}

int HttpProxyClientSocket::DoReadHeadersComplete(int result) {
  if (result < 0)
    return result;

  // Require the "HTTP/1.x" status line for SSL CONNECT.
  if (response_.headers->GetParsedHttpVersion() < HttpVersion(1, 0))
    return ERR_TUNNEL_CONNECTION_FAILED;

  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        NetLog::TYPE_HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
        make_scoped_refptr(new NetLogHttpResponseParameter(response_.headers)));
  }

  switch (response_.headers->response_code()) {
    case 200:  // OK
      if (http_stream_parser_->IsMoreDataBuffered())
        // The proxy sent extraneous data after the headers.
        return ERR_TUNNEL_CONNECTION_FAILED;

      next_state_ = STATE_DONE;
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
      // The next state is intentionally not set as it should be STATE_NONE;
      return HandleAuthChallenge();

    default:
      if (is_https_proxy_)
        return ERR_HTTPS_PROXY_TUNNEL_RESPONSE;
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

int HttpProxyClientSocket::DoDrainBody() {
  DCHECK(drain_buf_);
  DCHECK(transport_->is_initialized());
  next_state_ = STATE_DRAIN_BODY_COMPLETE;
  return http_stream_parser_->ReadResponseBody(drain_buf_, kDrainBodyBufferSize,
                                               &io_callback_);
}

int HttpProxyClientSocket::DoDrainBodyComplete(int result) {
  if (result < 0)
    return result;

  if (http_stream_parser_->IsResponseBodyComplete())
    return DidDrainBodyForAuthRestart(true);

  // Keep draining.
  next_state_ = STATE_DRAIN_BODY;
  return OK;
}

int HttpProxyClientSocket::DoTCPRestart() {
  next_state_ = STATE_TCP_RESTART_COMPLETE;
  return transport_->socket()->Connect(&io_callback_);
}

int HttpProxyClientSocket::DoTCPRestartComplete(int result) {
  if (result != OK)
    return result;

  next_state_ = STATE_GENERATE_AUTH_TOKEN;
  return result;
}

}  // namespace net
