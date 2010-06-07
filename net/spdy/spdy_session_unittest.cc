// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_io_buffer.h"

#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_info.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_stream.h"
#include "net/spdy/spdy_test_util.h"
#include "testing/platform_test.h"

namespace net {

// TODO(cbentzel): Expose compression setter/getter in public SpdySession
//                 interface rather than going through all these contortions.
class SpdySessionTest : public PlatformTest {
 public:
  static void TurnOffCompression() {
    spdy::SpdyFramer::set_enable_compression_default(false);
  }
};

namespace {

// Helper to manage the lifetimes of the dependencies for a
// SpdyNetworkTransaction.
class SessionDependencies {
 public:
  // Default set of dependencies -- "null" proxy service.
  SessionDependencies()
      : host_resolver(new MockHostResolver),
        proxy_service(ProxyService::CreateNull()),
        ssl_config_service(new SSLConfigServiceDefaults),
        spdy_session_pool(new SpdySessionPool(NULL)) {
  }

  scoped_refptr<MockHostResolverBase> host_resolver;
  scoped_refptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  MockClientSocketFactory socket_factory;
  scoped_refptr<SpdySessionPool> spdy_session_pool;
};

HttpNetworkSession* CreateSession(SessionDependencies* session_deps) {
  return new HttpNetworkSession(NULL,
                                session_deps->host_resolver,
                                session_deps->proxy_service,
                                &session_deps->socket_factory,
                                session_deps->ssl_config_service,
                                session_deps->spdy_session_pool,
                                NULL,
                                NULL);
}

// Test the SpdyIOBuffer class.
TEST_F(SpdySessionTest, SpdyIOBuffer) {
  std::priority_queue<SpdyIOBuffer> queue_;
  const size_t kQueueSize = 100;

  // Insert 100 items; pri 100 to 1.
  for (size_t index = 0; index < kQueueSize; ++index) {
    SpdyIOBuffer buffer(new IOBuffer(), 0, kQueueSize - index, NULL);
    queue_.push(buffer);
  }

  // Insert several priority 0 items last.
  const size_t kNumDuplicates = 12;
  IOBufferWithSize* buffers[kNumDuplicates];
  for (size_t index = 0; index < kNumDuplicates; ++index) {
    buffers[index] = new IOBufferWithSize(index+1);
    queue_.push(SpdyIOBuffer(buffers[index], buffers[index]->size(), 0, NULL));
  }

  EXPECT_EQ(kQueueSize + kNumDuplicates, queue_.size());

  // Verify the P0 items come out in FIFO order.
  for (size_t index = 0; index < kNumDuplicates; ++index) {
    SpdyIOBuffer buffer = queue_.top();
    EXPECT_EQ(0, buffer.priority());
    EXPECT_EQ(index + 1, buffer.size());
    queue_.pop();
  }

  int priority = 1;
  while (queue_.size()) {
    SpdyIOBuffer buffer = queue_.top();
    EXPECT_EQ(priority++, buffer.priority());
    queue_.pop();
  }
}

static const unsigned char kGoAway[] = {
  0x80, 0x01, 0x00, 0x07,  // header
  0x00, 0x00, 0x00, 0x04,  // flags, len
  0x00, 0x00, 0x00, 0x00,  // last-accepted-stream-id
};

TEST_F(SpdySessionTest, GoAway) {
  SessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(false, OK);
  MockRead reads[] = {
    MockRead(false, reinterpret_cast<const char*>(kGoAway),
             arraysize(kGoAway)),
    MockRead(false, 0, 0)  // EOF
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory.AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
  session_deps.socket_factory.AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(CreateSession(&session_deps));

  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair;
  test_host_port_pair.host = kTestHost;
  test_host_port_pair.port = kTestPort;

  scoped_refptr<SpdySessionPool> spdy_session_pool(
      http_session->spdy_session_pool());
  EXPECT_FALSE(spdy_session_pool->HasSession(test_host_port_pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(
          test_host_port_pair, http_session.get(), BoundNetLog());
  EXPECT_TRUE(spdy_session_pool->HasSession(test_host_port_pair));

  TCPSocketParams tcp_params(kTestHost, kTestPort, MEDIUM, GURL(), false);
  int rv = session->Connect(kTestHost, tcp_params, MEDIUM);
  ASSERT_EQ(OK, rv);

  // Flush the SpdySession::OnReadComplete() task.
  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(spdy_session_pool->HasSession(test_host_port_pair));

  scoped_refptr<SpdySession> session2 =
      spdy_session_pool->Get(
          test_host_port_pair, http_session.get(), BoundNetLog());

  // Delete the first session.
  session = NULL;

  // Delete the second session.
  spdy_session_pool->Remove(session2);
  session2 = NULL;
}

// StreamCanceler is used for SpdySessionTest.CancelStreamOnClose.  It is used
// for two callbacks: the stream's SendRequest() and ReadResponseBody()
// callbacks.
class StreamCanceler {
 public:
  enum State {
    WAITING_FOR_CONNECT,
    WAITING_FOR_RESPONSE,
    STATE_DONE
  };

  explicit StreamCanceler(const scoped_refptr<SpdyStream>& stream)
      : stream_(stream),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(this, &StreamCanceler::OnIOComplete)),
        buf_(new IOBufferWithSize(64)),
        state_(WAITING_FOR_CONNECT) {}

  CompletionCallback* callback() { return &callback_; }

  State state() const { return state_; }

 private:
  void OnIOComplete(int result) {
    MessageLoop::current()->Quit();
    switch (state_) {
      case WAITING_FOR_CONNECT:
        // After receiving this callback, start the ReadResponseBody() request.
        // We need to do this here rather than elsewhere, since the MessageLoop
        // will keep processing the pending read tasks until there aren't any
        // more, so we won't get a chance to get an asynchronous callback for
        // ReadResponseBody() unless we call it here in the callback for
        // SendRequest().
        EXPECT_EQ(OK, result);
        state_ = WAITING_FOR_RESPONSE;
        EXPECT_EQ(ERR_IO_PENDING, stream_->ReadResponseBody(
            buf_.get(), buf_->size(), &callback_));
        break;
      case WAITING_FOR_RESPONSE:
        // The result should be the 6 bytes of the body.  The next read will
        // succeed synchronously, indicating the stream is closed.  We cancel
        // the stream during the callback for the first ReadResponseBody() call
        // which will deactivate the stream.  The code should handle this case.
        EXPECT_EQ(6, result);
        EXPECT_EQ(OK,
                  stream_->ReadResponseBody(
                      buf_.get(), buf_->size(), &callback_));
        stream_->Cancel();
        state_ = STATE_DONE;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  const scoped_refptr<SpdyStream> stream_;
  CompletionCallbackImpl<StreamCanceler> callback_;
  scoped_refptr<IOBufferWithSize> buf_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(StreamCanceler);
};

TEST_F(SpdySessionTest, CancelStreamOnClose) {
  SpdySessionTest::TurnOffCompression();
  SessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(false, OK);
  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory.AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
  session_deps.socket_factory.AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(CreateSession(&session_deps));

  const std::string kTestHost("www.google.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair;
  test_host_port_pair.host = kTestHost;
  test_host_port_pair.port = kTestPort;

  scoped_refptr<SpdySessionPool> spdy_session_pool(
      http_session->spdy_session_pool());
  EXPECT_FALSE(spdy_session_pool->HasSession(test_host_port_pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(
          test_host_port_pair, http_session.get(), BoundNetLog());

  HttpRequestInfo request;
  request.url = GURL("http://www.google.com");

  scoped_refptr<SpdyStream> stream =
      session->GetOrCreateStream(request, NULL, BoundNetLog());
  TCPSocketParams tcp_params(kTestHost, kTestPort, MEDIUM, GURL(), false);
  int rv = session->Connect(kTestHost, tcp_params, MEDIUM);
  ASSERT_EQ(OK, rv);

  HttpResponseInfo response;
  TestCompletionCallback callback;
  StreamCanceler canceler(stream);
  rv = stream->SendRequest(NULL, &response, canceler.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  while (canceler.state() != StreamCanceler::STATE_DONE)
    MessageLoop::current()->Run();
}

// kPush is a server-issued SYN_STREAM with stream id 2, and
// associated stream id 1. It also includes 3 headers of path,
// status, and HTTP version.
static const uint8 kPush[] = {
  0x80, 0x01, 0x00, 0x01,  // SYN_STREAM for SPDY v1.
  0x00, 0x00, 0x00, 0x3b,  // No flags 59 bytes after this 8 byte header.
  0x00, 0x00, 0x00, 0x02,  // Stream ID of 2
  0x00, 0x00, 0x00, 0x01,  // Associate Stream ID of 1
  0x00, 0x00, 0x00, 0x03,  // Priority 0, 3 name/value pairs in block below.
  0x00, 0x04, 'p', 'a', 't', 'h',
  0x00, 0x07, '/', 'f', 'o', 'o', '.', 'j', 's',
  0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
  0x00, 0x03, '2', '0', '0',
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
};

}  // namespace

TEST_F(SpdySessionTest, GetPushStream) {
  SpdySessionTest::TurnOffCompression();

  SessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(false, OK);
  MockRead reads[] = {
    MockRead(false, reinterpret_cast<const char*>(kPush),
             arraysize(kPush)),
    MockRead(true, ERR_IO_PENDING, 0)  // EOF
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory.AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
  session_deps.socket_factory.AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(CreateSession(&session_deps));

  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair;
  test_host_port_pair.host = kTestHost;
  test_host_port_pair.port = kTestPort;

  scoped_refptr<SpdySessionPool> spdy_session_pool(
      http_session->spdy_session_pool());
  EXPECT_FALSE(spdy_session_pool->HasSession(test_host_port_pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(
          test_host_port_pair, http_session.get(), BoundNetLog());
  EXPECT_TRUE(spdy_session_pool->HasSession(test_host_port_pair));

  // No push streams should exist in the beginning.
  std::string test_push_path = "/foo.js";
  scoped_refptr<SpdyStream> first_stream = session->GetPushStream(
      test_push_path);
  EXPECT_EQ(static_cast<SpdyStream*>(NULL), first_stream.get());

  // Read in the data which contains a server-issued SYN_STREAM.
  TCPSocketParams tcp_params(test_host_port_pair, MEDIUM, GURL(), false);
  int rv = session->Connect(kTestHost, tcp_params, MEDIUM);
  ASSERT_EQ(OK, rv);
  MessageLoop::current()->RunAllPending();

  // An unpushed path should not work.
  scoped_refptr<SpdyStream> unpushed_stream = session->GetPushStream(
      "/unpushed_path");
  EXPECT_EQ(static_cast<SpdyStream*>(NULL), unpushed_stream.get());

  // The pushed path should be found.
  scoped_refptr<SpdyStream> second_stream = session->GetPushStream(
      test_push_path);
  ASSERT_NE(static_cast<SpdyStream*>(NULL), second_stream.get());
  EXPECT_EQ(test_push_path, second_stream->path());
  EXPECT_EQ(2U, second_stream->stream_id());
  EXPECT_EQ(0, second_stream->priority());

  // Clean up
  second_stream = NULL;
  session = NULL;
  spdy_session_pool->CloseAllSessions();

  // RunAllPending needs to be called here because the
  // ClientSocketPoolBase posts a task to clean up and destroy the
  // underlying socket.
  MessageLoop::current()->RunAllPending();
}

}  // namespace net
