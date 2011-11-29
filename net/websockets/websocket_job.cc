// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_job.h"

#include <algorithm>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/string_tokenizer.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/cookie_store.h"
#include "net/base/io_buffer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_frame_handler.h"
#include "net/websockets/websocket_handshake_handler.h"
#include "net/websockets/websocket_net_log_params.h"
#include "net/websockets/websocket_throttle.h"

static const int kMaxPendingSendAllowed = 32768;  // 32 kilobytes.

namespace {

// lower-case header names.
const char* const kCookieHeaders[] = {
  "cookie", "cookie2"
};
const char* const kSetCookieHeaders[] = {
  "set-cookie", "set-cookie2"
};

net::SocketStreamJob* WebSocketJobFactory(
    const GURL& url, net::SocketStream::Delegate* delegate) {
  net::WebSocketJob* job = new net::WebSocketJob(delegate);
  job->InitSocketStream(new net::SocketStream(url, job));
  return job;
}

class WebSocketJobInitSingleton {
 private:
  friend struct base::DefaultLazyInstanceTraits<WebSocketJobInitSingleton>;
  WebSocketJobInitSingleton() {
    net::SocketStreamJob::RegisterProtocolFactory("ws", WebSocketJobFactory);
    net::SocketStreamJob::RegisterProtocolFactory("wss", WebSocketJobFactory);
  }
};

static base::LazyInstance<WebSocketJobInitSingleton> g_websocket_job_init =
    LAZY_INSTANCE_INITIALIZER;

}  // anonymous namespace

namespace net {

bool WebSocketJob::websocket_over_spdy_enabled_ = false;

// static
void WebSocketJob::EnsureInit() {
  g_websocket_job_init.Get();
}

// static
void WebSocketJob::set_websocket_over_spdy_enabled(bool enabled) {
  websocket_over_spdy_enabled_ = enabled;
}

WebSocketJob::WebSocketJob(SocketStream::Delegate* delegate)
    : delegate_(delegate),
      state_(INITIALIZED),
      waiting_(false),
      callback_(NULL),
      handshake_request_(new WebSocketHandshakeRequestHandler),
      handshake_response_(new WebSocketHandshakeResponseHandler),
      started_to_send_handshake_request_(false),
      handshake_request_sent_(0),
      response_cookies_save_index_(0),
      send_frame_handler_(new WebSocketFrameHandler),
      receive_frame_handler_(new WebSocketFrameHandler),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_for_send_pending_(this)) {
}

WebSocketJob::~WebSocketJob() {
  DCHECK_EQ(CLOSED, state_);
  DCHECK(!delegate_);
  DCHECK(!socket_.get());
}

void WebSocketJob::Connect() {
  DCHECK(socket_.get());
  DCHECK_EQ(state_, INITIALIZED);
  state_ = CONNECTING;
  socket_->Connect();
}

bool WebSocketJob::SendData(const char* data, int len) {
  switch (state_) {
    case INITIALIZED:
      return false;

    case CONNECTING:
      return SendHandshakeRequest(data, len);

    case OPEN:
      {
        send_frame_handler_->AppendData(data, len);
        // If current buffer is sending now, this data will be sent in
        // SendPending() after current data was sent.
        // Do not buffer sending data for now.  Since
        // WebCore::SocketStreamHandle controls traffic to keep number of
        // pending bytes less than max_pending_send_allowed, so when sending
        // larger message than max_pending_send_allowed should not be buffered.
        // If we don't call OnSentData, WebCore::SocketStreamHandle would stop
        // sending more data when pending data reaches
        // max_pending_send_allowed.
        // TODO(ukai): Fix this to support compression for larger message.
        int err = 0;
        if (!send_frame_handler_->GetCurrentBuffer() &&
            (err = send_frame_handler_->UpdateCurrentBuffer(false)) > 0) {
          DCHECK(!current_buffer_);
          current_buffer_ = new DrainableIOBuffer(
              send_frame_handler_->GetCurrentBuffer(),
              send_frame_handler_->GetCurrentBufferSize());
          return SendDataInternal(
              current_buffer_->data(), current_buffer_->BytesRemaining());
        }
        return err >= 0;
      }

    case CLOSING:
    case CLOSED:
      return false;
  }
  return false;
}

void WebSocketJob::Close() {
  if (state_ == CLOSED)
    return;

  state_ = CLOSING;
  if (current_buffer_) {
    // Will close in SendPending.
    return;
  }
  state_ = CLOSED;
  CloseInternal();
}

