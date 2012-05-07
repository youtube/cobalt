// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session.h"

#include "net/base/host_cache.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_log_unittest.h"
#include "net/spdy/spdy_io_buffer.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_stream.h"
#include "net/spdy/spdy_test_util_spdy2.h"
#include "testing/platform_test.h"

using namespace net::test_spdy2;

namespace net {

// TODO(cbentzel): Expose compression setter/getter in public SpdySession
//                 interface rather than going through all these contortions.
class SpdySessionSpdy2Test : public PlatformTest {
 protected:
  virtual void SetUp() {
    SpdySession::set_default_protocol(kProtoSPDY2);
  }

 private:
  SpdyTestStateHelper spdy_state_;
};

class TestSpdyStreamDelegate : public net::SpdyStream::Delegate {
 public:
  explicit TestSpdyStreamDelegate(const CompletionCallback& callback)
      : callback_(callback) {}
  virtual ~TestSpdyStreamDelegate() {}

  virtual bool OnSendHeadersComplete(int status) { return true; }

  virtual int OnSendBody() {
    return ERR_UNEXPECTED;
  }

  virtual int OnSendBodyComplete(int /*status*/, bool* /*eof*/) {
    return ERR_UNEXPECTED;
  }

  virtual int OnResponseReceived(const SpdyHeaderBlock& response,
                                 base::Time response_time,
                                 int status) {
    return status;
  }

  virtual void OnDataReceived(const char* buffer, int bytes) {
  }

  virtual void OnDataSent(int length) {
  }

  virtual void OnClose(int status) {
    CompletionCallback callback = callback_;
    callback_.Reset();
    callback.Run(OK);
  }

  virtual void set_chunk_callback(net::ChunkCallback *) {}

