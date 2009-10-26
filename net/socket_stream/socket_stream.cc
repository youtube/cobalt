// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(ukai): code is similar with http_network_transaction.cc.  We should
//   think about ways to share code, if possible.

#include "net/socket_stream/socket_stream.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/socks5_client_socket.h"
#include "net/socket/socks_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/url_request/url_request.h"

static const int kMaxPendingSendAllowed = 32768;  // 32 kilobytes.
static const int kReadBufferSize = 4096;

namespace net {

void SocketStream::ResponseHeaders::Realloc(size_t new_size) {
  headers_.reset(static_cast<char*>(realloc(headers_.release(), new_size)));
}

SocketStream::SocketStream(const GURL& url, Delegate* delegate)
    : url_(url),
      delegate_(delegate),
      max_pending_send_allowed_(kMaxPendingSendAllowed),
      next_state_(STATE_NONE),
      host_resolver_(CreateSystemHostResolver()),
      factory_(ClientSocketFactory::GetDefaultFactory()),
      proxy_mode_(kDirectConnection),
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
      write_buf_size_(0) {
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
  DCHECK(delegate_);
}

SocketStream::~SocketStream() {
  DCHECK(!delegate_);
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

void SocketStream::set_context(URLRequestContext* context) {
  context_ = context;
}

void SocketStream::Connect() {
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
  ssl_config_service()->GetSSLConfig(&ssl_config_);

  AddRef();  // Released in Finish()
  // Open a connection asynchronously, so that delegate won't be called
  // back before returning Connect().
  next_state_ = STATE_RESOLVE_PROXY;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SocketStream::DoLoop, OK));
}

bool SocketStream::SendData(const char* data, int len) {
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
  if (!socket_.get() || !socket_->IsConnected())
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

    pending_write_bufs_.push_back(new IOBufferWithSize(len));
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
  if (!socket_.get())
    return;
  if (socket_->IsConnected())
    socket_->Disconnect();
  // Close asynchronously, so that delegate won't be called
  // back before returning Close().
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SocketStream::DoLoop, OK));
}

void SocketStream::DetachDelegate() {
  if (!delegate_)
    return;
  delegate_ = NULL;
  Close();
}

void SocketStream::Finish() {
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
  Delegate* delegate = delegate_;
  delegate_ = NULL;
  if (delegate) {
    delegate->OnClose(this);
    Release();
  }
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

void SocketStream::DidEstablishConnection() {
  if (!socket_.get() || !socket_->IsConnected()) {
    Finish();
    return;
  }
  next_state_ = STATE_READ_WRITE;

  if (delegate_)
    delegate_->OnConnected(this, max_pending_send_allowed_);

  return;
}

void SocketStream::DidReceiveData(int result) {
  DCHECK(read_buf_);
  DCHECK_GT(result, 0);
  if (!delegate_)
    return;
  // Notify recevied data to delegate.
  delegate_->OnReceivedData(this, read_buf_->data(), result);
  read_buf_ = NULL;
}

void SocketStream::DidSendData(int result) {
  current_write_buf_ = NULL;
  DCHECK(result > 0);
  if (!delegate_)
    return;

  delegate_->OnSentData(this, result);
  int remaining_size = write_buf_size_ - write_buf_offset_ - result;
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
    write_buf_offset_ += result;
  }
}

void SocketStream::OnIOCompleted(int result) {
  DoLoop(result);
  // TODO(ukai): notify error.
}

void SocketStream::OnReadCompleted(int result) {
  // TODO(ukai): notify error.
  if (result == 0) {
    // 0 indicates end-of-file, so socket was closed.
    next_state_ = STATE_NONE;
  } else if (result > 0 && read_buf_) {
    DidReceiveData(result);
    result = OK;
  }
  DoLoop(result);
}

void SocketStream::OnWriteCompleted(int result) {
  // TODO(ukai): notify error.
  if (result >= 0 && write_buf_) {
    DidSendData(result);
    result = OK;
  }
  DoLoop(result);
}

int SocketStream::DoLoop(int result) {
  if (next_state_ == STATE_NONE) {
    Finish();
    return ERR_CONNECTION_CLOSED;
  }

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
        DCHECK_EQ(OK, result);
        result = DoTcpConnect();
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
      default:
        NOTREACHED() << "bad state";
        result = ERR_UNEXPECTED;
        break;
    }
  } while (result != ERR_IO_PENDING && next_state_ != STATE_NONE);

  if (result != ERR_IO_PENDING)
    Finish();

  return result;
}