void WebSocketJob::RestartWithAuth(const AuthCredentials& credentials) {
  state_ = CONNECTING;
  socket_->RestartWithAuth(credentials);
}

void WebSocketJob::DetachDelegate() {
  state_ = CLOSED;
  WebSocketThrottle::GetInstance()->RemoveFromQueue(this);
  WebSocketThrottle::GetInstance()->WakeupSocketIfNecessary();

  scoped_refptr<WebSocketJob> protect(this);
  weak_ptr_factory_.InvalidateWeakPtrs();
  weak_ptr_factory_for_send_pending_.InvalidateWeakPtrs();

  delegate_ = NULL;
  if (socket_)
    socket_->DetachDelegate();
  socket_ = NULL;
  if (callback_) {
    waiting_ = false;
    callback_ = NULL;
    Release();  // Balanced with OnStartOpenConnection().
  }
}

int WebSocketJob::OnStartOpenConnection(
    SocketStream* socket, OldCompletionCallback* callback) {
  DCHECK(!callback_);
  state_ = CONNECTING;
  addresses_ = socket->address_list();
  WebSocketThrottle::GetInstance()->PutInQueue(this);
  if (delegate_) {
    int result = delegate_->OnStartOpenConnection(socket, callback);
    DCHECK_EQ(OK, result);
  }
  if (waiting_) {
    // PutInQueue() may set |waiting_| true for throttling. In this case,
    // Wakeup() will be called later.
    callback_ = callback;
    AddRef();  // Balanced when callback_ becomes NULL.
    return ERR_IO_PENDING;
  }
  return TrySpdyStream();
}

void WebSocketJob::OnConnected(
    SocketStream* socket, int max_pending_send_allowed) {
  if (state_ == CLOSED)
    return;
  DCHECK_EQ(CONNECTING, state_);
  if (delegate_)
    delegate_->OnConnected(socket, max_pending_send_allowed);
}

void WebSocketJob::OnSentData(SocketStream* socket, int amount_sent) {
  DCHECK_NE(INITIALIZED, state_);
  DCHECK_GT(amount_sent, 0);
  if (state_ == CLOSED)
    return;
  if (state_ == CONNECTING) {
    OnSentHandshakeRequest(socket, amount_sent);
    return;
  }
  if (delegate_) {
    DCHECK(state_ == OPEN || state_ == CLOSING);
    if (current_buffer_ == NULL) {
      VLOG(1) << "OnSentData current_buffer=NULL amount_sent=" << amount_sent;
      return;
    }
    current_buffer_->DidConsume(amount_sent);
    if (current_buffer_->BytesRemaining() > 0)
      return;

    // We need to report amount_sent of original buffer size, instead of
    // amount sent to |socket|.
    amount_sent = send_frame_handler_->GetOriginalBufferSize();
    DCHECK_GT(amount_sent, 0);
    current_buffer_ = NULL;
    send_frame_handler_->ReleaseCurrentBuffer();
    if (!weak_ptr_factory_for_send_pending_.HasWeakPtrs()) {
      MessageLoopForIO::current()->PostTask(
          FROM_HERE,
          base::Bind(&WebSocketJob::SendPending,
                     weak_ptr_factory_for_send_pending_.GetWeakPtr()));
    }
    delegate_->OnSentData(socket, amount_sent);
  }
}

void WebSocketJob::OnReceivedData(
    SocketStream* socket, const char* data, int len) {
  DCHECK_NE(INITIALIZED, state_);
  if (state_ == CLOSED)
    return;
  if (state_ == CONNECTING) {
    OnReceivedHandshakeResponse(socket, data, len);
    return;
  }
  DCHECK(state_ == OPEN || state_ == CLOSING);
  std::string received_data;
  receive_frame_handler_->AppendData(data, len);
  // Don't buffer receiving data for now.
  // TODO(ukai): fix performance of WebSocketFrameHandler.
  while (receive_frame_handler_->UpdateCurrentBuffer(false) > 0) {
    received_data +=
        std::string(receive_frame_handler_->GetCurrentBuffer()->data(),
                    receive_frame_handler_->GetCurrentBufferSize());
    receive_frame_handler_->ReleaseCurrentBuffer();
  }
  if (delegate_ && !received_data.empty())
      delegate_->OnReceivedData(
          socket, received_data.data(), received_data.size());
}

