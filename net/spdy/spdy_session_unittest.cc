// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_io_buffer.h"
#include "net/spdy/spdy_session.h"
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

TEST_F(SpdySessionTest, GoAway) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(false, OK);
  scoped_ptr<spdy::SpdyFrame> goaway(ConstructSpdyGoAway());
  MockRead reads[] = {
    CreateMockRead(*goaway),
    MockRead(false, 0, 0)  // EOF
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory.AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
  session_deps.socket_factory.AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

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

  scoped_refptr<TCPSocketParams> tcp_params =
      new TCPSocketParams(kTestHost, kTestPort, MEDIUM, GURL(), false);
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

}  // namespace

TEST_F(SpdySessionTest, GetActivePushStream) {
  spdy::SpdyFramer framer;
  SpdySessionTest::TurnOffCompression();

  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(false, OK);
  spdy::SpdyHeaderBlock headers;
  headers["path"] = "/foo.js";
  headers["status"] = "200";
  headers["version"] = "HTTP/1.1";
  scoped_ptr<spdy::SpdyFrame> push_syn(framer.CreateSynStream(
      2, 1, 0, spdy::CONTROL_FLAG_NONE, false, &headers));
  MockRead reads[] = {
    CreateMockRead(*push_syn),
    MockRead(true, ERR_IO_PENDING, 0)  // EOF
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory.AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
  session_deps.socket_factory.AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

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
  scoped_refptr<SpdyStream> first_stream = session->GetActivePushStream(
      test_push_path);
  EXPECT_EQ(static_cast<SpdyStream*>(NULL), first_stream.get());

  // Read in the data which contains a server-issued SYN_STREAM.
  scoped_refptr<TCPSocketParams> tcp_params =
      new TCPSocketParams(test_host_port_pair, MEDIUM, GURL(), false);
  int rv = session->Connect(kTestHost, tcp_params, MEDIUM);
  ASSERT_EQ(OK, rv);
  MessageLoop::current()->RunAllPending();

  // An unpushed path should not work.
  scoped_refptr<SpdyStream> unpushed_stream = session->GetActivePushStream(
      "/unpushed_path");
  EXPECT_EQ(static_cast<SpdyStream*>(NULL), unpushed_stream.get());

  // The pushed path should be found.
  scoped_refptr<SpdyStream> second_stream = session->GetActivePushStream(
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
