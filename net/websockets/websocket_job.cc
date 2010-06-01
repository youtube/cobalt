// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_job.h"

#include <algorithm>

#include "base/string_tokenizer.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/cookie_policy.h"
#include "net/base/cookie_store.h"
#include "net/base/io_buffer.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_frame_handler.h"
#include "net/websockets/websocket_throttle.h"

namespace {

const size_t kRequestKey3Size = 8U;
const size_t kResponseKeySize = 16U;

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
  friend struct DefaultSingletonTraits<WebSocketJobInitSingleton>;
  WebSocketJobInitSingleton() {
    net::SocketStreamJob::RegisterProtocolFactory("ws", WebSocketJobFactory);
    net::SocketStreamJob::RegisterProtocolFactory("wss", WebSocketJobFactory);
  }
};

void ParseHandshakeMessage(
    const char* handshake_message, int len,
    std::string* status_line,
    std::string* header) {
  size_t i = base::StringPiece(handshake_message, len).find_first_of("\r\n");
  if (i == base::StringPiece::npos) {
    *status_line = std::string(handshake_message, len);
    *header = "";
    return;
  }
  *status_line = std::string(handshake_message, i + 2);
  *header = std::string(handshake_message + i + 2, len - i - 2);
}

void FetchResponseCookies(
    const char* handshake_message, int len,
    std::vector<std::string>* response_cookies) {
  std::string handshake_response(handshake_message, len);
  net::HttpUtil::HeadersIterator iter(handshake_response.begin(),
                                      handshake_response.end(), "\r\n");
  while (iter.GetNext()) {
    for (size_t i = 0; i < arraysize(kSetCookieHeaders); i++) {
      if (LowerCaseEqualsASCII(iter.name_begin(), iter.name_end(),
                               kSetCookieHeaders[i])) {
        response_cookies->push_back(iter.values());
      }
    }
  }
}

bool GetHeaderName(std::string::const_iterator line_begin,
                   std::string::const_iterator line_end,
                   std::string::const_iterator* name_begin,
                   std::string::const_iterator* name_end) {
  std::string::const_iterator colon = std::find(line_begin, line_end, ':');
  if (colon == line_end) {
    return false;
  }
  *name_begin = line_begin;
  *name_end = colon;
  if (*name_begin == *name_end || net::HttpUtil::IsLWS(**name_begin))
    return false;
  net::HttpUtil::TrimLWS(name_begin, name_end);
  return true;
}

// Similar to HttpUtil::StripHeaders, but it preserves malformed headers, that
// is, lines that are not formatted as "<name>: <value>\r\n".
std::string FilterHeaders(
    const std::string& headers,
    const char* const headers_to_remove[],
    size_t headers_to_remove_len) {
  std::string filtered_headers;

  StringTokenizer lines(headers.begin(), headers.end(), "\r\n");
  while (lines.GetNext()) {
    std::string::const_iterator line_begin = lines.token_begin();
    std::string::const_iterator line_end = lines.token_end();
    std::string::const_iterator name_begin;
    std::string::const_iterator name_end;
    bool should_remove = false;
    if (GetHeaderName(line_begin, line_end, &name_begin, &name_end)) {
      for (size_t i = 0; i < headers_to_remove_len; ++i) {
        if (LowerCaseEqualsASCII(name_begin, name_end, headers_to_remove[i])) {
          should_remove = true;
          break;
        }
      }
    }
    if (!should_remove) {
      filtered_headers.append(line_begin, line_end);
      filtered_headers.append("\r\n");
    }
  }
  return filtered_headers;
}

}  // anonymous namespace