void WebSocketJob::OnClose(SocketStream* socket) {
  state_ = CLOSED;
  WebSocketThrottle::GetInstance()->RemoveFromQueue(this);
  WebSocketThrottle::GetInstance()->WakeupSocketIfNecessary();

  scoped_refptr<WebSocketJob> protect(this);
  weak_ptr_factory_.InvalidateWeakPtrs();

  SocketStream::Delegate* delegate = delegate_;
  delegate_ = NULL;
  socket_ = NULL;
  if (callback_) {
    waiting_ = false;
    callback_ = NULL;
    Release();  // Balanced with OnStartOpenConnection().
  }
  if (delegate)
    delegate->OnClose(socket);
}

void WebSocketJob::OnAuthRequired(
    SocketStream* socket, AuthChallengeInfo* auth_info) {
  if (delegate_)
    delegate_->OnAuthRequired(socket, auth_info);
}

void WebSocketJob::OnError(const SocketStream* socket, int error) {
  if (delegate_ && error != ERR_PROTOCOL_SWITCHED)
    delegate_->OnError(socket, error);
}

void WebSocketJob::OnCreatedSpdyStream(int result) {
  DCHECK(spdy_websocket_stream_.get());
  DCHECK(socket_.get());
  DCHECK_NE(ERR_IO_PENDING, result);

  if (state_ == CLOSED) {
    result = ERR_ABORTED;
  } else if (result == OK) {
    state_ = CONNECTING;
    result = ERR_PROTOCOL_SWITCHED;
  } else {
    spdy_websocket_stream_.reset();
  }

  CompleteIO(result);
}

void WebSocketJob::OnSentSpdyHeaders(int result) {
  DCHECK_NE(INITIALIZED, state_);
  if (state_ != CONNECTING)
    return;
  if (delegate_)
    delegate_->OnSentData(socket_, handshake_request_->original_length());
  handshake_request_.reset();
}

int WebSocketJob::OnReceivedSpdyResponseHeader(
    const spdy::SpdyHeaderBlock& headers, int status) {
  DCHECK_NE(INITIALIZED, state_);
  if (state_ != CONNECTING)
    return status;
  if (status != OK)
    return status;
  // TODO(toyoshim): Fallback to non-spdy connection?
  handshake_response_->ParseResponseHeaderBlock(headers, challenge_);

  SaveCookiesAndNotifyHeaderComplete();
  return OK;
}

void WebSocketJob::OnSentSpdyData(int amount_sent) {
  DCHECK_NE(INITIALIZED, state_);
  DCHECK_NE(CONNECTING, state_);
  if (state_ == CLOSED)
    return;
  if (!spdy_websocket_stream_.get())
    return;
  OnSentData(socket_, amount_sent);
}

void WebSocketJob::OnReceivedSpdyData(const char* data, int length) {
  DCHECK_NE(INITIALIZED, state_);
  DCHECK_NE(CONNECTING, state_);
  if (state_ == CLOSED)
    return;
  if (!spdy_websocket_stream_.get())
    return;
  OnReceivedData(socket_, data, length);
}

void WebSocketJob::OnCloseSpdyStream() {
  spdy_websocket_stream_.reset();
  OnClose(socket_);
}

bool WebSocketJob::SendHandshakeRequest(const char* data, int len) {
  DCHECK_EQ(state_, CONNECTING);
  if (started_to_send_handshake_request_)
    return false;
  if (!handshake_request_->ParseRequest(data, len))
    return false;

  // handshake message is completed.
  handshake_response_->set_protocol_version(
      handshake_request_->protocol_version());
  AddCookieHeaderAndSend();
  return true;
}

