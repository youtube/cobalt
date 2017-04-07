// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_job.h"

#include <algorithm>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/string_tokenizer.h"
#include "base/synchronization/waitable_event.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/cookies/cookie_store.h"
#include "net/base/io_buffer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_handshake_handler.h"
#include "net/websockets/websocket_net_log_params.h"
#include "net/websockets/websocket_throttle.h"
#include "starboard/memory.h"

static const int kMaxPendingSendAllowed = 32768;  // 32 kilobytes.

namespace {

// RFC 6455 (https://tools.ietf.org/html/rfc6455#section-7.1.1) advises
// to use 2 * Maximum Segment Life to wait for the close on the tcp connection.
// However, Chromium team has found that some servers wait for the client to
// close the connection, and this leads to unnecessary long wait times.
const int kClosingHandshakeTimeoutSeconds = 60;

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

bool IsWebSocketAlive(net::WebSocketJob::State state) {
  switch (state) {
    case net::WebSocketJob::OPEN:
    case net::WebSocketJob::SEND_CLOSED:
    case net::WebSocketJob::RECV_CLOSED:
    case net::WebSocketJob::CLOSE_WAIT:
      return true;
    case net::WebSocketJob::INITIALIZED:
    case net::WebSocketJob::CONNECTING:
    case net::WebSocketJob::CLOSED:
      return false;
  }

  return false;
}

}  // anonymous namespace

namespace net {

// static
void WebSocketJob::EnsureInit() {
  g_websocket_job_init.Get();
}

WebSocketJob::WebSocketJob(SocketStream::Delegate* delegate)
    : delegate_(delegate),
      state_(INITIALIZED),
      waiting_(false),
      handshake_request_(new WebSocketHandshakeRequestHandler),
      handshake_response_(new WebSocketHandshakeResponseHandler),
      started_to_send_handshake_request_(false),
      handshake_request_sent_(0),
      response_cookies_save_index_(0),
      closing_handshake_timeout_(
          base::TimeDelta::FromSeconds(kClosingHandshakeTimeoutSeconds)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_for_send_pending_(this)) {
}

WebSocketJob::~WebSocketJob() {
  DCHECK_EQ(CLOSED, state_);
  DCHECK(!delegate_);
  DCHECK(!socket_.get());

  base::WaitableEvent timer_stop_event(false, false);

  if (network_task_runner()) {
    network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&WebSocketJob::StopTimer, base::Unretained(this),
                              &timer_stop_event));
    timer_stop_event.Wait();
  }
}

void WebSocketJob::StopTimer(base::WaitableEvent* timer_stop_event) {
  if (close_timer_) {
    close_timer_.reset();
  }
  if (timer_stop_event) {
    timer_stop_event->Signal();
  }
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

    case OPEN: {
        scoped_refptr<IOBufferWithSize> buffer = new IOBufferWithSize(len);
        SbMemoryCopy(buffer->data(), data, len);
        if (current_send_buffer_ || !send_buffer_queue_.empty()) {
          send_buffer_queue_.push_back(buffer);
          return true;
        }
        current_send_buffer_ = new DrainableIOBuffer(buffer.get(), len);
        return SendDataInternal(current_send_buffer_->data(),
                                current_send_buffer_->BytesRemaining());
        break;
      }

      case CLOSE_WAIT:
      case SEND_CLOSED:
      case RECV_CLOSED:
    case CLOSED:
      return false;
  }
  return false;
}

void WebSocketJob::CloseTimeout() {
  DLOG(INFO) << "Close timed out";
  // DCHECK to make sure we've either:
  // 1. Sent the close message, but timed out waiting to received a response
  // 2. Closing handshake is complete, but the server has not closed the
  // connection.
  DCHECK((state_ == SEND_CLOSED) || (state_ == CLOSE_WAIT));
  state_ = CLOSED;
  CloseInternal();
}

void WebSocketJob::Close() {
  if (state_ == CLOSED)
    return;

  if (current_send_buffer_) {
    // Will close in SendPending.
    return;
  }

  WaitForSocketClose();
}

void WebSocketJob::DelayedClose() {
  if (!close_timer_) {
    DLOG(INFO) << "Delaying close";
    close_timer_ = make_scoped_ptr<base::OneShotTimer<WebSocketJob> >(
        new base::OneShotTimer<WebSocketJob>());
    close_timer_->Start(
        FROM_HERE, closing_handshake_timeout_,
        base::Bind(&WebSocketJob::CloseTimeout, base::Unretained(this)));
  }
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
  if (!callback_.is_null()) {
    waiting_ = false;
    callback_.Reset();
    Release();  // Balanced with OnStartOpenConnection().
  }
}

int WebSocketJob::OnStartOpenConnection(
    SocketStream* socket, const CompletionCallback& callback) {
  DCHECK(callback_.is_null());
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
    AddRef();  // Balanced when callback_ is cleared.
    return ERR_IO_PENDING;
  }
  return OK;
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
    DCHECK(IsWebSocketAlive(state_));
    if (!current_send_buffer_) {
      VLOG(1) << "OnSentData current_send_buffer=NULL amount_sent="
              << amount_sent;
      return;
    }
    current_send_buffer_->DidConsume(amount_sent);
    if (current_send_buffer_->BytesRemaining() > 0)
      return;

    // We need to report amount_sent of original buffer size, instead of
    // amount sent to |socket|.
    amount_sent = current_send_buffer_->size();
    DCHECK_GT(amount_sent, 0);
    current_send_buffer_ = NULL;
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
  DCHECK(IsWebSocketAlive(state_));
  if (delegate_ && len > 0)
    delegate_->OnReceivedData(socket, data, len);
}

