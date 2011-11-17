// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session.h"

#include "net/base/ip_endpoint.h"
#include "net/spdy/spdy_io_buffer.h"
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
 protected:
  virtual void TearDown() {
    // Wanted to be 100% sure PING is disabled.
    SpdySession::set_enable_ping_based_connection_checking(false);
  }
};

class TestSpdyStreamDelegate : public net::SpdyStream::Delegate {
 public:
  explicit TestSpdyStreamDelegate(OldCompletionCallback* callback)
      : callback_(callback) {}
  virtual ~TestSpdyStreamDelegate() {}

  virtual bool OnSendHeadersComplete(int status) { return true; }

  virtual int OnSendBody() {
    return ERR_UNEXPECTED;
  }

  virtual int OnSendBodyComplete(int /*status*/, bool* /*eof*/) {
    return ERR_UNEXPECTED;
  }

  virtual int OnResponseReceived(const spdy::SpdyHeaderBlock& response,
                                 base::Time response_time,
                                 int status) {
    return status;
  }

  virtual void OnDataReceived(const char* buffer, int bytes) {
  }

  virtual void OnDataSent(int length) {
  }

  virtual void OnClose(int status) {
    OldCompletionCallback* callback = callback_;
    callback_ = NULL;
    callback->Run(OK);
  }

  virtual void set_chunk_callback(net::ChunkCallback *) {}