int SocketStream::DoResolveProxy() {
  DCHECK(!pac_request_);
  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;

  return proxy_service()->ResolveProxy(
      url_, &proxy_info_, &io_callback_, &pac_request_, NULL);
}

int SocketStream::DoResolveProxyComplete(int result) {
  next_state_ = STATE_RESOLVE_HOST;

  pac_request_ = NULL;
  if (result != OK) {
    LOG(ERROR) << "Failed to resolve proxy: " << result;
    proxy_info_.UseDirect();
  }

  return OK;
}

int SocketStream::DoResolveHost() {
  next_state_ = STATE_RESOLVE_HOST_COMPLETE;

  if (proxy_info_.is_direct())
    proxy_mode_ = kDirectConnection;
  else if (proxy_info_.proxy_server().is_socks())
    proxy_mode_ = kSOCKSProxy;
  else
    proxy_mode_ = kTunnelProxy;

  // Determine the host and port to connect to.
  std::string host;
  int port;
  if (proxy_mode_ != kDirectConnection) {
    ProxyServer proxy_server = proxy_info_.proxy_server();
    host = proxy_server.HostNoBrackets();
    port = proxy_server.port();
  } else {
    host = url_.HostNoBrackets();
    port = url_.EffectiveIntPort();
  }

  HostResolver::RequestInfo resolve_info(host, port);

  resolver_.reset(new SingleRequestHostResolver(host_resolver_.get()));
  return resolver_->Resolve(resolve_info, &addresses_, &io_callback_, NULL);
}

int SocketStream::DoResolveHostComplete(int result) {
  if (result == OK)
    next_state_ = STATE_TCP_CONNECT;
  return result;
}

int SocketStream::DoTcpConnect() {
  next_state_ = STATE_TCP_CONNECT_COMPLETE;
  DCHECK(factory_);
  socket_.reset(factory_->CreateTCPClientSocket(addresses_));
  return socket_->Connect(&io_callback_);
}

int SocketStream::DoTcpConnectComplete(int result) {
  if (result != OK)
    return result;

  if (proxy_mode_ == kTunnelProxy)
    next_state_ = STATE_WRITE_TUNNEL_HEADERS;
  else if (proxy_mode_ == kSOCKSProxy)
    next_state_ = STATE_SOCKS_CONNECT;
  else if (is_secure()) {
    next_state_ = STATE_SSL_CONNECT;
  } else {
    DidEstablishConnection();
  }
  return OK;
}

