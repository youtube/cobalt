// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(ukai): code is similar with http_network_transaction.cc.  We should
//   think about ways to share code, if possible.

#include "net/socket_stream/socket_stream.h"

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "net/base/auth.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socks5_client_socket.h"
#include "net/socket/socks_client_socket.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket_stream/socket_stream_metrics.h"
#include "net/url_request/url_request.h"

static const int kMaxPendingSendAllowed = 32768;  // 32 kilobytes.
static const int kReadBufferSize = 4096;

namespace net {

SocketStream::ResponseHeaders::ResponseHeaders() : IOBuffer() {}

void SocketStream::ResponseHeaders::Realloc(size_t new_size) {
  headers_.reset(static_cast<char*>(realloc(headers_.release(), new_size)));
}

SocketStream::ResponseHeaders::~ResponseHeaders() { data_ = NULL; }

SocketStream::SocketStream(const GURL& url, Delegate* delegate)
    : delegate_(delegate),
      url_(url),
      max_pending_send_allowed_(kMaxPendingSendAllowed),
      next_state_(STATE_NONE),
      host_resolver_(NULL),
      cert_verifier_(NULL),
      http_auth_handler_factory_(NULL),
      factory_(ClientSocketFactory::GetDefaultFactory()),
      proxy_mode_(kDirectConnection),
      proxy_url_(url),
      pac_request_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &SocketStream::OnIOCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &SocketStream::OnReadCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &SocketStream::OnWriteCompleted)),
      read_buf_(NULL),
      write_buf_(NULL),
      current_write_buf_(NULL),
      write_buf_offset_(0),
      write_buf_size_(0),
      closing_(false),
      server_closed_(false),
      metrics_(new SocketStreamMetrics(url)) {
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
  DCHECK(delegate_);
}

SocketStream::UserData* SocketStream::GetUserData(
    const void* key) const {
  UserDataMap::const_iterator found = user_data_.find(key);
  if (found != user_data_.end())
    return found->second.get();
  return NULL;
}

void SocketStream::SetUserData(const void* key, UserData* data) {
  user_data_[key] = linked_ptr<UserData>(data);
}

bool SocketStream::is_secure() const {
  return url_.SchemeIs("wss");
}

void SocketStream::set_context(URLRequestContext* context) {
  scoped_refptr<URLRequestContext> prev_context = context_;

  context_ = context;

  if (prev_context != context) {
    if (prev_context && pac_request_) {
      prev_context->proxy_service()->CancelPacRequest(pac_request_);
      pac_request_ = NULL;
    }

    net_log_.EndEvent(NetLog::TYPE_REQUEST_ALIVE, NULL);
    net_log_ = BoundNetLog();

    if (context) {
      net_log_ = BoundNetLog::Make(
          context->net_log(),
          NetLog::SOURCE_SOCKET_STREAM);

      net_log_.BeginEvent(NetLog::TYPE_REQUEST_ALIVE, NULL);
    }
  }

  if (context_) {
    host_resolver_ = context_->host_resolver();
    cert_verifier_ = context_->cert_verifier();
    http_auth_handler_factory_ = context_->http_auth_handler_factory();
  }
}

void SocketStream::Connect() {
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
  if (context_)
    ssl_config_service()->GetSSLConfig(&ssl_config_);
  DCHECK_EQ(next_state_, STATE_NONE);

  AddRef();  // Released in Finish()
  // Open a connection asynchronously, so that delegate won't be called
  // back before returning Connect().
  next_state_ = STATE_RESOLVE_PROXY;
  net_log_.BeginEvent(
      NetLog::TYPE_SOCKET_STREAM_CONNECT,
      make_scoped_refptr(
          new NetLogStringParameter("url", url_.possibly_invalid_spec())));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SocketStream::DoLoop, OK));
}

bool SocketStream::SendData(const char* data, int len) {
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
  if (!socket_.get() || !socket_->IsConnected() || next_state_ == STATE_NONE)
    return false;
  if (write_buf_) {
    int current_amount_send = write_buf_size_ - write_buf_offset_;
    for (PendingDataQueue::const_iterator iter = pending_write_bufs_.begin();
         iter != pending_write_bufs_.end();
         ++iter)
      current_amount_send += (*iter)->size();

    current_amount_send += len;
    if (current_amount_send > max_pending_send_allowed_)
      return false;

    pending_write_bufs_.push_back(make_scoped_refptr(
        new IOBufferWithSize(len)));
    memcpy(pending_write_bufs_.back()->data(), data, len);
    return true;
  }
  DCHECK(!current_write_buf_);
  write_buf_ = new IOBuffer(len);
  memcpy(write_buf_->data(), data, len);
  write_buf_size_ = len;
  write_buf_offset_ = 0;
  // Send pending data asynchronously, so that delegate won't be called
  // back before returning SendData().
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SocketStream::DoLoop, OK));
  return true;
}

