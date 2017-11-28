// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_JOB_H_
#define NET_WEBSOCKETS_WEBSOCKET_JOB_H_

#include <deque>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/socket_stream/socket_stream_job.h"

class GURL;

namespace net {

class DrainableIOBuffer;
class SSLInfo;
class WebSocketHandshakeRequestHandler;
class WebSocketHandshakeResponseHandler;

// WebSocket protocol specific job on SocketStream.
// It captures WebSocket handshake message and handles cookie operations.
// Chrome security policy doesn't allow renderer process (except dev tools)
// see HttpOnly cookies, so it injects cookie header in handshake request and
// strips set-cookie headers in handshake response.
// TODO(ukai): refactor websocket.cc to use this.
class NET_EXPORT WebSocketJob
    : public SocketStreamJob,
      public SocketStream::Delegate {
 public:
  // This is state of WebSocket, not SocketStream.
  enum State {
    INITIALIZED = -1,
    CONNECTING = 0,
    OPEN = 1,
    SEND_CLOSED = 2,  // A Close frame has been sent but not received.
    RECV_CLOSED = 3,  // Used briefly between receiving a Close frame and
                      // sending the response. Once the response is sent, the
                      // state changes to CLOSED.
    CLOSE_WAIT = 4,   // The Closing Handshake has completed, but the remote
                      // server has not yet closed the connection.
    CLOSED = 5,       // The Closing Handshake has completed and the connection
                      // has been closed; or the connection is failed.
  };

  explicit WebSocketJob(SocketStream::Delegate* delegate);

  static void EnsureInit();

  State state() const { return state_; }
  virtual void Connect() override;
  virtual bool SendData(const char* data, int len) override;
  virtual void Close() override;
  virtual void RestartWithAuth(const AuthCredentials& credentials) override;
  virtual void DetachDelegate() override;

  // SocketStream::Delegate methods.
  virtual int OnStartOpenConnection(
      SocketStream* socket, const CompletionCallback& callback) override;
  virtual void OnConnected(SocketStream* socket,
                           int max_pending_send_allowed) override;
  virtual void OnSentData(SocketStream* socket, int amount_sent) override;
  virtual void OnReceivedData(SocketStream* socket,
                              const char* data,
                              int len) override;
  virtual void OnClose(SocketStream* socket) override;
  virtual void OnAuthRequired(
      SocketStream* socket, AuthChallengeInfo* auth_info) override;
  virtual void OnSSLCertificateError(SocketStream* socket,
                                     const SSLInfo& ssl_info,
                                     bool fatal) override;
  virtual void OnError(const SocketStream* socket, int error) override;

  WebSocketHandshakeResponseHandler* GetHandshakeResponse() {
    return handshake_response_.get();
  }

  void SetState(const State new_state) {
    DCHECK_GE(new_state, state_);  // states should only move forward.
    state_ = new_state;
  }

 private:
  friend class WebSocketThrottle;
  virtual ~WebSocketJob();

  bool SendHandshakeRequest(const char* data, int len);
  void AddCookieHeaderAndSend();
  void LoadCookieCallback(const std::string& cookie);

  void OnSentHandshakeRequest(SocketStream* socket, int amount_sent);
  void OnReceivedHandshakeResponse(
      SocketStream* socket, const char* data, int len);
  void SaveCookiesAndNotifyHeaderComplete();
  void SaveNextCookie();
  void SaveCookieCallback(bool cookie_status);
  void DoSendData();

  GURL GetURLForCookies() const;

  const AddressList& address_list() const;
  void SetWaiting();
  bool IsWaiting() const;
  void Wakeup();
  void RetryPendingIO();
  void CompleteIO(int result);

  bool SendDataInternal(const char* data, int length);
  void CloseInternal();
  void SendPending();
  void CloseTimeout();
  void StopTimer(base::WaitableEvent* timer_stop_event);

  void WaitForSocketClose();
  void DelayedClose();

  SocketStream::Delegate* delegate_;
  State state_;
  bool waiting_;
  AddressList addresses_;
  CompletionCallback callback_;  // for throttling.

  scoped_ptr<WebSocketHandshakeRequestHandler> handshake_request_;
  scoped_ptr<WebSocketHandshakeResponseHandler> handshake_response_;

  bool started_to_send_handshake_request_;
  size_t handshake_request_sent_;

  std::vector<std::string> response_cookies_;
  size_t response_cookies_save_index_;

  std::deque<scoped_refptr<IOBufferWithSize> > send_buffer_queue_;
  scoped_refptr<DrainableIOBuffer> current_send_buffer_;
  std::vector<char> received_data_after_handshake_;

  std::string challenge_;

  base::TimeDelta closing_handshake_timeout_;
  scoped_ptr<base::OneShotTimer<WebSocketJob> > close_timer_;

  base::WeakPtrFactory<WebSocketJob> weak_ptr_factory_;
  base::WeakPtrFactory<WebSocketJob> weak_ptr_factory_for_send_pending_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketJob);
};

}  // namespace

#endif  // NET_WEBSOCKETS_WEBSOCKET_JOB_H_