 private:
  CompletionCallback callback_;
};

// Test the SpdyIOBuffer class.
TEST_F(SpdySessionSpdy2Test, SpdyIOBuffer) {
  std::priority_queue<SpdyIOBuffer> queue_;
  const size_t kQueueSize = 100;

  // Insert items with random priority and increasing buffer size.
  for (size_t index = 0; index < kQueueSize; ++index) {
    queue_.push(SpdyIOBuffer(
        new IOBufferWithSize(index + 1),
        index + 1,
        static_cast<RequestPriority>(rand() % NUM_PRIORITIES),
        NULL));
  }

  EXPECT_EQ(kQueueSize, queue_.size());

  // Verify items come out with decreasing priority or FIFO order.
  RequestPriority last_priority = NUM_PRIORITIES;
  size_t last_size = 0;
  for (size_t index = 0; index < kQueueSize; ++index) {
    SpdyIOBuffer buffer = queue_.top();
    EXPECT_LE(buffer.priority(), last_priority);
    if (buffer.priority() < last_priority)
      last_size = 0;
    EXPECT_LT(last_size, buffer.size());
    last_priority = buffer.priority();
    last_size = buffer.size();
    queue_.pop();
  }

  EXPECT_EQ(0u, queue_.size());
}

TEST_F(SpdySessionSpdy2Test, GoAway) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> goaway(ConstructSpdyGoAway());
  MockRead reads[] = {
    CreateMockRead(*goaway),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  ssl.SetNextProto(kProtoSPDY2);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  HostPortProxyPair pair(test_host_port_pair, ProxyServer::Direct());

  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  EXPECT_FALSE(spdy_session_pool->HasSession(pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(pair, BoundNetLog());
  EXPECT_TRUE(spdy_session_pool->HasSession(pair));

  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                                MEDIUM,
                                false,
                                false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair.ToString(),
                                 transport_params, MEDIUM, CompletionCallback(),
                                 http_session->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));
  EXPECT_EQ(2, session->GetProtocolVersion());

  // Flush the SpdySession::OnReadComplete() task.
  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(spdy_session_pool->HasSession(pair));

  scoped_refptr<SpdySession> session2 =
      spdy_session_pool->Get(pair, BoundNetLog());

  // Delete the first session.
  session = NULL;

  // Delete the second session.
  spdy_session_pool->Remove(session2);
  session2 = NULL;
}

TEST_F(SpdySessionSpdy2Test, Ping) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> read_ping(ConstructSpdyPing());
  MockRead reads[] = {
    CreateMockRead(*read_ping),
    CreateMockRead(*read_ping),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  scoped_ptr<SpdyFrame> write_ping(ConstructSpdyPing());
  MockRead writes[] = {
    CreateMockRead(*write_ping),
    CreateMockRead(*write_ping),
  };
  StaticSocketDataProvider data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  static const char kStreamUrl[] = "http://www.google.com/";
  GURL url(kStreamUrl);

  const std::string kTestHost("www.google.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  HostPortProxyPair pair(test_host_port_pair, ProxyServer::Direct());

  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  EXPECT_FALSE(spdy_session_pool->HasSession(pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(pair, BoundNetLog());
  EXPECT_TRUE(spdy_session_pool->HasSession(pair));


  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                                MEDIUM,
                                false,
                                false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair.ToString(),
                                 transport_params, MEDIUM, CompletionCallback(),
                                 http_session->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

  scoped_refptr<SpdyStream> spdy_stream1;
  TestCompletionCallback callback1;
  EXPECT_EQ(OK, session->CreateStream(url,
                                      MEDIUM,
                                      &spdy_stream1,
                                      BoundNetLog(),
                                      callback1.callback()));
  scoped_ptr<TestSpdyStreamDelegate> delegate(
      new TestSpdyStreamDelegate(callback1.callback()));
  spdy_stream1->SetDelegate(delegate.get());

  base::TimeTicks before_ping_time = base::TimeTicks::Now();

  // Enable sending of PING.
  SpdySession::set_enable_ping_based_connection_checking(true);
  session->set_connection_at_risk_of_loss_time(base::TimeDelta::FromSeconds(0));
  session->set_trailing_ping_delay_time(base::TimeDelta::FromSeconds(0));
  session->set_hung_interval(base::TimeDelta::FromMilliseconds(50));

  session->SendPrefacePingIfNoneInFlight();

  EXPECT_EQ(OK, callback1.WaitForResult());

  session->CheckPingStatus(before_ping_time);

  EXPECT_EQ(0, session->pings_in_flight());
  EXPECT_GT(session->next_ping_id(), static_cast<uint32>(1));
  EXPECT_FALSE(session->trailing_ping_pending());
  EXPECT_FALSE(session->check_ping_status_pending());
  EXPECT_GE(session->received_data_time(), before_ping_time);

  EXPECT_FALSE(spdy_session_pool->HasSession(pair));

  // Delete the first session.
  session = NULL;
}

TEST_F(SpdySessionSpdy2Test, FailedPing) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> read_ping(ConstructSpdyPing());
  MockRead reads[] = {
    CreateMockRead(*read_ping),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  scoped_ptr<SpdyFrame> write_ping(ConstructSpdyPing());
  MockRead writes[] = {
    CreateMockRead(*write_ping),
  };
  StaticSocketDataProvider data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  static const char kStreamUrl[] = "http://www.gmail.com/";
  GURL url(kStreamUrl);

  const std::string kTestHost("www.gmail.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  HostPortProxyPair pair(test_host_port_pair, ProxyServer::Direct());

  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  EXPECT_FALSE(spdy_session_pool->HasSession(pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(pair, BoundNetLog());
  EXPECT_TRUE(spdy_session_pool->HasSession(pair));

  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                                MEDIUM,
                                false,
                                false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair.ToString(),
                                 transport_params, MEDIUM, CompletionCallback(),
                                 http_session->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

  scoped_refptr<SpdyStream> spdy_stream1;
  TestCompletionCallback callback1;
  EXPECT_EQ(OK, session->CreateStream(url,
                                      MEDIUM,
                                      &spdy_stream1,
                                      BoundNetLog(),
                                      callback1.callback()));
  scoped_ptr<TestSpdyStreamDelegate> delegate(
      new TestSpdyStreamDelegate(callback1.callback()));
  spdy_stream1->SetDelegate(delegate.get());

  // Enable sending of PING.
  SpdySession::set_enable_ping_based_connection_checking(true);
  session->set_connection_at_risk_of_loss_time(base::TimeDelta::FromSeconds(0));
  session->set_trailing_ping_delay_time(base::TimeDelta::FromSeconds(0));
  session->set_hung_interval(base::TimeDelta::FromSeconds(0));

  // Send a PING frame.
  session->WritePingFrame(1);
  EXPECT_LT(0, session->pings_in_flight());
  EXPECT_GT(session->next_ping_id(), static_cast<uint32>(1));
  EXPECT_TRUE(session->check_ping_status_pending());

  // Assert session is not closed.
  EXPECT_FALSE(session->IsClosed());
  EXPECT_LT(0u, session->num_active_streams());
  EXPECT_TRUE(spdy_session_pool->HasSession(pair));

  // We set last time we have received any data in 1 sec less than now.
  // CheckPingStatus will trigger timeout because hung interval is zero.
  base::TimeTicks now = base::TimeTicks::Now();
  session->received_data_time_ = now - base::TimeDelta::FromSeconds(1);
  session->CheckPingStatus(now);

  EXPECT_TRUE(session->IsClosed());
  EXPECT_EQ(0u, session->num_active_streams());
  EXPECT_EQ(0u, session->num_unclaimed_pushed_streams());
  EXPECT_FALSE(spdy_session_pool->HasSession(pair));

  // Delete the first session.
  session = NULL;
}

class StreamReleaserCallback : public TestCompletionCallbackBase {
 public:
  StreamReleaserCallback(SpdySession* session,
                         SpdyStream* first_stream)
      : session_(session),
        first_stream_(first_stream),
        ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
            base::Bind(&StreamReleaserCallback::OnComplete,
                       base::Unretained(this)))) {
  }

  virtual ~StreamReleaserCallback() {}

  scoped_refptr<SpdyStream>* stream() { return &stream_; }

  const CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result) {
    session_->CloseSessionOnError(ERR_FAILED, false, "On complete.");
    session_ = NULL;
    first_stream_->Cancel();
    first_stream_ = NULL;
    stream_->Cancel();
    stream_ = NULL;
    SetResult(result);
  }

  scoped_refptr<SpdySession> session_;
  scoped_refptr<SpdyStream> first_stream_;
  scoped_refptr<SpdyStream> stream_;
  CompletionCallback callback_;
};

// TODO(kristianm): Could also test with more sessions where some are idle,
// and more than one session to a HostPortPair.
TEST_F(SpdySessionSpdy2Test, CloseIdleSessions) {
  SpdySessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> http_session(
  SpdySessionDependencies::SpdyCreateSession(&session_deps));
  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());

  // Set up session 1
  const std::string kTestHost1("http://www.a.com");
  HostPortPair test_host_port_pair1(kTestHost1, 80);
  HostPortProxyPair pair1(test_host_port_pair1, ProxyServer::Direct());
  scoped_refptr<SpdySession> session1 =
      spdy_session_pool->Get(pair1, BoundNetLog());
  scoped_refptr<SpdyStream> spdy_stream1;
  TestCompletionCallback callback1;
  GURL url1(kTestHost1);
  EXPECT_EQ(OK, session1->CreateStream(url1,
                                      MEDIUM, /* priority, not important */
                                      &spdy_stream1,
                                      BoundNetLog(),
                                      callback1.callback()));

  // Set up session 2
  const std::string kTestHost2("http://www.b.com");
  HostPortPair test_host_port_pair2(kTestHost2, 80);
  HostPortProxyPair pair2(test_host_port_pair2, ProxyServer::Direct());
  scoped_refptr<SpdySession> session2 =
      spdy_session_pool->Get(pair2, BoundNetLog());
  scoped_refptr<SpdyStream> spdy_stream2;
  TestCompletionCallback callback2;
  GURL url2(kTestHost2);
  EXPECT_EQ(OK, session2->CreateStream(
      url2, MEDIUM, /* priority, not important */
      &spdy_stream2, BoundNetLog(), callback2.callback()));

  // Set up session 3
  const std::string kTestHost3("http://www.c.com");
  HostPortPair test_host_port_pair3(kTestHost3, 80);
  HostPortProxyPair pair3(test_host_port_pair3, ProxyServer::Direct());
  scoped_refptr<SpdySession> session3 =
      spdy_session_pool->Get(pair3, BoundNetLog());
  scoped_refptr<SpdyStream> spdy_stream3;
  TestCompletionCallback callback3;
  GURL url3(kTestHost3);
  EXPECT_EQ(OK, session3->CreateStream(
      url3, MEDIUM, /* priority, not important */
      &spdy_stream3, BoundNetLog(), callback3.callback()));

  // All sessions are active and not closed
  EXPECT_TRUE(session1->is_active());
  EXPECT_FALSE(session1->IsClosed());
  EXPECT_TRUE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());
  EXPECT_TRUE(session3->is_active());
  EXPECT_FALSE(session3->IsClosed());

  // Should not do anything, all are active
  spdy_session_pool->CloseIdleSessions();
  EXPECT_TRUE(session1->is_active());
  EXPECT_FALSE(session1->IsClosed());
  EXPECT_TRUE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());
  EXPECT_TRUE(session3->is_active());
  EXPECT_FALSE(session3->IsClosed());

  // Make sessions 1 and 3 inactive, but keep them open.
  // Session 2 still open and active
  session1->CloseStream(spdy_stream1->stream_id(), OK);
  session3->CloseStream(spdy_stream3->stream_id(), OK);
  EXPECT_FALSE(session1->is_active());
  EXPECT_FALSE(session1->IsClosed());
  EXPECT_TRUE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());
  EXPECT_FALSE(session3->is_active());
  EXPECT_FALSE(session3->IsClosed());