void SocketStream::Close() {
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
  // If next_state_ is STATE_NONE, the socket was not opened, or already
  // closed.  So, return immediately.
  // Otherwise, it might call Finish() more than once, so breaks balance
  // of AddRef() and Release() in Connect() and Finish(), respectively.
  if (next_state_ == STATE_NONE)
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SocketStream::DoClose));
}

void SocketStream::RestartWithAuth(
    const string16& username, const string16& password) {
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
  DCHECK(auth_handler_.get());
  if (!socket_.get()) {
    LOG(ERROR) << "Socket is closed before restarting with auth.";
    return;
  }

  if (auth_identity_.invalid) {
    // Update the username/password.
    auth_identity_.source = HttpAuth::IDENT_SRC_EXTERNAL;
    auth_identity_.invalid = false;
    auth_identity_.username = username;
    auth_identity_.password = password;
  }

  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SocketStream::DoRestartWithAuth));
}

void SocketStream::DetachDelegate() {
  if (!delegate_)
    return;
  delegate_ = NULL;
  net_log_.AddEvent(NetLog::TYPE_CANCELLED, NULL);
  // We don't need to send pending data when client detach the delegate.
  pending_write_bufs_.clear();
  Close();
}

void SocketStream::SetHostResolver(HostResolver* host_resolver) {
  DCHECK(host_resolver);
  host_resolver_ = host_resolver;
}

void SocketStream::SetClientSocketFactory(
    ClientSocketFactory* factory) {
  DCHECK(factory);
  factory_ = factory;
}

SocketStream::~SocketStream() {
  set_context(NULL);
  DCHECK(!delegate_);
  DCHECK(!pac_request_);
}

void SocketStream::CopyAddrInfo(struct addrinfo* head) {
  addresses_.Copy(head, true);
}

void SocketStream::DoClose() {
  closing_ = true;
  // If next_state_ is STATE_TCP_CONNECT, it's waiting other socket establishing
  // connection.  If next_state_ is STATE_AUTH_REQUIRED, it's waiting for
  // restarting.  In these states, we'll close the SocketStream now.
  if (next_state_ == STATE_TCP_CONNECT || next_state_ == STATE_AUTH_REQUIRED) {
    DoLoop(ERR_ABORTED);
    return;
  }
  // If next_state_ is STATE_READ_WRITE, we'll run DoLoop and close
  // the SocketStream.
  // If it's writing now, we should defer the closing after the current
  // writing is completed.
  if (next_state_ == STATE_READ_WRITE && !current_write_buf_)
    DoLoop(ERR_ABORTED);

  // In other next_state_, we'll wait for callback of other APIs, such as
  // ResolveProxy().
}

void SocketStream::Finish(int result) {
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
  DCHECK_LE(result, OK);
  if (result == OK)
    result = ERR_CONNECTION_CLOSED;
  DCHECK_EQ(next_state_, STATE_NONE);
  DVLOG(1) << "Finish result=" << ErrorToString(result);
  if (delegate_)
    delegate_->OnError(this, result);

  metrics_->OnClose();
  Delegate* delegate = delegate_;
  delegate_ = NULL;
  if (delegate) {
    delegate->OnClose(this);
  }
  Release();
}

int SocketStream::DidEstablishConnection() {
  if (!socket_.get() || !socket_->IsConnected()) {
    next_state_ = STATE_CLOSE;
    return ERR_CONNECTION_FAILED;
  }
  next_state_ = STATE_READ_WRITE;
  metrics_->OnConnected();

  net_log_.EndEvent(NetLog::TYPE_SOCKET_STREAM_CONNECT, NULL);
  if (delegate_)
    delegate_->OnConnected(this, max_pending_send_allowed_);

  return OK;
}

