// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "net/base/auth.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "net/socket_stream/socket_stream.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

struct SocketStreamEvent {
  enum EventType {
    EVENT_CONNECTED, EVENT_SENT_DATA, EVENT_RECEIVED_DATA, EVENT_CLOSE,
    EVENT_AUTH_REQUIRED,
  };

  SocketStreamEvent(EventType type, net::SocketStream* socket_stream,
                    int num, const std::string& str,
                    net::AuthChallengeInfo* auth_challenge_info)
      : event_type(type), socket(socket_stream), number(num), data(str),
        auth_info(auth_challenge_info) {}

  EventType event_type;
  net::SocketStream* socket;
  int number;
  std::string data;
  scoped_refptr<net::AuthChallengeInfo> auth_info;
};

class SocketStreamEventRecorder : public net::SocketStream::Delegate {
 public:
  explicit SocketStreamEventRecorder(net::CompletionCallback* callback)
      : on_connected_(NULL),
        on_sent_data_(NULL),
        on_received_data_(NULL),
        on_close_(NULL),
        on_auth_required_(NULL),
        callback_(callback) {}
  virtual ~SocketStreamEventRecorder() {
    delete on_connected_;
    delete on_sent_data_;
    delete on_received_data_;
    delete on_close_;
    delete on_auth_required_;
  }

  void SetOnConnected(Callback1<SocketStreamEvent*>::Type* callback) {
    on_connected_ = callback;
  }
  void SetOnSentData(Callback1<SocketStreamEvent*>::Type* callback) {
    on_sent_data_ = callback;
  }
  void SetOnReceivedData(Callback1<SocketStreamEvent*>::Type* callback) {
    on_received_data_ = callback;
  }
  void SetOnClose(Callback1<SocketStreamEvent*>::Type* callback) {
    on_close_ = callback;
  }
  void SetOnAuthRequired(Callback1<SocketStreamEvent*>::Type* callback) {
    on_auth_required_ = callback;
  }

  virtual void OnConnected(net::SocketStream* socket,
                           int num_pending_send_allowed) {
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_CONNECTED,
                          socket, num_pending_send_allowed, std::string(),
                          NULL));
    if (on_connected_)
      on_connected_->Run(&events_.back());
  }
  virtual void OnSentData(net::SocketStream* socket,
                          int amount_sent) {
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_SENT_DATA,
                          socket, amount_sent, std::string(), NULL));
    if (on_sent_data_)
      on_sent_data_->Run(&events_.back());
  }
  virtual void OnReceivedData(net::SocketStream* socket,
                              const char* data, int len) {
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_RECEIVED_DATA,
                          socket, len, std::string(data, len), NULL));
    if (on_received_data_)
      on_received_data_->Run(&events_.back());
  }
  virtual void OnClose(net::SocketStream* socket) {
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_CLOSE,
                          socket, 0, std::string(), NULL));
    if (on_close_)
      on_close_->Run(&events_.back());
    if (callback_)
      callback_->Run(net::OK);
  }
  virtual void OnAuthRequired(net::SocketStream* socket,
                              net::AuthChallengeInfo* auth_info) {
    events_.push_back(
        SocketStreamEvent(SocketStreamEvent::EVENT_AUTH_REQUIRED,
                          socket, 0, std::string(), auth_info));
    if (on_auth_required_)
      on_auth_required_->Run(&events_.back());
  }

  void DoClose(SocketStreamEvent* event) {
    event->socket->Close();
  }
  void DoRestartWithAuth(SocketStreamEvent* event) {
    VLOG(1) << "RestartWithAuth username=" << username_
            << " password=" << password_;
    event->socket->RestartWithAuth(username_, password_);
  }
  void SetAuthInfo(const string16& username,
                   const string16& password) {
    username_ = username;
    password_ = password;
  }

  const std::vector<SocketStreamEvent>& GetSeenEvents() const {
    return events_;
  }

 private:
  std::vector<SocketStreamEvent> events_;
  Callback1<SocketStreamEvent*>::Type* on_connected_;
  Callback1<SocketStreamEvent*>::Type* on_sent_data_;
  Callback1<SocketStreamEvent*>::Type* on_received_data_;
  Callback1<SocketStreamEvent*>::Type* on_close_;
  Callback1<SocketStreamEvent*>::Type* on_auth_required_;
  net::CompletionCallback* callback_;

  string16 username_;
  string16 password_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamEventRecorder);
};

namespace net {

class SocketStreamTest : public PlatformTest {
 public:
  virtual ~SocketStreamTest() {}
  virtual void SetUp() {
    mock_socket_factory_.reset();
    handshake_request_ = kWebSocketHandshakeRequest;
    handshake_response_ = kWebSocketHandshakeResponse;
  }
  virtual void TearDown() {
    mock_socket_factory_.reset();
  }

  virtual void SetWebSocketHandshakeMessage(
      const char* request, const char* response) {
    handshake_request_ = request;
    handshake_response_ = response;
  }
  virtual void AddWebSocketMessage(const std::string& message) {
    messages_.push_back(message);
  }

  virtual MockClientSocketFactory* GetMockClientSocketFactory() {
    mock_socket_factory_.reset(new MockClientSocketFactory);
    return mock_socket_factory_.get();
  }

  virtual void DoSendWebSocketHandshake(SocketStreamEvent* event) {
    event->socket->SendData(
        handshake_request_.data(), handshake_request_.size());
  }