namespace net {

// static
void WebSocketJob::EnsureInit() {
  Singleton<WebSocketJobInitSingleton>::get();
}

WebSocketJob::WebSocketJob(SocketStream::Delegate* delegate)
    : delegate_(delegate),
      state_(INITIALIZED),
      waiting_(false),
      callback_(NULL),
      handshake_request_sent_(0),
      handshake_response_header_length_(0),
      response_cookies_save_index_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(can_get_cookies_callback_(
          this, &WebSocketJob::OnCanGetCookiesCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(can_set_cookie_callback_(
          this, &WebSocketJob::OnCanSetCookieCompleted)),
      send_frame_handler_(new WebSocketFrameHandler),
      receive_frame_handler_(new WebSocketFrameHandler) {
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
        // sending more data when pending data reaches max_pending_send_allowed.
        // TODO(ukai): Fix this to support compression for larger message.
        int err = 0;
        if (!send_frame_handler_->GetCurrentBuffer() &&
            (err = send_frame_handler_->UpdateCurrentBuffer(false)) > 0) {
          current_buffer_ = new DrainableIOBuffer(
              send_frame_handler_->GetCurrentBuffer(),
              send_frame_handler_->GetCurrentBufferSize());
          return socket_->SendData(
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
  state_ = CLOSING;
  if (current_buffer_) {
    // Will close in SendPending.
    return;
  }
  state_ = CLOSED;
  socket_->Close();
}

void WebSocketJob::RestartWithAuth(
    const std::wstring& username,
    const std::wstring& password) {
  state_ = CONNECTING;
  socket_->RestartWithAuth(username, password);
}

void WebSocketJob::DetachDelegate() {
  state_ = CLOSED;
  Singleton<WebSocketThrottle>::get()->RemoveFromQueue(this);
  Singleton<WebSocketThrottle>::get()->WakeupSocketIfNecessary();

  scoped_refptr<WebSocketJob> protect(this);

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
    SocketStream* socket, CompletionCallback* callback) {
  DCHECK(!callback_);
  state_ = CONNECTING;
  addresses_.Copy(socket->address_list().head(), true);
  Singleton<WebSocketThrottle>::get()->PutInQueue(this);
  if (!waiting_)
    return OK;
  callback_ = callback;
  AddRef();  // Balanced when callback_ becomes NULL.
  return ERR_IO_PENDING;
}

void WebSocketJob::OnConnected(
    SocketStream* socket, int max_pending_send_allowed) {
  if (delegate_)
    delegate_->OnConnected(socket, max_pending_send_allowed);
}

void WebSocketJob::OnSentData(SocketStream* socket, int amount_sent) {
  if (state_ == CONNECTING) {
    OnSentHandshakeRequest(socket, amount_sent);
    return;
  }
  if (delegate_) {
    DCHECK_GT(amount_sent, 0);
    current_buffer_->DidConsume(amount_sent);
    if (current_buffer_->BytesRemaining() > 0)
      return;

    // We need to report amount_sent of original buffer size, instead of
    // amount sent to |socket|.
    amount_sent = send_frame_handler_->GetOriginalBufferSize();
    DCHECK_GT(amount_sent, 0);
    current_buffer_ = NULL;
    send_frame_handler_->ReleaseCurrentBuffer();
    delegate_->OnSentData(socket, amount_sent);
    MessageLoopForIO::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &WebSocketJob::SendPending));
  }
}

void WebSocketJob::OnReceivedData(
    SocketStream* socket, const char* data, int len) {
  if (state_ == CONNECTING) {
    OnReceivedHandshakeResponse(socket, data, len);
    return;
  }
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
  if (delegate_ && received_data.size() > 0)
      delegate_->OnReceivedData(
          socket, received_data.data(), received_data.size());
}

void WebSocketJob::OnClose(SocketStream* socket) {
  state_ = CLOSED;
  Singleton<WebSocketThrottle>::get()->RemoveFromQueue(this);
  Singleton<WebSocketThrottle>::get()->WakeupSocketIfNecessary();

  scoped_refptr<WebSocketJob> protect(this);

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
  if (delegate_)
    delegate_->OnError(socket, error);
}

bool WebSocketJob::SendHandshakeRequest(const char* data, int len) {
  DCHECK_EQ(state_, CONNECTING);
  if (!handshake_request_.empty()) {
    // if we're already sending handshake message, don't send any more data
    // until handshake is completed.
    return false;
  }
  original_handshake_request_.append(data, len);
  original_handshake_request_header_length_ =
      HttpUtil::LocateEndOfHeaders(original_handshake_request_.data(),
                                   original_handshake_request_.size(), 0);
  if (original_handshake_request_header_length_ > 0 &&
      original_handshake_request_header_length_ + kRequestKey3Size <=
      original_handshake_request_.size()) {
    // handshake message is completed.
    AddCookieHeaderAndSend();
  }
  // Just buffered in original_handshake_request_.
  return true;
}

void WebSocketJob::AddCookieHeaderAndSend() {
  AddRef();  // Balanced in OnCanGetCookiesCompleted

  int policy = OK;
  if (socket_->context()->cookie_policy()) {
    GURL url_for_cookies = GetURLForCookies();
    policy = socket_->context()->cookie_policy()->CanGetCookies(
        url_for_cookies,
        url_for_cookies,
        &can_get_cookies_callback_);
    if (policy == ERR_IO_PENDING)
      return;  // Wait for completion callback
  }
  OnCanGetCookiesCompleted(policy);
}

void WebSocketJob::OnCanGetCookiesCompleted(int policy) {
  if (socket_ && delegate_ && state_ == CONNECTING) {
    std::string handshake_request_status_line;
    std::string handshake_request_header;
    ParseHandshakeMessage(original_handshake_request_.data(),
                          original_handshake_request_header_length_,
                          &handshake_request_status_line,
                          &handshake_request_header);

    // Remove cookie headers.  We should not send out malformed headers, so
    // use HttpUtil::StripHeaders() instead of FilterHeaders().
    handshake_request_header = HttpUtil::StripHeaders(
        handshake_request_header,
        kCookieHeaders, arraysize(kCookieHeaders));

    if (policy == OK) {
      // Add cookies, including HttpOnly cookies.
      if (socket_->context()->cookie_store()) {
        CookieOptions cookie_options;
        cookie_options.set_include_httponly();
        std::string cookie =
            socket_->context()->cookie_store()->GetCookiesWithOptions(
                GetURLForCookies(), cookie_options);
        if (!cookie.empty()) {
          HttpUtil::AppendHeaderIfMissing("Cookie", cookie,
                                          &handshake_request_header);
        }
      }
    }

    // draft-hixie-thewebsocketprotocol-76 or later will send /key3/
    // after handshake request header.
    // Assumes WebKit doesn't send any data after handshake request message
    // until handshake is finished.
    // Thus, additional_data is part of handshake message, and not in part
    // of websocket frame stream.
    DCHECK_EQ(kRequestKey3Size,
              original_handshake_request_.size() -
              original_handshake_request_header_length_);
    std::string additional_data =
      std::string(original_handshake_request_.data() +
                  original_handshake_request_header_length_,
                  original_handshake_request_.size() -
                  original_handshake_request_header_length_);
    handshake_request_ =
        handshake_request_status_line + handshake_request_header + "\r\n" +
        additional_data;

    handshake_request_sent_ = 0;
    socket_->SendData(handshake_request_.data(),
                      handshake_request_.size());
  }
  Release();  // Balance AddRef taken in AddCookieHeaderAndSend
}

void WebSocketJob::OnSentHandshakeRequest(
    SocketStream* socket, int amount_sent) {
  DCHECK_EQ(state_, CONNECTING);
  handshake_request_sent_ += amount_sent;
  DCHECK_LE(handshake_request_sent_, handshake_request_.size());
  if (handshake_request_sent_ >= handshake_request_.size()) {
    // handshake request has been sent.
    // notify original size of handshake request to delegate.
    if (delegate_)
      delegate_->OnSentData(socket, original_handshake_request_.size());
  }
}

void WebSocketJob::OnReceivedHandshakeResponse(
    SocketStream* socket, const char* data, int len) {
  DCHECK_EQ(state_, CONNECTING);
  // Check if response is already full received before appending new data
  // to |handshake_response_|
  if (handshake_response_header_length_ > 0 &&
      handshake_response_header_length_ + kResponseKeySize
      <= handshake_response_.size()) {
    // already started cookies processing.
    handshake_response_.append(data, len);
    return;
  }
  handshake_response_.append(data, len);
  handshake_response_header_length_ = HttpUtil::LocateEndOfHeaders(
      handshake_response_.data(),
      handshake_response_.size(), 0);
  if (handshake_response_header_length_ > 0 &&
      handshake_response_header_length_ + kResponseKeySize
      <= handshake_response_.size()) {
    // handshake message is completed.
    SaveCookiesAndNotifyHeaderComplete();
  }
}

void WebSocketJob::SaveCookiesAndNotifyHeaderComplete() {
  // handshake message is completed.
  DCHECK(handshake_response_.data());
  DCHECK_GT(handshake_response_header_length_, 0);

  response_cookies_.clear();
  response_cookies_save_index_ = 0;

  FetchResponseCookies(handshake_response_.data(),
                       handshake_response_header_length_,
                       &response_cookies_);

  // Now, loop over the response cookies, and attempt to persist each.
  SaveNextCookie();
}

void WebSocketJob::SaveNextCookie() {
  if (response_cookies_save_index_ == response_cookies_.size()) {
    response_cookies_.clear();
    response_cookies_save_index_ = 0;

    std::string handshake_response_status_line;
    std::string handshake_response_header;
    ParseHandshakeMessage(handshake_response_.data(),
                          handshake_response_header_length_,
                          &handshake_response_status_line,
                          &handshake_response_header);
    // Remove cookie headers, with malformed headers preserved.
    // Actual handshake should be done in WebKit.
    std::string filtered_handshake_response_header =
        FilterHeaders(handshake_response_header,
                      kSetCookieHeaders, arraysize(kSetCookieHeaders));
    std::string response_key =
        std::string(handshake_response_.data() +
                    handshake_response_header_length_,
                    kResponseKeySize);
    std::string received_data =
        handshake_response_status_line +
        filtered_handshake_response_header +
        "\r\n" +
        response_key;
    if (handshake_response_header_length_ + kResponseKeySize
        < handshake_response_.size()) {
      receive_frame_handler_->AppendData(
          handshake_response_.data() + handshake_response_header_length_ +
          kResponseKeySize,
          handshake_response_.size() - handshake_response_header_length_ -
          kResponseKeySize);
      // Don't buffer receiving data for now.
      // TODO(ukai): fix performance of WebSocketFrameHandler.
      while (receive_frame_handler_->UpdateCurrentBuffer(false) > 0) {
        received_data +=
            std::string(receive_frame_handler_->GetCurrentBuffer()->data(),
                        receive_frame_handler_->GetCurrentBufferSize());
        receive_frame_handler_->ReleaseCurrentBuffer();
      }
    }

    state_ = OPEN;
    if (delegate_)
      delegate_->OnReceivedData(
          socket_, received_data.data(), received_data.size());

    Singleton<WebSocketThrottle>::get()->RemoveFromQueue(this);
    Singleton<WebSocketThrottle>::get()->WakeupSocketIfNecessary();
    return;
  }

  AddRef();  // Balanced in OnCanSetCookieCompleted

  int policy = OK;
  if (socket_->context()->cookie_policy()) {
    GURL url_for_cookies = GetURLForCookies();
    policy = socket_->context()->cookie_policy()->CanSetCookie(
        url_for_cookies,
        url_for_cookies,
        response_cookies_[response_cookies_save_index_],
        &can_set_cookie_callback_);
    if (policy == ERR_IO_PENDING)
      return;  // Wait for completion callback
  }

  OnCanSetCookieCompleted(policy);
}

void WebSocketJob::OnCanSetCookieCompleted(int policy) {
  if (socket_ && delegate_ && state_ == CONNECTING) {
    if ((policy == OK || policy == OK_FOR_SESSION_ONLY) &&
        socket_->context()->cookie_store()) {
      CookieOptions options;
      options.set_include_httponly();
      if (policy == OK_FOR_SESSION_ONLY)
        options.set_force_session();
      GURL url_for_cookies = GetURLForCookies();
      socket_->context()->cookie_store()->SetCookieWithOptions(
          url_for_cookies, response_cookies_[response_cookies_save_index_],
          options);
    }
    response_cookies_save_index_++;
    SaveNextCookie();
  }
  Release();  // Balance AddRef taken in SaveNextCookie
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
  DCHECK(callback_);
  MessageLoopForIO::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &WebSocketJob::DoCallback));
}

void WebSocketJob::DoCallback() {
  // |callback_| may be NULL if OnClose() or DetachDelegate() was called.
  if (callback_) {
    net::CompletionCallback* callback = callback_;
    callback_ = NULL;
    callback->Run(net::OK);
    Release();  // Balanced with OnStartOpenConnection().
  }
}

void WebSocketJob::SendPending() {
  if (current_buffer_)
    return;
  // Current buffer is done.  Try next buffer if any.
  // Don't buffer sending data. See comment on case OPEN in SendData().
  if (send_frame_handler_->UpdateCurrentBuffer(false) <= 0) {
    // No more data to send.
    if (state_ == CLOSING)
      socket_->Close();
    return;
  }
  current_buffer_ = new DrainableIOBuffer(
      send_frame_handler_->GetCurrentBuffer(),
      send_frame_handler_->GetCurrentBufferSize());
  socket_->SendData(current_buffer_->data(), current_buffer_->BytesRemaining());
}

}  // namespace net