int SocketStream::DidReceiveData(int result) {
  DCHECK(read_buf_);
  DCHECK_GT(result, 0);
  net_log_.AddEvent(NetLog::TYPE_SOCKET_STREAM_RECEIVED, NULL);
  int len = result;
  metrics_->OnRead(len);
  if (delegate_) {
    // Notify recevied data to delegate.
    delegate_->OnReceivedData(this, read_buf_->data(), len);
  }
  read_buf_ = NULL;
  return OK;
}

int SocketStream::DidSendData(int result) {
  DCHECK_GT(result, 0);
  net_log_.AddEvent(NetLog::TYPE_SOCKET_STREAM_SENT, NULL);
  int len = result;
  metrics_->OnWrite(len);
  current_write_buf_ = NULL;
  if (delegate_)
    delegate_->OnSentData(this, len);

  int remaining_size = write_buf_size_ - write_buf_offset_ - len;
  if (remaining_size == 0) {
    if (!pending_write_bufs_.empty()) {
      write_buf_size_ = pending_write_bufs_.front()->size();
      write_buf_ = pending_write_bufs_.front();
      pending_write_bufs_.pop_front();
    } else {
      write_buf_size_ = 0;
      write_buf_ = NULL;
    }
    write_buf_offset_ = 0;
  } else {
    write_buf_offset_ += len;
  }
  return OK;
}

void SocketStream::OnIOCompleted(int result) {
  DoLoop(result);
}

void SocketStream::OnReadCompleted(int result) {
  if (result == 0) {
    // 0 indicates end-of-file, so socket was closed.
    // Don't close the socket if it's still writing.
    server_closed_ = true;
  } else if (result > 0 && read_buf_) {
    result = DidReceiveData(result);
  }
  DoLoop(result);
}

void SocketStream::OnWriteCompleted(int result) {
  if (result >= 0 && write_buf_) {
    result = DidSendData(result);
  }
  DoLoop(result);
}

void SocketStream::DoLoop(int result) {
  // If context was not set, close immediately.
  if (!context_)
    next_state_ = STATE_CLOSE;

  if (next_state_ == STATE_NONE)
    return;

  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_PROXY:
        DCHECK_EQ(OK, result);
        result = DoResolveProxy();
        break;
      case STATE_RESOLVE_PROXY_COMPLETE:
        result = DoResolveProxyComplete(result);
        break;
      case STATE_RESOLVE_HOST:
        DCHECK_EQ(OK, result);
        result = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        result = DoResolveHostComplete(result);
        break;
      case STATE_TCP_CONNECT:
        result = DoTcpConnect(result);
        break;
      case STATE_TCP_CONNECT_COMPLETE:
        result = DoTcpConnectComplete(result);
        break;
      case STATE_WRITE_TUNNEL_HEADERS:
        DCHECK_EQ(OK, result);
        result = DoWriteTunnelHeaders();
        break;
      case STATE_WRITE_TUNNEL_HEADERS_COMPLETE:
        result = DoWriteTunnelHeadersComplete(result);
        break;
      case STATE_READ_TUNNEL_HEADERS:
        DCHECK_EQ(OK, result);
        result = DoReadTunnelHeaders();
        break;
      case STATE_READ_TUNNEL_HEADERS_COMPLETE:
        result = DoReadTunnelHeadersComplete(result);
        break;
      case STATE_SOCKS_CONNECT:
        DCHECK_EQ(OK, result);
        result = DoSOCKSConnect();
        break;
      case STATE_SOCKS_CONNECT_COMPLETE:
        result = DoSOCKSConnectComplete(result);
        break;
      case STATE_SSL_CONNECT:
        DCHECK_EQ(OK, result);
        result = DoSSLConnect();
        break;
      case STATE_SSL_CONNECT_COMPLETE:
        result = DoSSLConnectComplete(result);
        break;
      case STATE_READ_WRITE:
        result = DoReadWrite(result);
        break;
      case STATE_AUTH_REQUIRED:
        // It might be called when DoClose is called while waiting in
        // STATE_AUTH_REQUIRED.
        Finish(result);
        return;
      case STATE_CLOSE:
        DCHECK_LE(result, OK);
        Finish(result);
        return;
      default:
        NOTREACHED() << "bad state " << state;
        Finish(result);
        return;
    }
    // If the connection is not established yet and had actual errors,
    // close the connection.
    if (state != STATE_READ_WRITE && result < ERR_IO_PENDING) {
      DCHECK_EQ(next_state_, STATE_CLOSE);
      net_log_.EndEventWithNetErrorCode(
          NetLog::TYPE_SOCKET_STREAM_CONNECT, result);
    }
  } while (result != ERR_IO_PENDING);
}