  virtual void DoCloseFlushPendingWriteTest(SocketStreamEvent* event) {
    // handshake response received.
    for (size_t i = 0; i < messages_.size(); i++) {
      std::vector<char> frame;
      frame.push_back('\0');
      frame.insert(frame.end(), messages_[i].begin(), messages_[i].end());
      frame.push_back('\xff');
      EXPECT_TRUE(event->socket->SendData(&frame[0], frame.size()));
    }
    // Actual ClientSocket close must happen after all frames queued by
    // SendData above are sent out.
    event->socket->Close();
  }

  static const char* kWebSocketHandshakeRequest;
  static const char* kWebSocketHandshakeResponse;

 private:
  std::string handshake_request_;
  std::string handshake_response_;
  std::vector<std::string> messages_;

  scoped_ptr<MockClientSocketFactory> mock_socket_factory_;
};

const char* SocketStreamTest::kWebSocketHandshakeRequest =
    "GET /demo HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Upgrade: WebSocket\r\n"
    "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
    "Origin: http://example.com\r\n"
    "\r\n"
    "^n:ds[4U";

const char* SocketStreamTest::kWebSocketHandshakeResponse =
    "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Origin: http://example.com\r\n"
    "Sec-WebSocket-Location: ws://example.com/demo\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "\r\n"
    "8jKS'y:G*Co,Wxa-";

TEST_F(SocketStreamTest, CloseFlushPendingWrite) {
  TestCompletionCallback callback;

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(&callback));
  // Necessary for NewCallback.
  SocketStreamTest* test = this;
  delegate->SetOnConnected(NewCallback(
      test, &SocketStreamTest::DoSendWebSocketHandshake));
  delegate->SetOnReceivedData(NewCallback(
      test, &SocketStreamTest::DoCloseFlushPendingWriteTest));

  MockHostResolver host_resolver;

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get()));

  socket_stream->set_context(new TestURLRequestContext());
  socket_stream->SetHostResolver(&host_resolver);

  MockWrite data_writes[] = {
    MockWrite(SocketStreamTest::kWebSocketHandshakeRequest),
    MockWrite(true, "\0message1\xff", 10),
    MockWrite(true, "\0message2\xff", 10)
  };
  MockRead data_reads[] = {
    MockRead(SocketStreamTest::kWebSocketHandshakeResponse),
    // Server doesn't close the connection after handshake.
    MockRead(true, ERR_IO_PENDING)
  };
  AddWebSocketMessage("message1");
  AddWebSocketMessage("message2");

  scoped_refptr<DelayedSocketData> data_provider(
      new DelayedSocketData(1,
                            data_reads, arraysize(data_reads),
                            data_writes, arraysize(data_writes)));

  MockClientSocketFactory* mock_socket_factory =
      GetMockClientSocketFactory();
  mock_socket_factory->AddSocketDataProvider(data_provider.get());

  socket_stream->SetClientSocketFactory(mock_socket_factory);

  socket_stream->Connect();

  callback.WaitForResult();

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  EXPECT_EQ(6U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_CONNECTED, events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_SENT_DATA, events[1].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_RECEIVED_DATA, events[2].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_SENT_DATA, events[3].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_SENT_DATA, events[4].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[5].event_type);
}

TEST_F(SocketStreamTest, BasicAuthProxy) {
  MockClientSocketFactory mock_socket_factory;
  MockWrite data_writes1[] = {
    MockWrite("CONNECT example.com:80 HTTP/1.1\r\n"
              "Host: example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("\r\n"),
  };
  StaticSocketDataProvider data1(data_reads1, arraysize(data_reads1),
                                 data_writes1, arraysize(data_writes1));
  mock_socket_factory.AddSocketDataProvider(&data1);

  MockWrite data_writes2[] = {
    MockWrite("CONNECT example.com:80 HTTP/1.1\r\n"
              "Host: example.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead data_reads2[] = {
    MockRead("HTTP/1.1 200 Connection Established\r\n"),
    MockRead("Proxy-agent: Apache/2.2.8\r\n"),
    MockRead("\r\n"),
    // SocketStream::DoClose is run asynchronously.  Socket can be read after
    // "\r\n".  We have to give ERR_IO_PENDING to SocketStream then to indicate
    // server doesn't close the connection.
    MockRead(true, ERR_IO_PENDING)
  };
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                 data_writes2, arraysize(data_writes2));
  mock_socket_factory.AddSocketDataProvider(&data2);

  TestCompletionCallback callback;

  scoped_ptr<SocketStreamEventRecorder> delegate(
      new SocketStreamEventRecorder(&callback));
  delegate->SetOnConnected(NewCallback(delegate.get(),
                                       &SocketStreamEventRecorder::DoClose));
  delegate->SetAuthInfo(ASCIIToUTF16("foo"), ASCIIToUTF16("bar"));
  delegate->SetOnAuthRequired(
      NewCallback(delegate.get(),
                  &SocketStreamEventRecorder::DoRestartWithAuth));

  scoped_refptr<SocketStream> socket_stream(
      new SocketStream(GURL("ws://example.com/demo"), delegate.get()));

  socket_stream->set_context(new TestURLRequestContext("myproxy:70"));
  MockHostResolver host_resolver;
  socket_stream->SetHostResolver(&host_resolver);
  socket_stream->SetClientSocketFactory(&mock_socket_factory);

  socket_stream->Connect();

  callback.WaitForResult();

  const std::vector<SocketStreamEvent>& events = delegate->GetSeenEvents();
  EXPECT_EQ(3U, events.size());

  EXPECT_EQ(SocketStreamEvent::EVENT_AUTH_REQUIRED, events[0].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CONNECTED, events[1].event_type);
  EXPECT_EQ(SocketStreamEvent::EVENT_CLOSE, events[2].event_type);

  // TODO(eroman): Add back NetLogTest here...
}

}  // namespace net