  // Should close session 1 and 3, 2 should be left open
  spdy_session_pool->CloseIdleSessions();
  EXPECT_FALSE(session1->is_active());
  EXPECT_TRUE(session1->IsClosed());
  EXPECT_TRUE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());
  EXPECT_FALSE(session3->is_active());
  EXPECT_TRUE(session3->IsClosed());

  // Should not do anything
  spdy_session_pool->CloseIdleSessions();
  EXPECT_TRUE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());

  // Make 2 not active
  session2->CloseStream(spdy_stream2->stream_id(), OK);
  EXPECT_FALSE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());

  // This should close session 2
  spdy_session_pool->CloseIdleSessions();
  EXPECT_FALSE(session2->is_active());
  EXPECT_TRUE(session2->IsClosed());
}

// Start with max concurrent streams set to 1.  Request two streams.  Receive a
// settings frame setting max concurrent streams to 2.  Have the callback
// release the stream, which releases its reference (the last) to the session.
// Make sure nothing blows up.
// http://crbug.com/57331
TEST_F(SpdySessionSpdy2Test, OnSettings) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  SettingsMap new_settings;
  const SpdySettingsIds kSpdySettingsIds1 = SETTINGS_MAX_CONCURRENT_STREAMS;
  const size_t max_concurrent_streams = 2;
  new_settings[kSpdySettingsIds1] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, max_concurrent_streams);

  // Set up the socket so we read a SETTINGS frame that raises max concurrent
  // streams to 2.
  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> settings_frame(ConstructSpdySettings(new_settings));
  MockRead reads[] = {
    CreateMockRead(*settings_frame),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  HostPortProxyPair pair(test_host_port_pair, ProxyServer::Direct());

  // Initialize the SpdySetting with 1 max concurrent streams.
  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  spdy_session_pool->http_server_properties()->SetSpdySetting(
      test_host_port_pair,
      kSpdySettingsIds1,
      SETTINGS_FLAG_PLEASE_PERSIST,
      1);

  // Create a session.
  EXPECT_FALSE(spdy_session_pool->HasSession(pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(pair, BoundNetLog());
  ASSERT_TRUE(spdy_session_pool->HasSession(pair));

  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                                MEDIUM,
                                false,
                                false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair.ToString(),
                                 transport_params, MEDIUM, CompletionCallback(),
                                 http_session->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

  // Create 2 streams.  First will succeed.  Second will be pending.
  scoped_refptr<SpdyStream> spdy_stream1;
  TestCompletionCallback callback1;
  GURL url("http://www.google.com");
  EXPECT_EQ(OK,
            session->CreateStream(url,
                                  MEDIUM, /* priority, not important */
                                  &spdy_stream1,
                                  BoundNetLog(),
                                  callback1.callback()));

  StreamReleaserCallback stream_releaser(session, spdy_stream1);

  ASSERT_EQ(ERR_IO_PENDING,
            session->CreateStream(url,
                                  MEDIUM, /* priority, not important */
                                  stream_releaser.stream(),
                                  BoundNetLog(),
                                  stream_releaser.callback()));

  // Make sure |stream_releaser| holds the last refs.
  session = NULL;
  spdy_stream1 = NULL;

  EXPECT_EQ(OK, stream_releaser.WaitForResult());
}

// Start with max concurrent streams set to 1.  Request two streams.  When the
// first completes, have the callback close itself, which should trigger the
// second stream creation.  Then cancel that one immediately.  Don't crash.
// http://crbug.com/63532
TEST_F(SpdySessionSpdy2Test, CancelPendingCreateStream) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  MockConnect connect_data(SYNCHRONOUS, OK);

  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  HostPortProxyPair pair(test_host_port_pair, ProxyServer::Direct());

  // Initialize the SpdySetting with 1 max concurrent streams.
  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  spdy_session_pool->http_server_properties()->SetSpdySetting(
      test_host_port_pair,
      SETTINGS_MAX_CONCURRENT_STREAMS,
      SETTINGS_FLAG_PLEASE_PERSIST,
      1);

  // Create a session.
  EXPECT_FALSE(spdy_session_pool->HasSession(pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(pair, BoundNetLog());
  ASSERT_TRUE(spdy_session_pool->HasSession(pair));

  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                                MEDIUM,
                                false,
                                false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair.ToString(),
                                 transport_params, MEDIUM, CompletionCallback(),
                                 http_session->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

  // Use scoped_ptr to let us invalidate the memory when we want to, to trigger
  // a valgrind error if the callback is invoked when it's not supposed to be.
  scoped_ptr<TestCompletionCallback> callback(new TestCompletionCallback);

  // Create 2 streams.  First will succeed.  Second will be pending.
  scoped_refptr<SpdyStream> spdy_stream1;
  GURL url("http://www.google.com");
  ASSERT_EQ(OK,
            session->CreateStream(url,
                                  MEDIUM, /* priority, not important */
                                  &spdy_stream1,
                                  BoundNetLog(),
                                  callback->callback()));

  scoped_refptr<SpdyStream> spdy_stream2;
  ASSERT_EQ(ERR_IO_PENDING,
            session->CreateStream(url,
                                  MEDIUM, /* priority, not important */
                                  &spdy_stream2,
                                  BoundNetLog(),
                                  callback->callback()));

  // Release the first one, this will allow the second to be created.
  spdy_stream1->Cancel();
  spdy_stream1 = NULL;

  session->CancelPendingCreateStreams(&spdy_stream2);
  callback.reset();

  // Should not crash when running the pending callback.
  MessageLoop::current()->RunAllPending();
}

TEST_F(SpdySessionSpdy2Test, SendSettingsOnNewSession) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  // Create the bogus setting that we want to verify is sent out.
  // Note that it will be marked as SETTINGS_FLAG_PERSISTED when sent out. But
  // to persist it into the HttpServerProperties, we need to mark as
  // SETTINGS_FLAG_PLEASE_PERSIST.
  SettingsMap settings;
  const SpdySettingsIds kSpdySettingsIds1 = SETTINGS_UPLOAD_BANDWIDTH;
  const uint32 kBogusSettingValue = 0xCDCD;
  settings[kSpdySettingsIds1] =
      SettingsFlagsAndValue(SETTINGS_FLAG_PERSISTED, kBogusSettingValue);
  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> settings_frame(ConstructSpdySettings(settings));
  MockWrite writes[] = {
    CreateMockWrite(*settings_frame),
  };

  StaticSocketDataProvider data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  HostPortProxyPair pair(test_host_port_pair, ProxyServer::Direct());

  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  spdy_session_pool->http_server_properties()->SetSpdySetting(
      test_host_port_pair,
      kSpdySettingsIds1,
      SETTINGS_FLAG_PLEASE_PERSIST,
      kBogusSettingValue);

  EXPECT_FALSE(spdy_session_pool->HasSession(pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(pair, BoundNetLog());
  EXPECT_TRUE(spdy_session_pool->HasSession(pair));

  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                                MEDIUM,
                                false,
                                false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair.ToString(),
                                 transport_params, MEDIUM, CompletionCallback(),
                                 http_session->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(data.at_write_eof());
}

namespace {
// This test has two variants, one for each style of closing the connection.
// If |clean_via_close_current_sessions| is false, the sessions are closed
// manually, calling SpdySessionPool::Remove() directly.  If it is true,
// sessions are closed with SpdySessionPool::CloseCurrentSessions().
void IPPoolingTest(bool clean_via_close_current_sessions) {
  const int kTestPort = 80;
  struct TestHosts {
    std::string name;
    std::string iplist;
    HostPortProxyPair pair;
    AddressList addresses;
  } test_hosts[] = {
    { "www.foo.com",    "192.0.2.33,192.168.0.1,192.168.0.5" },
    { "images.foo.com", "192.168.0.2,192.168.0.3,192.168.0.5,192.0.2.33" },
    { "js.foo.com",     "192.168.0.4,192.168.0.3" },
  };

  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_hosts); i++) {
    session_deps.host_resolver->rules()->AddIPLiteralRule(test_hosts[i].name,
        test_hosts[i].iplist, "");

    // This test requires that the HostResolver cache be populated.  Normal
    // code would have done this already, but we do it manually.
    HostResolver::RequestInfo info(HostPortPair(test_hosts[i].name, kTestPort));
    session_deps.host_resolver->Resolve(
        info, &test_hosts[i].addresses, CompletionCallback(), NULL,
        BoundNetLog());

    // Setup a HostPortProxyPair
    test_hosts[i].pair = HostPortProxyPair(
        HostPortPair(test_hosts[i].name, kTestPort), ProxyServer::Direct());
  }

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Setup the first session to the first host.
  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[0].pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(test_hosts[0].pair, BoundNetLog());
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[0].pair));

  HostPortPair test_host_port_pair(test_hosts[0].name, kTestPort);
  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                          MEDIUM,
                          false,
                          false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair.ToString(),
                                 transport_params, MEDIUM, CompletionCallback(),
                                 http_session->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

  // TODO(rtenneti): MockClientSocket::GetPeerAddress return's 0 as the port
  // number. Fix it to return port 80 and then use GetPeerAddress to AddAlias.
  SpdySessionPoolPeer pool_peer(spdy_session_pool);
  pool_peer.AddAlias(test_hosts[0].addresses.front(), test_hosts[0].pair);

  // Flush the SpdySession::OnReadComplete() task.
  MessageLoop::current()->RunAllPending();

  // The third host has no overlap with the first, so it can't pool IPs.
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[2].pair));

  // The second host overlaps with the first, and should IP pool.
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[1].pair));

  // Verify that the second host, through a proxy, won't share the IP.
  HostPortProxyPair proxy_pair(test_hosts[1].pair.first,
      ProxyServer::FromPacString("HTTP http://proxy.foo.com/"));
  EXPECT_FALSE(spdy_session_pool->HasSession(proxy_pair));

  // Overlap between 2 and 3 does is not transitive to 1.
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[2].pair));

  // Create a new session to host 2.
  scoped_refptr<SpdySession> session2 =
      spdy_session_pool->Get(test_hosts[2].pair, BoundNetLog());

  // Verify that we have sessions for everything.
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[0].pair));
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[1].pair));
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[2].pair));

  // Grab the session to host 1 and verify that it is the same session
  // we got with host 0, and that is a different than host 2's session.
  scoped_refptr<SpdySession> session1 =
      spdy_session_pool->Get(test_hosts[1].pair, BoundNetLog());
  EXPECT_EQ(session.get(), session1.get());
  EXPECT_NE(session2.get(), session1.get());

  // Remove the aliases and observe that we still have a session for host1.
  pool_peer.RemoveAliases(test_hosts[0].pair);
  pool_peer.RemoveAliases(test_hosts[1].pair);
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[1].pair));

  // Expire the host cache
  session_deps.host_resolver->GetHostCache()->clear();
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[1].pair));

  // Cleanup the sessions.
  if (!clean_via_close_current_sessions) {
    spdy_session_pool->Remove(session);
    session = NULL;
    spdy_session_pool->Remove(session2);
    session2 = NULL;
  } else {
    spdy_session_pool->CloseCurrentSessions();
  }

  // Verify that the map is all cleaned up.
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[0].pair));
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[1].pair));
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[2].pair));
}

}  // namespace