int SocketStream::DoResolveProxy() {
  DCHECK(!pac_request_);
  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;

  if (!proxy_url_.is_valid()) {
    next_state_ = STATE_CLOSE;
    return ERR_INVALID_ARGUMENT;
  }

  return proxy_service()->ResolveProxy(
      proxy_url_, &proxy_info_, &io_callback_, &pac_request_, net_log_);
}

int SocketStream::DoResolveProxyComplete(int result) {
  pac_request_ = NULL;
  if (result != OK) {
    LOG(ERROR) << "Failed to resolve proxy: " << result;
    if (delegate_)
      delegate_->OnError(this, result);
    proxy_info_.UseDirect();
  }
  if (proxy_info_.is_direct()) {
    // If proxy was not found for original URL (i.e. websocket URL),
    // try again with https URL, like Safari implementation.
    // Note that we don't want to use http proxy, because we'll use tunnel
    // proxy using CONNECT method, which is used by https proxy.
    if (!proxy_url_.SchemeIs("https")) {
      const std::string scheme = "https";
      GURL::Replacements repl;
      repl.SetSchemeStr(scheme);
      proxy_url_ = url_.ReplaceComponents(repl);
      DVLOG(1) << "Try https proxy: " << proxy_url_;
      next_state_ = STATE_RESOLVE_PROXY;
      return OK;
    }
  }

  if (proxy_info_.is_empty()) {
    // No proxies/direct to choose from. This happens when we don't support any
    // of the proxies in the returned list.
    return ERR_NO_SUPPORTED_PROXIES;
  }

  next_state_ = STATE_RESOLVE_HOST;
  return OK;
}

int SocketStream::DoResolveHost() {
  next_state_ = STATE_RESOLVE_HOST_COMPLETE;

  DCHECK(!proxy_info_.is_empty());
  if (proxy_info_.is_direct())
    proxy_mode_ = kDirectConnection;
  else if (proxy_info_.proxy_server().is_socks())
    proxy_mode_ = kSOCKSProxy;
  else
    proxy_mode_ = kTunnelProxy;

  // Determine the host and port to connect to.
  HostPortPair host_port_pair;
  if (proxy_mode_ != kDirectConnection) {
    host_port_pair = proxy_info_.proxy_server().host_port_pair();
  } else {
    host_port_pair = HostPortPair::FromURL(url_);
  }

  HostResolver::RequestInfo resolve_info(host_port_pair);

  DCHECK(host_resolver_);
  resolver_.reset(new SingleRequestHostResolver(host_resolver_));
  return resolver_->Resolve(resolve_info, &addresses_, &io_callback_,
                            net_log_);
}

int SocketStream::DoResolveHostComplete(int result) {
  if (result == OK && delegate_) {
    next_state_ = STATE_TCP_CONNECT;
    result = delegate_->OnStartOpenConnection(this, &io_callback_);
    if (result == ERR_IO_PENDING)
      metrics_->OnWaitConnection();
  } else {
    next_state_ = STATE_CLOSE;
  }
  // TODO(ukai): if error occured, reconsider proxy after error.
  return result;
}

int SocketStream::DoTcpConnect(int result) {
  if (result != OK) {
    next_state_ = STATE_CLOSE;
    return result;
  }
  next_state_ = STATE_TCP_CONNECT_COMPLETE;
  DCHECK(factory_);
  socket_.reset(factory_->CreateTCPClientSocket(addresses_,
                                                net_log_.net_log(),
                                                net_log_.source()));
  metrics_->OnStartConnection();
  return socket_->Connect(&io_callback_);
}

int SocketStream::DoTcpConnectComplete(int result) {
  // TODO(ukai): if error occured, reconsider proxy after error.
  if (result != OK) {
    next_state_ = STATE_CLOSE;
    return result;
  }

  if (proxy_mode_ == kTunnelProxy)
    next_state_ = STATE_WRITE_TUNNEL_HEADERS;
  else if (proxy_mode_ == kSOCKSProxy)
    next_state_ = STATE_SOCKS_CONNECT;
  else if (is_secure()) {
    next_state_ = STATE_SSL_CONNECT;
  } else {
    result = DidEstablishConnection();
  }
  return result;
}