 private:
  OldCompletionCallback* callback_;
};


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
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
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
  EXPECT_EQ(OK,
            connection->Init(test_host_port_pair.ToString(),
                             transport_params, MEDIUM,
                             NULL, http_session->GetTransportSocketPool(),
                             BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

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

TEST_F(SpdySessionTest, Ping) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(false, OK);
  scoped_ptr<spdy::SpdyFrame> read_ping(ConstructSpdyPing());
  MockRead reads[] = {
    CreateMockRead(*read_ping),
    CreateMockRead(*read_ping),
    MockRead(false, 0, 0)  // EOF
  };
  scoped_ptr<spdy::SpdyFrame> write_ping(ConstructSpdyPing());
  MockRead writes[] = {
    CreateMockRead(*write_ping),
    CreateMockRead(*write_ping),
  };
  StaticSocketDataProvider data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
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
  EXPECT_EQ(OK,
            connection->Init(test_host_port_pair.ToString(),
                             transport_params,
                             MEDIUM,
                             NULL,
                             http_session->GetTransportSocketPool(),
                             BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

  scoped_refptr<SpdyStream> spdy_stream1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(OK, session->CreateStream(url,
                                      MEDIUM,
                                      &spdy_stream1,
                                      BoundNetLog(),
                                      &callback1));
  scoped_ptr<TestSpdyStreamDelegate> delegate(
      new TestSpdyStreamDelegate(&callback1));
  spdy_stream1->SetDelegate(delegate.get());

  base::TimeTicks before_ping_time = base::TimeTicks::Now();

  // Enable sending of PING.
  SpdySession::set_enable_ping_based_connection_checking(true);
  SpdySession::set_connection_at_risk_of_loss_seconds(0);
  SpdySession::set_trailing_ping_delay_time_ms(0);
  SpdySession::set_hung_interval_ms(50);

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

TEST_F(SpdySessionTest, FailedPing) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(false, OK);
  scoped_ptr<spdy::SpdyFrame> read_ping(ConstructSpdyPing());
  MockRead reads[] = {
    CreateMockRead(*read_ping),
    MockRead(false, 0, 0)  // EOF
  };
  scoped_ptr<spdy::SpdyFrame> write_ping(ConstructSpdyPing());
  MockRead writes[] = {
    CreateMockRead(*write_ping),
  };
  StaticSocketDataProvider data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
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
  EXPECT_EQ(OK,
            connection->Init(test_host_port_pair.ToString(),
                             transport_params,
                             MEDIUM,
                             NULL,
                             http_session->GetTransportSocketPool(),
                             BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

  scoped_refptr<SpdyStream> spdy_stream1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(OK, session->CreateStream(url,
                                      MEDIUM,
                                      &spdy_stream1,
                                      BoundNetLog(),
                                      &callback1));
  scoped_ptr<TestSpdyStreamDelegate> delegate(
      new TestSpdyStreamDelegate(&callback1));
  spdy_stream1->SetDelegate(delegate.get());

  // Enable sending of PING.
  SpdySession::set_enable_ping_based_connection_checking(true);
  SpdySession::set_connection_at_risk_of_loss_seconds(0);
  SpdySession::set_trailing_ping_delay_time_ms(0);
  SpdySession::set_hung_interval_ms(0);

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

class StreamReleaserCallback : public CallbackRunner<Tuple1<int> > {
 public:
  StreamReleaserCallback(SpdySession* session,
                         SpdyStream* first_stream)
      : session_(session), first_stream_(first_stream) {}
  ~StreamReleaserCallback() {}

  int WaitForResult() { return callback_.WaitForResult(); }

  virtual void RunWithParams(const Tuple1<int>& params) {
    session_->CloseSessionOnError(ERR_FAILED, false);
    session_ = NULL;
    first_stream_->Cancel();
    first_stream_ = NULL;
    stream_->Cancel();
    stream_ = NULL;
    callback_.RunWithParams(params);
  }

  scoped_refptr<SpdyStream>* stream() { return &stream_; }

 private:
  scoped_refptr<SpdySession> session_;
  scoped_refptr<SpdyStream> first_stream_;
  scoped_refptr<SpdyStream> stream_;
  TestOldCompletionCallback callback_;
};

// TODO(kristianm): Could also test with more sessions where some are idle,
// and more than one session to a HostPortPair.
TEST_F(SpdySessionTest, CloseIdleSessions) {
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
  TestOldCompletionCallback callback1;
  GURL url1(kTestHost1);
  EXPECT_EQ(OK, session1->CreateStream(url1,
                                      MEDIUM, /* priority, not important */
                                      &spdy_stream1,
                                      BoundNetLog(),
                                      &callback1));

  // Set up session 2
  const std::string kTestHost2("http://www.b.com");
  HostPortPair test_host_port_pair2(kTestHost2, 80);
  HostPortProxyPair pair2(test_host_port_pair2, ProxyServer::Direct());
  scoped_refptr<SpdySession> session2 =
      spdy_session_pool->Get(pair2, BoundNetLog());
  scoped_refptr<SpdyStream> spdy_stream2;
  TestOldCompletionCallback callback2;
  GURL url2(kTestHost2);
  EXPECT_EQ(OK, session2->CreateStream(url2,
                                      MEDIUM, /* priority, not important */
                                      &spdy_stream2,
                                      BoundNetLog(),
                                      &callback2));

  // Set up session 3
  const std::string kTestHost3("http://www.c.com");
  HostPortPair test_host_port_pair3(kTestHost3, 80);
  HostPortProxyPair pair3(test_host_port_pair3, ProxyServer::Direct());
  scoped_refptr<SpdySession> session3 =
      spdy_session_pool->Get(pair3, BoundNetLog());
  scoped_refptr<SpdyStream> spdy_stream3;
  TestOldCompletionCallback callback3;
  GURL url3(kTestHost3);
  EXPECT_EQ(OK, session3->CreateStream(url3,
                                      MEDIUM, /* priority, not important */
                                      &spdy_stream3,
                                      BoundNetLog(),
                                      &callback3));

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
TEST_F(SpdySessionTest, OnSettings) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  spdy::SpdySettings new_settings;
  spdy::SettingsFlagsAndId id(0);
  id.set_id(spdy::SETTINGS_MAX_CONCURRENT_STREAMS);
  const size_t max_concurrent_streams = 2;
  new_settings.push_back(spdy::SpdySetting(id, max_concurrent_streams));

  // Set up the socket so we read a SETTINGS frame that raises max concurrent
  // streams to 2.
  MockConnect connect_data(false, OK);
  scoped_ptr<spdy::SpdyFrame> settings_frame(
      ConstructSpdySettings(new_settings));
  MockRead reads[] = {
    CreateMockRead(*settings_frame),
    MockRead(false, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  HostPortProxyPair pair(test_host_port_pair, ProxyServer::Direct());

  // Initialize the SpdySettingsStorage with 1 max concurrent streams.
  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  spdy::SpdySettings old_settings;
  id.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
  old_settings.push_back(spdy::SpdySetting(id, 1));
  spdy_session_pool->http_server_properties()->SetSpdySettings(
      test_host_port_pair, old_settings);

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
  EXPECT_EQ(OK,
            connection->Init(test_host_port_pair.ToString(),
                             transport_params, MEDIUM,
                             NULL, http_session->GetTransportSocketPool(),
                             BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

  // Create 2 streams.  First will succeed.  Second will be pending.
  scoped_refptr<SpdyStream> spdy_stream1;
  TestOldCompletionCallback callback1;
  GURL url("http://www.google.com");
  EXPECT_EQ(OK,
            session->CreateStream(url,
                                  MEDIUM, /* priority, not important */
                                  &spdy_stream1,
                                  BoundNetLog(),
                                  &callback1));

  StreamReleaserCallback stream_releaser(session, spdy_stream1);

  ASSERT_EQ(ERR_IO_PENDING,
            session->CreateStream(url,
                                  MEDIUM, /* priority, not important */
                                  stream_releaser.stream(),
                                  BoundNetLog(),
                                  &stream_releaser));

  // Make sure |stream_releaser| holds the last refs.
  session = NULL;
  spdy_stream1 = NULL;

  EXPECT_EQ(OK, stream_releaser.WaitForResult());
}

// Start with max concurrent streams set to 1.  Request two streams.  When the
// first completes, have the callback close itself, which should trigger the
// second stream creation.  Then cancel that one immediately.  Don't crash.
// http://crbug.com/63532
TEST_F(SpdySessionTest, CancelPendingCreateStream) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockRead reads[] = {
    MockRead(false, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  MockConnect connect_data(false, OK);

  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  HostPortProxyPair pair(test_host_port_pair, ProxyServer::Direct());

  // Initialize the SpdySettingsStorage with 1 max concurrent streams.
  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  spdy::SpdySettings settings;
  spdy::SettingsFlagsAndId id(0);
  id.set_id(spdy::SETTINGS_MAX_CONCURRENT_STREAMS);
  id.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
  settings.push_back(spdy::SpdySetting(id, 1));
  spdy_session_pool->http_server_properties()->SetSpdySettings(
      test_host_port_pair, settings);

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
  EXPECT_EQ(OK,
            connection->Init(test_host_port_pair.ToString(),
                             transport_params, MEDIUM,
                             NULL, http_session->GetTransportSocketPool(),
                             BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

  // Use scoped_ptr to let us invalidate the memory when we want to, to trigger
  // a valgrind error if the callback is invoked when it's not supposed to be.
  scoped_ptr<TestOldCompletionCallback> callback(new TestOldCompletionCallback);

  // Create 2 streams.  First will succeed.  Second will be pending.
  scoped_refptr<SpdyStream> spdy_stream1;
  GURL url("http://www.google.com");
  ASSERT_EQ(OK,
            session->CreateStream(url,
                                  MEDIUM, /* priority, not important */
                                  &spdy_stream1,
                                  BoundNetLog(),
                                  callback.get()));

  scoped_refptr<SpdyStream> spdy_stream2;
  ASSERT_EQ(ERR_IO_PENDING,
            session->CreateStream(url,
                                  MEDIUM, /* priority, not important */
                                  &spdy_stream2,
                                  BoundNetLog(),
                                  callback.get()));

  // Release the first one, this will allow the second to be created.
  spdy_stream1->Cancel();
  spdy_stream1 = NULL;

  session->CancelPendingCreateStreams(&spdy_stream2);
  callback.reset();

  // Should not crash when running the pending callback.
  MessageLoop::current()->RunAllPending();
}

TEST_F(SpdySessionTest, SendSettingsOnNewSession) {
  SpdySessionDependencies session_deps;
  session_deps.host_resolver->set_synchronous_mode(true);

  MockRead reads[] = {
    MockRead(false, ERR_IO_PENDING)  // Stall forever.
  };

  // Create the bogus setting that we want to verify is sent out.
  // Note that it will be marked as SETTINGS_FLAG_PERSISTED when sent out.  But
  // to set it into the SpdySettingsStorage, we need to mark as
  // SETTINGS_FLAG_PLEASE_PERSIST.
  spdy::SpdySettings settings;
  const uint32 kBogusSettingId = 0xABAB;
  const uint32 kBogusSettingValue = 0xCDCD;
  spdy::SettingsFlagsAndId id(0);
  id.set_id(kBogusSettingId);
  id.set_flags(spdy::SETTINGS_FLAG_PERSISTED);
  settings.push_back(spdy::SpdySetting(id, kBogusSettingValue));
  MockConnect connect_data(false, OK);
  scoped_ptr<spdy::SpdyFrame> settings_frame(
      ConstructSpdySettings(settings));
  MockWrite writes[] = {
    CreateMockWrite(*settings_frame),
  };

  StaticSocketDataProvider data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  HostPortProxyPair pair(test_host_port_pair, ProxyServer::Direct());

  id.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
  settings.clear();
  settings.push_back(spdy::SpdySetting(id, kBogusSettingValue));
  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  spdy_session_pool->http_server_properties()->SetSpdySettings(
      test_host_port_pair, settings);
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
  EXPECT_EQ(OK,
            connection->Init(test_host_port_pair.ToString(),
                             transport_params, MEDIUM,
                             NULL, http_session->GetTransportSocketPool(),
                             BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(data.at_write_eof());
}

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

  MockConnect connect_data(false, OK);
  MockRead reads[] = {
    MockRead(false, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(false, OK);
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
  EXPECT_EQ(OK,
            connection->Init(test_host_port_pair.ToString(),
                             transport_params, MEDIUM,
                             NULL, http_session->GetTransportSocketPool(),
                             BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));

  // TODO(rtenneti): MockClientSocket::GetPeerAddress return's 0 as the port
  // number. Fix it to return port 80 and then use GetPeerAddress to AddAlias.
  const addrinfo* address = test_hosts[0].addresses.head();
  SpdySessionPoolPeer pool_peer(spdy_session_pool);
  pool_peer.AddAlias(address, test_hosts[0].pair);

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

TEST_F(SpdySessionTest, IPPooling) {
  IPPoolingTest(false);
}

TEST_F(SpdySessionTest, IPPoolingCloseCurrentSessions) {
  IPPoolingTest(true);
}

TEST_F(SpdySessionTest, ClearSettingsStorage) {
  SpdySettingsStorage settings_storage;
  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  spdy::SpdySettings test_settings;
  spdy::SettingsFlagsAndId id(0);
  id.set_id(spdy::SETTINGS_MAX_CONCURRENT_STREAMS);
  id.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
  const size_t max_concurrent_streams = 2;
  test_settings.push_back(spdy::SpdySetting(id, max_concurrent_streams));

  settings_storage.Set(test_host_port_pair, test_settings);
  EXPECT_NE(0u, settings_storage.Get(test_host_port_pair).size());
  settings_storage.Clear();
  EXPECT_EQ(0u, settings_storage.Get(test_host_port_pair).size());
}

TEST_F(SpdySessionTest, ClearSettingsStorageOnIPAddressChanged) {
  const std::string kTestHost("www.foo.com");
  const int kTestPort = 80;
  HostPortPair test_host_port_pair(kTestHost, kTestPort);

  SpdySessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));
  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());

  HttpServerProperties* test_http_server_properties =
      spdy_session_pool->http_server_properties();
  spdy::SettingsFlagsAndId id(0);
  id.set_id(spdy::SETTINGS_MAX_CONCURRENT_STREAMS);
  id.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
  const size_t max_concurrent_streams = 2;
  spdy::SpdySettings test_settings;
  test_settings.push_back(spdy::SpdySetting(id, max_concurrent_streams));

  test_http_server_properties->SetSpdySettings(test_host_port_pair,
                                               test_settings);
  EXPECT_NE(0u, test_http_server_properties->GetSpdySettings(
      test_host_port_pair).size());
  spdy_session_pool->OnIPAddressChanged();
  EXPECT_EQ(0u, test_http_server_properties->GetSpdySettings(
      test_host_port_pair).size());
}

}  // namespace net