TEST_F(SpdySessionSpdy2Test, IPPooling) {
  IPPoolingTest(false);
}

TEST_F(SpdySessionSpdy2Test, IPPoolingCloseCurrentSessions) {
  IPPoolingTest(true);
}

TEST_F(SpdySessionSpdy2Test, ClearSettingsStorageOnIPAddressChanged) {
  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);

  SpdySessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));
  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());

  HttpServerProperties* test_http_server_properties =
      spdy_session_pool->http_server_properties();
  SettingsFlagsAndValue flags_and_value1(SETTINGS_FLAG_PLEASE_PERSIST, 2);
  test_http_server_properties->SetSpdySetting(
      test_host_port_pair,
      SETTINGS_MAX_CONCURRENT_STREAMS,
      SETTINGS_FLAG_PLEASE_PERSIST,
      2);
  EXPECT_NE(0u, test_http_server_properties->GetSpdySettings(
      test_host_port_pair).size());
  spdy_session_pool->OnIPAddressChanged();
  EXPECT_EQ(0u, test_http_server_properties->GetSpdySettings(
      test_host_port_pair).size());
}

TEST_F(SpdySessionSpdy2Test, NeedsCredentials) {
  SpdySessionDependencies session_deps;

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  ssl.domain_bound_cert_type = CLIENT_CERT_ECDSA_SIGN;
  ssl.protocol_negotiated = kProtoSPDY2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  HostPortProxyPair pair(test_host_port_pair, ProxyServer::Direct());

  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  EXPECT_FALSE(spdy_session_pool->HasSession(pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(pair, BoundNetLog());
  EXPECT_TRUE(spdy_session_pool->HasSession(pair));

  SSLConfig ssl_config;
  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                                MEDIUM,
                                false,
                                false));
  scoped_refptr<SOCKSSocketParams> socks_params;
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SSLSocketParams> ssl_params(
      new SSLSocketParams(transport_params,
                          socks_params,
                          http_proxy_params,
                          ProxyServer::SCHEME_DIRECT,
                          test_host_port_pair,
                          ssl_config,
                          0,
                          false,
                          false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair.ToString(),
                                 ssl_params, MEDIUM, CompletionCallback(),
                                 http_session->GetSSLSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));

  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), true, OK));

  EXPECT_FALSE(session->NeedsCredentials());

  // Flush the SpdySession::OnReadComplete() task.
  MessageLoop::current()->RunAllPending();

  spdy_session_pool->Remove(session);
  EXPECT_FALSE(spdy_session_pool->HasSession(pair));
}