int SocketStream::DoWriteTunnelHeaders() {
  DCHECK_EQ(kTunnelProxy, proxy_mode_);

  next_state_ = STATE_WRITE_TUNNEL_HEADERS_COMPLETE;

  if (!tunnel_request_headers_.get()) {
    metrics_->OnTunnelProxy();
    tunnel_request_headers_ = new RequestHeaders();
    tunnel_request_headers_bytes_sent_ = 0;
  }
  if (tunnel_request_headers_->headers_.empty()) {
    std::string authorization_headers;

    if (!auth_handler_.get()) {
      // Do preemptive authentication.
      HttpAuthCache::Entry* entry = auth_cache_.LookupByPath(
          ProxyAuthOrigin(), std::string());
      if (entry) {
        scoped_ptr<HttpAuthHandler> handler_preemptive;
        int rv_create = http_auth_handler_factory_->
            CreatePreemptiveAuthHandlerFromString(
                entry->auth_challenge(), HttpAuth::AUTH_PROXY,
                ProxyAuthOrigin(), entry->IncrementNonceCount(),
                net_log_, &handler_preemptive);
        if (rv_create == OK) {
          auth_identity_.source = HttpAuth::IDENT_SRC_PATH_LOOKUP;
          auth_identity_.invalid = false;
          auth_identity_.username = entry->username();
          auth_identity_.password = entry->password();
          auth_handler_.swap(handler_preemptive);
        }
      }
    }

    // Support basic authentication scheme only, because we don't have
    // HttpRequestInfo.
    // TODO(ukai): Add support other authentication scheme.
    if (auth_handler_.get() &&
        auth_handler_->auth_scheme() == HttpAuth::AUTH_SCHEME_BASIC) {
      HttpRequestInfo request_info;
      std::string auth_token;
      int rv = auth_handler_->GenerateAuthToken(
          &auth_identity_.username,
          &auth_identity_.password,
          &request_info,
          NULL,
          &auth_token);
      // TODO(cbentzel): Support async auth handlers.
      DCHECK_NE(ERR_IO_PENDING, rv);
      if (rv != OK)
        return rv;
      authorization_headers.append(
          HttpAuth::GetAuthorizationHeaderName(HttpAuth::AUTH_PROXY) +
          ": " + auth_token + "\r\n");
    }

    tunnel_request_headers_->headers_ = base::StringPrintf(
        "CONNECT %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Proxy-Connection: keep-alive\r\n",
        GetHostAndPort(url_).c_str(),
        GetHostAndOptionalPort(url_).c_str());
    if (!authorization_headers.empty())
      tunnel_request_headers_->headers_ += authorization_headers;
    tunnel_request_headers_->headers_ += "\r\n";
  }
  tunnel_request_headers_->SetDataOffset(tunnel_request_headers_bytes_sent_);
  int buf_len = static_cast<int>(tunnel_request_headers_->headers_.size() -
                                 tunnel_request_headers_bytes_sent_);
  DCHECK_GT(buf_len, 0);
  return socket_->Write(tunnel_request_headers_, buf_len, &io_callback_);
}

int SocketStream::DoWriteTunnelHeadersComplete(int result) {
  DCHECK_EQ(kTunnelProxy, proxy_mode_);

  if (result < 0) {
    next_state_ = STATE_CLOSE;
    return result;
  }

  tunnel_request_headers_bytes_sent_ += result;
  if (tunnel_request_headers_bytes_sent_ <
      tunnel_request_headers_->headers_.size())
    next_state_ = STATE_WRITE_TUNNEL_HEADERS;
  else
    next_state_ = STATE_READ_TUNNEL_HEADERS;
  return OK;
}

int SocketStream::DoReadTunnelHeaders() {
  DCHECK_EQ(kTunnelProxy, proxy_mode_);

  next_state_ = STATE_READ_TUNNEL_HEADERS_COMPLETE;

  if (!tunnel_response_headers_.get()) {
    tunnel_response_headers_ = new ResponseHeaders();
    tunnel_response_headers_capacity_ = kMaxTunnelResponseHeadersSize;
    tunnel_response_headers_->Realloc(tunnel_response_headers_capacity_);
    tunnel_response_headers_len_ = 0;
  }

  int buf_len = tunnel_response_headers_capacity_ -
      tunnel_response_headers_len_;
  tunnel_response_headers_->SetDataOffset(tunnel_response_headers_len_);
  CHECK(tunnel_response_headers_->data());

  return socket_->Read(tunnel_response_headers_, buf_len, &io_callback_);
}