int SocketStream::DoWriteTunnelHeaders() {
  DCHECK_EQ(kTunnelProxy, proxy_mode_);

  next_state_ = STATE_WRITE_TUNNEL_HEADERS_COMPLETE;

  if (!tunnel_request_headers_.get()) {
    tunnel_request_headers_ = new RequestHeaders();
    tunnel_request_headers_bytes_sent_ = 0;
  }
  if (tunnel_request_headers_->headers_.empty()) {
    tunnel_request_headers_->headers_ = StringPrintf(
        "CONNECT %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Proxy-Connection: keep-alive\r\n",
        GetHostAndPort(url_).c_str(),
        GetHostAndOptionalPort(url_).c_str());
    // TODO(ukai): set proxy auth if necessary.
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

  if (result < 0)
    return result;

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

  if (result < 0)
    return result;

  if (result == 0) {
    // 0 indicates end-of-file, so socket was closed.
    Finish();
    return result;
  }

  tunnel_response_headers_len_ += result;
  DCHECK(tunnel_response_headers_len_ <= tunnel_response_headers_capacity_);

  int eoh = HttpUtil::LocateEndOfHeaders(
      tunnel_response_headers_->headers(), tunnel_response_headers_len_, 0);
  if (eoh == -1) {
    if (tunnel_response_headers_len_ >= kMaxTunnelResponseHeadersSize)
      return ERR_RESPONSE_HEADERS_TOO_BIG;

    next_state_ = STATE_READ_TUNNEL_HEADERS;
    return OK;
  }
  // DidReadResponseHeaders
  scoped_refptr<HttpResponseHeaders> headers;
  headers = new HttpResponseHeaders(
      HttpUtil::AssembleRawHeaders(tunnel_response_headers_->headers(), eoh));
  if (headers->GetParsedHttpVersion() < HttpVersion(1, 0)) {
    // Require the "HTTP/1.x" status line.
    return ERR_TUNNEL_CONNECTION_FAILED;
  }
  switch (headers->response_code()) {
    case 200:  // OK
      if (is_secure()) {
        DCHECK_EQ(eoh, tunnel_response_headers_len_);
        next_state_ = STATE_SSL_CONNECT;
      } else {
        DidEstablishConnection();
        if ((eoh < tunnel_response_headers_len_) && delegate_)
          delegate_->OnReceivedData(
              this, tunnel_response_headers_->headers() + eoh,
              tunnel_response_headers_len_ - eoh);
      }
      return OK;
    case 407:  // Proxy Authentication Required.
      // TODO(ukai): handle Proxy Authentication.
      break;
    default:
      break;
  }
  return ERR_TUNNEL_CONNECTION_FAILED;
}

int SocketStream::DoSOCKSConnect() {
  DCHECK_EQ(kSOCKSProxy, proxy_mode_);

  next_state_ = STATE_SOCKS_CONNECT_COMPLETE;

  ClientSocket* s = socket_.release();
  HostResolver::RequestInfo req_info(url_.HostNoBrackets(),
                                     url_.EffectiveIntPort());

  if (proxy_info_.proxy_server().scheme() == ProxyServer::SCHEME_SOCKS5)
    s = new SOCKS5ClientSocket(s, req_info, host_resolver_.get());
  else
    s = new SOCKSClientSocket(s, req_info, host_resolver_.get());
  socket_.reset(s);
  return socket_->Connect(&io_callback_);
}

int SocketStream::DoSOCKSConnectComplete(int result) {
  DCHECK_EQ(kSOCKSProxy, proxy_mode_);

  if (result == OK) {
    if (is_secure())
      next_state_ = STATE_SSL_CONNECT;
    else
      DidEstablishConnection();
  }
  return result;
}

int SocketStream::DoSSLConnect() {
  DCHECK(factory_);
  socket_.reset(factory_->CreateSSLClientSocket(
      socket_.release(), url_.HostNoBrackets(), ssl_config_));
  next_state_ = STATE_SSL_CONNECT_COMPLETE;
  return socket_->Connect(&io_callback_);
}

int SocketStream::DoSSLConnectComplete(int result) {
  if (IsCertificateError(result))
    result = HandleCertificateError(result);

  if (result == OK)
    DidEstablishConnection();
  return result;
}

int SocketStream::DoReadWrite(int result) {
  if (result < OK) {
    Finish();
    return result;
  }
  if (!socket_.get() || !socket_->IsConnected()) {
    Finish();
    return ERR_CONNECTION_CLOSED;
  }

  next_state_ = STATE_READ_WRITE;

  if (!read_buf_) {
    // No read pending.
    read_buf_ = new IOBuffer(kReadBufferSize);
    result = socket_->Read(read_buf_, kReadBufferSize, &read_callback_);
    if (result > 0) {
      DidReceiveData(result);
      return OK;
    } else if (result == 0) {
      // 0 indicates end-of-file, so socket was closed.
      Finish();
      return ERR_CONNECTION_CLOSED;
    }
    // If read is pending, try write as well.
    // Otherwise, return the result and do next loop.
    if (result != ERR_IO_PENDING)
      return result;
  }
  // Read is pending.
  DCHECK(read_buf_);

  if (write_buf_ && !current_write_buf_) {
    // No write pending.
    current_write_buf_ = new DrainableIOBuffer(write_buf_, write_buf_size_);
    current_write_buf_->SetOffset(write_buf_offset_);
    result = socket_->Write(current_write_buf_,
                            current_write_buf_->BytesRemaining(),
                            &write_callback_);
    if (result > 0) {
      DidSendData(result);
      return OK;
    }
    return result;
  }

  // We arrived here when both operation is pending.
  return ERR_IO_PENDING;
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

bool SocketStream::is_secure() const {
  return url_.SchemeIs("wss");
}

SSLConfigService* SocketStream::ssl_config_service() const {
  return context_->ssl_config_service();
}

ProxyService* SocketStream::proxy_service() const {
  return context_->proxy_service();
}

}  // namespace net