void WebSocketJob::AddCookieHeaderAndSend() {
  bool allow = true;
  if (delegate_ && !delegate_->CanGetCookies(socket_, GetURLForCookies()))
    allow = false;

  if (socket_ && delegate_ && state_ == CONNECTING) {
    handshake_request_->RemoveHeaders(
        kCookieHeaders, arraysize(kCookieHeaders));
    if (allow && socket_->context()->cookie_store()) {
      // Add cookies, including HttpOnly cookies.
      CookieOptions cookie_options;
      cookie_options.set_include_httponly();
      socket_->context()->cookie_store()->GetCookiesWithOptionsAsync(
          GetURLForCookies(), cookie_options,
          base::Bind(&WebSocketJob::LoadCookieCallback,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      DoSendData();
    }
  }
}

void WebSocketJob::LoadCookieCallback(const std::string& cookie) {
  if (!cookie.empty())
    handshake_request_->AppendHeaderIfMissing("Cookie", cookie);
  DoSendData();
}

void WebSocketJob::DoSendData() {
  if (spdy_websocket_stream_.get()) {
    linked_ptr<spdy::SpdyHeaderBlock> headers(new spdy::SpdyHeaderBlock);
    handshake_request_->GetRequestHeaderBlock(
        socket_->url(), headers.get(), &challenge_);
    spdy_websocket_stream_->SendRequest(headers);
  } else {
    const std::string& handshake_request =
        handshake_request_->GetRawRequest();
    handshake_request_sent_ = 0;
    socket_->net_log()->AddEvent(
        NetLog::TYPE_WEB_SOCKET_SEND_REQUEST_HEADERS,
        make_scoped_refptr(
            new NetLogWebSocketHandshakeParameter(handshake_request)));
    socket_->SendData(handshake_request.data(),
                      handshake_request.size());
  }
  // Just buffered in |handshake_request_|.
  started_to_send_handshake_request_ = true;
}

void WebSocketJob::OnSentHandshakeRequest(
    SocketStream* socket, int amount_sent) {
  DCHECK_EQ(state_, CONNECTING);
  handshake_request_sent_ += amount_sent;
  DCHECK_LE(handshake_request_sent_, handshake_request_->raw_length());
  if (handshake_request_sent_ >= handshake_request_->raw_length()) {
    // handshake request has been sent.
    // notify original size of handshake request to delegate.
    if (delegate_)
      delegate_->OnSentData(
          socket,
          handshake_request_->original_length());
    handshake_request_.reset();
  }
}

void WebSocketJob::OnReceivedHandshakeResponse(
    SocketStream* socket, const char* data, int len) {
  DCHECK_EQ(state_, CONNECTING);
  if (handshake_response_->HasResponse()) {
    // If we already has handshake response, received data should be frame
    // data, not handshake message.
    receive_frame_handler_->AppendData(data, len);
    return;
  }

  size_t response_length = handshake_response_->ParseRawResponse(data, len);
  if (!handshake_response_->HasResponse()) {
    // not yet. we need more data.
    return;
  }
  // handshake message is completed.
  socket_->net_log()->AddEvent(
      NetLog::TYPE_WEB_SOCKET_READ_RESPONSE_HEADERS,
      make_scoped_refptr(new NetLogWebSocketHandshakeParameter(
          handshake_response_->GetRawResponse())));
  if (len - response_length > 0) {
    // If we received extra data, it should be frame data.
    receive_frame_handler_->AppendData(data + response_length,
                                       len - response_length);
  }
  SaveCookiesAndNotifyHeaderComplete();
}

void WebSocketJob::SaveCookiesAndNotifyHeaderComplete() {
  // handshake message is completed.
  DCHECK(handshake_response_->HasResponse());

  response_cookies_.clear();
  response_cookies_save_index_ = 0;

  handshake_response_->GetHeaders(
      kSetCookieHeaders, arraysize(kSetCookieHeaders), &response_cookies_);

  // Now, loop over the response cookies, and attempt to persist each.
  SaveNextCookie();
}

void WebSocketJob::SaveNextCookie() {
  if (response_cookies_save_index_ == response_cookies_.size()) {
    response_cookies_.clear();
    response_cookies_save_index_ = 0;

    // Remove cookie headers, with malformed headers preserved.
    // Actual handshake should be done in WebKit.
    handshake_response_->RemoveHeaders(
        kSetCookieHeaders, arraysize(kSetCookieHeaders));
    std::string received_data = handshake_response_->GetResponse();
    // Don't buffer receiving data for now.
    // TODO(ukai): fix performance of WebSocketFrameHandler.
    while (receive_frame_handler_->UpdateCurrentBuffer(false) > 0) {
      received_data +=
          std::string(receive_frame_handler_->GetCurrentBuffer()->data(),
                      receive_frame_handler_->GetCurrentBufferSize());
      receive_frame_handler_->ReleaseCurrentBuffer();
    }

    state_ = OPEN;
    if (delegate_)
      delegate_->OnReceivedData(
          socket_, received_data.data(), received_data.size());

    handshake_response_.reset();

    WebSocketThrottle::GetInstance()->RemoveFromQueue(this);
    WebSocketThrottle::GetInstance()->WakeupSocketIfNecessary();
    return;
  }

  bool allow = true;
  CookieOptions options;
  GURL url = GetURLForCookies();
  std::string cookie = response_cookies_[response_cookies_save_index_];
  if (delegate_ && !delegate_->CanSetCookie(socket_, url, cookie, &options))
    allow = false;

  if (socket_ && delegate_ && state_ == CONNECTING) {
    response_cookies_save_index_++;
    if (allow && socket_->context()->cookie_store()) {
      options.set_include_httponly();
      socket_->context()->cookie_store()->SetCookieWithOptionsAsync(
          url, cookie, options,
          base::Bind(&WebSocketJob::SaveCookieCallback,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      SaveNextCookie();
    }
  }
}

void WebSocketJob::SaveCookieCallback(bool cookie_status) {
  SaveNextCookie();
}

GURL WebSocketJob::GetURLForCookies() const {
  GURL url = socket_->url();
  std::string scheme = socket_->is_secure() ? "https" : "http";
  url_canon::Replacements<char> replacements;
  replacements.SetScheme(scheme.c_str(),
                         url_parse::Component(0, scheme.length()));
  return url.ReplaceComponents(replacements);
}

const AddressList& WebSocketJob::address_list() const {
  return addresses_;
}

int WebSocketJob::TrySpdyStream() {
  if (!socket_.get())
    return ERR_FAILED;

  if (!websocket_over_spdy_enabled_)
    return OK;

  // Check if we have a SPDY session available.
  HttpTransactionFactory* factory =
      socket_->context()->http_transaction_factory();
  if (!factory)
    return OK;
  scoped_refptr<HttpNetworkSession> session = factory->GetSession();
  if (!session.get())
    return OK;
  SpdySessionPool* spdy_pool = session->spdy_session_pool();
  const HostPortProxyPair pair(HostPortPair::FromURL(socket_->url()),
                               socket_->proxy_server());
  if (!spdy_pool->HasSession(pair))
    return OK;

  // Forbid wss downgrade to SPDY without SSL.
  // TODO(toyoshim): Does it realize the same policy with HTTP?
  scoped_refptr<SpdySession> spdy_session =
      spdy_pool->Get(pair, *socket_->net_log());
  SSLInfo ssl_info;
  bool was_npn_negotiated;
  bool use_ssl = spdy_session->GetSSLInfo(&ssl_info, &was_npn_negotiated);
  if (socket_->is_secure() && !use_ssl)
    return OK;

  // Create SpdyWebSocketStream.
  spdy_websocket_stream_.reset(new SpdyWebSocketStream(spdy_session, this));

  int result = spdy_websocket_stream_->InitializeStream(
      socket_->url(), MEDIUM, *socket_->net_log());
  if (result == OK) {
    OnConnected(socket_, kMaxPendingSendAllowed);
    return ERR_PROTOCOL_SWITCHED;
  }
  if (result != ERR_IO_PENDING) {
    spdy_websocket_stream_.reset();
    return OK;
  }

  return ERR_IO_PENDING;
}

void WebSocketJob::SetWaiting() {
  waiting_ = true;
}

bool WebSocketJob::IsWaiting() const {
  return waiting_;
}

void WebSocketJob::Wakeup() {
  if (!waiting_)
    return;
  waiting_ = false;
  DCHECK(callback_);
  MessageLoopForIO::current()->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketJob::RetryPendingIO,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebSocketJob::RetryPendingIO() {
  int result = TrySpdyStream();

  // In the case of ERR_IO_PENDING, CompleteIO() will be called from
  // OnCreatedSpdyStream().
  if (result != ERR_IO_PENDING)
    CompleteIO(result);
}

void WebSocketJob::CompleteIO(int result) {
  // |callback_| may be NULL if OnClose() or DetachDelegate() was called.
  if (callback_) {
    net::OldCompletionCallback* callback = callback_;
    callback_ = NULL;
    callback->Run(result);
    Release();  // Balanced with OnStartOpenConnection().
  }
}

bool WebSocketJob::SendDataInternal(const char* data, int length) {
  if (spdy_websocket_stream_.get())
    return ERR_IO_PENDING == spdy_websocket_stream_->SendData(data, length);
  if (socket_.get())
    return socket_->SendData(data, length);
  return false;
}

void WebSocketJob::CloseInternal() {
  if (spdy_websocket_stream_.get())
    spdy_websocket_stream_->Close();
  if (socket_.get())
    socket_->Close();
}

void WebSocketJob::SendPending() {
  if (current_buffer_)
    return;
  // Current buffer is done.  Try next buffer if any.
  // Don't buffer sending data. See comment on case OPEN in SendData().
  if (send_frame_handler_->UpdateCurrentBuffer(false) <= 0) {
    // No more data to send.
    if (state_ == CLOSING)
      CloseInternal();
    return;
  }
  current_buffer_ = new DrainableIOBuffer(
      send_frame_handler_->GetCurrentBuffer(),
      send_frame_handler_->GetCurrentBufferSize());
  SendDataInternal(current_buffer_->data(), current_buffer_->BytesRemaining());
}

}  // namespace net