int SocketStream::DoReadTunnelHeadersComplete(int result) {
  DCHECK_EQ(kTunnelProxy, proxy_mode_);

  if (result < 0) {
    next_state_ = STATE_CLOSE;
    return result;
  }

  if (result == 0) {
    // 0 indicates end-of-file, so socket was closed.
    next_state_ = STATE_CLOSE;
    return ERR_CONNECTION_CLOSED;
  }

  tunnel_response_headers_len_ += result;
  DCHECK(tunnel_response_headers_len_ <= tunnel_response_headers_capacity_);

  int eoh = HttpUtil::LocateEndOfHeaders(
      tunnel_response_headers_->headers(), tunnel_response_headers_len_, 0);
  if (eoh == -1) {
    if (tunnel_response_headers_len_ >= kMaxTunnelResponseHeadersSize) {
      next_state_ = STATE_CLOSE;
      return ERR_RESPONSE_HEADERS_TOO_BIG;
    }

    next_state_ = STATE_READ_TUNNEL_HEADERS;
    return OK;
  }
  // DidReadResponseHeaders
  scoped_refptr<HttpResponseHeaders> headers;
  headers = new HttpResponseHeaders(
      HttpUtil::AssembleRawHeaders(tunnel_response_headers_->headers(), eoh));
  if (headers->GetParsedHttpVersion() < HttpVersion(1, 0)) {
    // Require the "HTTP/1.x" status line.
    next_state_ = STATE_CLOSE;
    return ERR_TUNNEL_CONNECTION_FAILED;
  }
  switch (headers->response_code()) {
    case 200:  // OK
      if (is_secure()) {
        DCHECK_EQ(eoh, tunnel_response_headers_len_);
        next_state_ = STATE_SSL_CONNECT;
      } else {
        result = DidEstablishConnection();
        if (result < 0) {
          next_state_ = STATE_CLOSE;
          return result;
        }
        if ((eoh < tunnel_response_headers_len_) && delegate_)
          delegate_->OnReceivedData(
              this, tunnel_response_headers_->headers() + eoh,
              tunnel_response_headers_len_ - eoh);
      }
      return OK;
    case 407:  // Proxy Authentication Required.
      result = HandleAuthChallenge(headers.get());
      if (result == ERR_PROXY_AUTH_UNSUPPORTED &&
          auth_handler_.get() && delegate_) {
        DCHECK(!proxy_info_.is_empty());
        auth_info_ = new AuthChallengeInfo;
        auth_info_->is_proxy = true;
        auth_info_->host_and_port =
            ASCIIToWide(proxy_info_.proxy_server().host_port_pair().ToString());
        auth_info_->scheme = ASCIIToWide(
            HttpAuth::SchemeToString(auth_handler_->auth_scheme()));
        auth_info_->realm = ASCIIToWide(auth_handler_->realm());
        // Wait until RestartWithAuth or Close is called.
        MessageLoop::current()->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &SocketStream::DoAuthRequired));
        next_state_ = STATE_AUTH_REQUIRED;
        return ERR_IO_PENDING;
      }
    default:
      break;
  }
  next_state_ = STATE_CLOSE;
  return ERR_TUNNEL_CONNECTION_FAILED;
}

int SocketStream::DoSOCKSConnect() {
  DCHECK_EQ(kSOCKSProxy, proxy_mode_);

  next_state_ = STATE_SOCKS_CONNECT_COMPLETE;

  ClientSocket* s = socket_.release();
  HostResolver::RequestInfo req_info(HostPortPair::FromURL(url_));

  DCHECK(!proxy_info_.is_empty());
  if (proxy_info_.proxy_server().scheme() == ProxyServer::SCHEME_SOCKS5)
    s = new SOCKS5ClientSocket(s, req_info);
  else
    s = new SOCKSClientSocket(s, req_info, host_resolver_);
  socket_.reset(s);
  metrics_->OnSOCKSProxy();
  return socket_->Connect(&io_callback_);
}