void WebSocketJob::OnClose(SocketStream* socket) {
  state_ = CLOSED;
  if (close_timer_) {
    close_timer_.reset();
  }
  WebSocketThrottle::GetInstance()->RemoveFromQueue(this);
  WebSocketThrottle::GetInstance()->WakeupSocketIfNecessary();

  scoped_refptr<WebSocketJob> protect(this);
  weak_ptr_factory_.InvalidateWeakPtrs();

  SocketStream::Delegate* delegate = delegate_;
  delegate_ = NULL;
  socket_ = NULL;
  if (!callback_.is_null()) {
    waiting_ = false;
    callback_.Reset();
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

void WebSocketJob::OnSSLCertificateError(
    SocketStream* socket, const SSLInfo& ssl_info, bool fatal) {
  if (delegate_)
    delegate_->OnSSLCertificateError(socket, ssl_info, fatal);
}

void WebSocketJob::OnError(const SocketStream* socket, int error) {
  if (delegate_ && error != ERR_PROTOCOL_SWITCHED)
    delegate_->OnError(socket, error);
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
      const std::string& handshake_request =
        handshake_request_->GetRawRequest();
    handshake_request_sent_ = 0;
    socket_->net_log()->AddEvent(
        NetLog::TYPE_WEB_SOCKET_SEND_REQUEST_HEADERS,
        base::Bind(&NetLogWebSocketHandshakeCallback, &handshake_request));
    socket_->SendData(handshake_request.data(),
                      handshake_request.size());
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
    std::size_t original_length = handshake_request_->original_length();
    handshake_request_.reset();
    if (delegate_)
      delegate_->OnSentData(
          socket,
          original_length);
  }
}

void WebSocketJob::OnReceivedHandshakeResponse(
    SocketStream* socket, const char* data, int len) {
  DCHECK_EQ(state_, CONNECTING);
  if (handshake_response_->HasResponse()) {
    // If we already has handshake response, received data should be frame
    // data, not handshake message.
    received_data_after_handshake_.insert(
        received_data_after_handshake_.end(), data, data + len);
    return;
  }

  size_t response_length = handshake_response_->ParseRawResponse(data, len);
  if (!handshake_response_->HasResponse()) {
    // not yet. we need more data.
    return;
  }
  // handshake message is completed.

  if (len - response_length > 0) {
    // If we received extra data, it should be frame data.
    DCHECK(received_data_after_handshake_.empty());
    received_data_after_handshake_.assign(data + response_length, data + len);
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
    std::string handshake_response = handshake_response_->GetResponse();
    std::vector<char> received_data(handshake_response.begin(),
                                    handshake_response.end());
    received_data.insert(received_data.end(),
                         received_data_after_handshake_.begin(),
                         received_data_after_handshake_.end());
    received_data_after_handshake_.clear();

    state_ = OPEN;

    DCHECK(!received_data.empty());
    if (delegate_)
      delegate_->OnReceivedData(
          socket_, &received_data.front(), received_data.size());

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
  DCHECK(!callback_.is_null());
  MessageLoopForIO::current()->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketJob::RetryPendingIO,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebSocketJob::RetryPendingIO() {
  CompleteIO(OK);
}

void WebSocketJob::CompleteIO(int result) {
  // |callback_| may be null if OnClose() or DetachDelegate() was called.
  if (!callback_.is_null()) {
    CompletionCallback callback = callback_;
    callback_.Reset();
    callback.Run(result);
    Release();  // Balanced with OnStartOpenConnection().
  }
}

bool WebSocketJob::SendDataInternal(const char* data, int length) {
  if (socket_.get())
    return socket_->SendData(data, length);
  return false;
}

void WebSocketJob::CloseInternal() {
  if (socket_.get())
    socket_->Close();
}

void WebSocketJob::WaitForSocketClose() {
  switch (state_) {
    case CLOSE_WAIT:
    case CLOSED:
    case OPEN:
      break;
    case INITIALIZED:
    case CONNECTING:
      NOTREACHED();
      break;
    case SEND_CLOSED:
    // Close frame has been sent, so wait for peer to send a close reponse,
    // and a socket close.
    case RECV_CLOSED:
      // Our reponse to a close frame has gone out (since |send_buffer_queue_|
      // is empty.  So wait for the peer to close the connection;
      DelayedClose();
      break;
  }
}

void WebSocketJob::SendPending() {
  if (current_send_buffer_)
    return;

  // Current buffer has been sent. Try next if any.
  if (send_buffer_queue_.empty()) {
    // No more data to send.
    WaitForSocketClose();
    return;
  }

  scoped_refptr<IOBufferWithSize> next_buffer = send_buffer_queue_.front();
  send_buffer_queue_.pop_front();
  current_send_buffer_ = new DrainableIOBuffer(next_buffer,
                                               next_buffer->size());
  SendDataInternal(current_send_buffer_->data(),
                   current_send_buffer_->BytesRemaining());
}

}  // namespace net