TEST_F(SpdySessionSpdy2Test, CloseSessionOnError) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> goaway(ConstructSpdyGoAway());
  MockRead reads[] = {
    CreateMockRead(*goaway),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };

  net::CapturingBoundNetLog log(net::CapturingNetLog::kUnbounded);

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  HostPortProxyPair pair(test_host_port_pair, ProxyServer::Direct());

  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  EXPECT_FALSE(spdy_session_pool->HasSession(pair));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(pair, log.bound());
  EXPECT_TRUE(spdy_session_pool->HasSession(pair));

  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                                MEDIUM,
                                false,
                                false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair.ToString(),
                                 transport_params, MEDIUM, CompletionCallback(),
                                 http_session->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 log.bound()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

  // Flush the SpdySession::OnReadComplete() task.
  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(spdy_session_pool->HasSession(pair));

  // Check that the NetLog was filled reasonably.
  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged SPDY_SESSION_CLOSE correctly.
  int pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_SPDY_SESSION_CLOSE,
      net::NetLog::PHASE_NONE);

  CapturingNetLog::Entry entry = entries[pos];
  NetLogSpdySessionCloseParameter* request_params =
      static_cast<NetLogSpdySessionCloseParameter*>(
          entry.extra_parameters.get());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, request_params->status());
}

}  // namespace net