int SocketStream::DoSOCKSConnectComplete(int result) {
  DCHECK_EQ(kSOCKSProxy, proxy_mode_);

  if (result == OK) {
    if (is_secure())
      next_state_ = STATE_SSL_CONNECT;
    else
      result = DidEstablishConnection();
  } else {
    next_state_ = STATE_CLOSE;
  }
  return result;
}

int SocketStream::DoSSLConnect() {
  DCHECK(factory_);
  // TODO(agl): look into plumbing SSLHostInfo here.
  socket_.reset(factory_->CreateSSLClientSocket(socket_.release(),
                                                HostPortPair::FromURL(url_),
                                                ssl_config_,
                                                NULL /* ssl_host_info */,
                                                cert_verifier_));
  next_state_ = STATE_SSL_CONNECT_COMPLETE;
  metrics_->OnSSLConnection();
  return socket_->Connect(&io_callback_);
}

int SocketStream::DoSSLConnectComplete(int result) {
  if (IsCertificateError(result)) {
    if (socket_->IsConnectedAndIdle()) {
      result = HandleCertificateError(result);
    } else {
      // SSLClientSocket for Mac will report socket is not connected,
      // if it returns cert verification error.  It didn't perform
      // SSLHandshake yet.
      // So, we should restart establishing connection with the
      // certificate in allowed bad certificates in |ssl_config_|.
      // See also net/http/http_network_transaction.cc
      //  HandleCertificateError() and RestartIgnoringLastError().
      SSLClientSocket* ssl_socket =
        reinterpret_cast<SSLClientSocket*>(socket_.get());
      SSLInfo ssl_info;
      ssl_socket->GetSSLInfo(&ssl_info);
      if (ssl_config_.IsAllowedBadCert(ssl_info.cert)) {
        // If we already have the certificate in the set of allowed bad
        // certificates, we did try it and failed again, so we should not
        // retry again: the connection should fail at last.
        next_state_ = STATE_CLOSE;
        return result;
      }
      // Add the bad certificate to the set of allowed certificates in the
      // SSL config object.
      SSLConfig::CertAndStatus bad_cert;
      bad_cert.cert = ssl_info.cert;
      bad_cert.cert_status = ssl_info.cert_status;
      ssl_config_.allowed_bad_certs.push_back(bad_cert);
      // Restart connection ignoring the bad certificate.
      socket_->Disconnect();
      socket_.reset();
      next_state_ = STATE_TCP_CONNECT;
      return OK;
    }
  }

  if (result == OK)
    result = DidEstablishConnection();
  else
    next_state_ = STATE_CLOSE;
  return result;
}

int SocketStream::DoReadWrite(int result) {
  if (result < OK) {
    next_state_ = STATE_CLOSE;
    return result;
  }
  if (!socket_.get() || !socket_->IsConnected()) {
    next_state_ = STATE_CLOSE;
    return ERR_CONNECTION_CLOSED;
  }

  // If client has requested close(), and there's nothing to write, then
  // let's close the socket.
  // We don't care about receiving data after the socket is closed.
  if (closing_ && !write_buf_ && pending_write_bufs_.empty()) {
    socket_->Disconnect();
    next_state_ = STATE_CLOSE;
    return OK;
  }

  next_state_ = STATE_READ_WRITE;

  // If server already closed the socket, we don't try to read.
  if (!server_closed_) {
    if (!read_buf_) {
      // No read pending and server didn't close the socket.
      read_buf_ = new IOBuffer(kReadBufferSize);
      result = socket_->Read(read_buf_, kReadBufferSize, &read_callback_);
      if (result > 0) {
        return DidReceiveData(result);
      } else if (result == 0) {
        // 0 indicates end-of-file, so socket was closed.
        next_state_ = STATE_CLOSE;
        server_closed_ = true;
        return ERR_CONNECTION_CLOSED;
      }
      // If read is pending, try write as well.
      // Otherwise, return the result and do next loop (to close the
      // connection).
      if (result != ERR_IO_PENDING) {
        next_state_ = STATE_CLOSE;
        server_closed_ = true;
        return result;
      }
    }
    // Read is pending.
    DCHECK(read_buf_);
  }

  if (write_buf_ && !current_write_buf_) {
    // No write pending.
    current_write_buf_ = new DrainableIOBuffer(write_buf_, write_buf_size_);
    current_write_buf_->SetOffset(write_buf_offset_);
    result = socket_->Write(current_write_buf_,
                            current_write_buf_->BytesRemaining(),
                            &write_callback_);
    if (result > 0) {
      return DidSendData(result);
    }
    // If write is not pending, return the result and do next loop (to close
    // the connection).
    if (result != 0 && result != ERR_IO_PENDING) {
      next_state_ = STATE_CLOSE;
      return result;
    }
    return result;
  }

  // We arrived here when both operation is pending.
  return ERR_IO_PENDING;
}

GURL SocketStream::ProxyAuthOrigin() const {
  DCHECK(!proxy_info_.is_empty());
  return GURL("http://" +
              proxy_info_.proxy_server().host_port_pair().ToString());
}

int SocketStream::HandleAuthChallenge(const HttpResponseHeaders* headers) {
  GURL auth_origin(ProxyAuthOrigin());

  VLOG(1) << "The proxy " << auth_origin << " requested auth";

  // TODO(cbentzel): Since SocketStream only suppports basic authentication
  // right now, another challenge is always treated as a rejection.
  // Ultimately this should be converted to use HttpAuthController like the
  // HttpNetworkTransaction has.
  if (auth_handler_.get() && !auth_identity_.invalid) {
    if (auth_identity_.source != HttpAuth::IDENT_SRC_PATH_LOOKUP)
      auth_cache_.Remove(auth_origin,
                         auth_handler_->realm(),
                         auth_handler_->auth_scheme(),
                         auth_identity_.username,
                         auth_identity_.password);
    auth_handler_.reset();
    auth_identity_ = HttpAuth::Identity();
  }

  auth_identity_.invalid = true;
  std::set<HttpAuth::Scheme> disabled_schemes;
  HttpAuth::ChooseBestChallenge(http_auth_handler_factory_, headers,
                                HttpAuth::AUTH_PROXY,
                                auth_origin, disabled_schemes,
                                net_log_, &auth_handler_);
  if (!auth_handler_.get()) {
    LOG(ERROR) << "Can't perform auth to the proxy " << auth_origin;
    return ERR_TUNNEL_CONNECTION_FAILED;
  }
  if (auth_handler_->NeedsIdentity()) {
    // We only support basic authentication scheme now.
    // TODO(ukai): Support other authentication scheme.
    HttpAuthCache::Entry* entry = auth_cache_.Lookup(
        auth_origin, auth_handler_->realm(), HttpAuth::AUTH_SCHEME_BASIC);
    if (entry) {
      auth_identity_.source = HttpAuth::IDENT_SRC_REALM_LOOKUP;
      auth_identity_.invalid = false;
      auth_identity_.username = entry->username();
      auth_identity_.password = entry->password();
      // Restart with auth info.
    }
    return ERR_PROXY_AUTH_UNSUPPORTED;
  } else {
    auth_identity_.invalid = false;
  }
  return ERR_TUNNEL_CONNECTION_FAILED;
}

void SocketStream::DoAuthRequired() {
  if (delegate_ && auth_info_.get())
    delegate_->OnAuthRequired(this, auth_info_.get());
  else
    DoLoop(ERR_UNEXPECTED);
}

void SocketStream::DoRestartWithAuth() {
  DCHECK_EQ(next_state_, STATE_AUTH_REQUIRED);
  auth_cache_.Add(ProxyAuthOrigin(),
                  auth_handler_->realm(),
                  auth_handler_->auth_scheme(),
                  auth_handler_->challenge(),
                  auth_identity_.username,
                  auth_identity_.password,
                  std::string());

  tunnel_request_headers_ = NULL;
  tunnel_request_headers_bytes_sent_ = 0;
  tunnel_response_headers_ = NULL;
  tunnel_response_headers_capacity_ = 0;
  tunnel_response_headers_len_ = 0;

  next_state_ = STATE_TCP_CONNECT;
  DoLoop(OK);
}

int SocketStream::HandleCertificateError(int result) {
  // TODO(ukai): handle cert error properly.
  switch (result) {
    case ERR_CERT_COMMON_NAME_INVALID:
    case ERR_CERT_DATE_INVALID:
    case ERR_CERT_AUTHORITY_INVALID:
      result = OK;
      break;
    default:
      break;
  }
  return result;
}

SSLConfigService* SocketStream::ssl_config_service() const {
  return context_->ssl_config_service();
}

ProxyService* SocketStream::proxy_service() const {
  return context_->proxy_service();
}

}  // namespace net
