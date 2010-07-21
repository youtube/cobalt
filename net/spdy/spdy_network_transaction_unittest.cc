// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_unittest.h"
#include "net/http/http_transaction_unittest.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_network_transaction.h"
#include "net/spdy/spdy_test_util.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

class SpdyNetworkTransactionTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    // By default, all tests turn off compression.
    EnableCompression(false);
    google_get_request_initialized_ = false;
    HttpNetworkTransaction::SetUseAlternateProtocols(true);
    HttpNetworkTransaction::SetNextProtos(
        "\x08http/1.1\x07http1.1\x06spdy/1\x04spdy");
  }

  virtual void TearDown() {
    // Empty the current queue.
    MessageLoop::current()->RunAllPending();
    PlatformTest::TearDown();
  }

  void KeepAliveConnectionResendRequestTest(const MockRead& read_failure);

  struct TransactionHelperResult {
    int rv;
    std::string status_line;
    std::string response_data;
    HttpResponseInfo response_info;
  };

  void EnableCompression(bool enabled) {
    spdy::SpdyFramer::set_enable_compression_default(enabled);
  }

  class StartTransactionCallback;
  class DeleteSessionCallback;

  // A helper class that handles all the initial npn/ssl setup.
  class NormalSpdyTransactionHelper {
   public:
    NormalSpdyTransactionHelper(const HttpRequestInfo& request,
                                const BoundNetLog& log)
        : request_(request),
          session_(SpdySessionDependencies::SpdyCreateSession(&session_deps_)),
          log_(log), add_data_allowed_(true) {}

    void RunPreTestSetup() {
      // Disallow future calls to AddData
      add_data_allowed_ = false;

      // Set up http data.
      MockRead data_reads[] = {
        MockRead("HTTP/1.1 200 OK\r\n"),
        MockRead("Alternate-Protocol: 443:npn-spdy/1\r\n\r\n"),
        MockRead("hello world"),
        MockRead(true, OK),
      };
      first_transaction_.reset(
          new StaticSocketDataProvider(data_reads, arraysize(data_reads),
                                       NULL, 0));
      session_deps_.socket_factory.AddSocketDataProvider(
          first_transaction_.get());

      // Set up actual test data. Also add one SSLSocketDataProvider per
      // DataProvider.
      for(DataVector::iterator it = data_vector_.begin();
          it != data_vector_.end(); ++it) {
        linked_ptr<SSLSocketDataProvider> ssl_(
            new SSLSocketDataProvider(true, OK));
        ssl_->next_proto_status = SSLClientSocket::kNextProtoNegotiated;
        ssl_->next_proto = "spdy/1";
        ssl_->was_npn_negotiated = true;
        ssl_vector_.push_back(ssl_);
        session_deps_.socket_factory.AddSSLSocketDataProvider(ssl_.get());
        session_deps_.socket_factory.AddSocketDataProvider(*it);
      }

      // We first send an http request. The Alternate-Protocol header switches
      // the HttpNetworkTransaction into SSL/SPDY mode.
      trans_http_.reset(new HttpNetworkTransaction(session_));
      int rv = trans_http_->Start(&request_, &callback, log_);
      EXPECT_EQ(ERR_IO_PENDING, rv);
      EXPECT_EQ(OK, callback.WaitForResult());
      const HttpResponseInfo* response = trans_http_->GetResponseInfo();
      EXPECT_TRUE(response != NULL);
      EXPECT_TRUE(response->headers != NULL);
      EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
      std::string response_data;
      EXPECT_EQ(OK, ReadTransaction(trans_http_.get(), &response_data));
      EXPECT_EQ("hello world", response_data);

      // We're now ready to use SSL-npn SPDY.
      trans_.reset(new HttpNetworkTransaction(session_));
    }

    // Start the transaction, read some data, finish.
    void RunDefaultTest() {
      output_.rv = trans_->Start(&request_, &callback, log_);

      // We expect an IO Pending or some sort of error.
      EXPECT_LT(output_.rv, 0);
      if (output_.rv != ERR_IO_PENDING)
        return;

      output_.rv = callback.WaitForResult();
      if (output_.rv != OK) {
        session_->spdy_session_pool()->ClearSessions();
        return;
      }

      // Verify responses.
      const HttpResponseInfo* response = trans_->GetResponseInfo();
      ASSERT_TRUE(response != NULL);
      ASSERT_TRUE(response->headers != NULL);
      EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
      EXPECT_TRUE(response->was_fetched_via_spdy);
      EXPECT_TRUE(response->was_npn_negotiated);
      EXPECT_TRUE(response->was_alternate_protocol_available);
      output_.status_line = response->headers->GetStatusLine();
      output_.response_info = *response;  // Make a copy so we can verify.
      output_.rv = ReadTransaction(trans_.get(), &output_.response_data);
      EXPECT_EQ(OK, output_.rv);
      return;
    }

    // Most tests will want to call this function. In particular, the MockReads
    // should end with an empty read, and that read needs to be processed to
    // ensure proper deletion of the spdy_session_pool.
    void VerifyDataConsumed() {
      for(DataVector::iterator it = data_vector_.begin();
          it != data_vector_.end(); ++it) {
        EXPECT_TRUE((*it)->at_read_eof()) << "Read count: "
                                          << (*it)->read_count()
                                          << " Read index: "
                                          << (*it)->read_index();
        EXPECT_TRUE((*it)->at_write_eof()) << "Write count: "
                                           << (*it)->write_count()
                                           << " Write index: "
                                           << (*it)->write_index();
      }
    }

    // Occasionally a test will expect to error out before certain reads are
    // processed. In that case we want to explicitly ensure that the reads were
    // not processed.
    void VerifyDataNotConsumed() {
      for(DataVector::iterator it = data_vector_.begin();
          it != data_vector_.end(); ++it) {
        EXPECT_TRUE(!(*it)->at_read_eof()) << "Read count: "
                                           << (*it)->read_count()
                                           << " Read index: "
                                           << (*it)->read_index();
        EXPECT_TRUE(!(*it)->at_write_eof()) << "Write count: "
                                            << (*it)->write_count()
                                            << " Write index: "
                                            << (*it)->write_index();

      }
    }

    void RunToCompletion(StaticSocketDataProvider* data) {
      AddData(data);
      RunPreTestSetup();
      RunDefaultTest();
      VerifyDataConsumed();
    }

    // Only call AddData before calling RunPreTestSetup!! When RunPreTestSetup
    // is run, it will add this Data Provider, and a corresponding SSL data
    // provider.
    void AddData(StaticSocketDataProvider* data) {
      EXPECT_TRUE(add_data_allowed_);
      data_vector_.push_back(data);
    }

    // This can only be called after RunPreTestSetup. It adds a Data Provider,
    // but not a corresponding SSL data provider
    void AddDataNoSSL(StaticSocketDataProvider* data) {
      session_deps_.socket_factory.AddSocketDataProvider(data);
    }

    void SetSession(scoped_refptr<HttpNetworkSession>& session) {
      session_ = session;
    }
    HttpNetworkTransaction* trans() { return trans_.get(); }
    void ResetTrans() { trans_.reset(); }
    TransactionHelperResult& output() { return output_; }
    HttpRequestInfo& request() { return request_; }
    scoped_refptr<HttpNetworkSession>& session() { return session_; }

   private:
    typedef std::vector<StaticSocketDataProvider*> DataVector;
    typedef std::vector<linked_ptr<SSLSocketDataProvider> > SSLVector;
    HttpRequestInfo request_;
    SpdySessionDependencies session_deps_;
    scoped_refptr<HttpNetworkSession> session_;
    TransactionHelperResult output_;
    scoped_ptr<StaticSocketDataProvider> first_transaction_;
    SSLVector ssl_vector_;
    TestCompletionCallback callback;
    scoped_ptr<HttpNetworkTransaction> trans_;
    scoped_ptr<HttpNetworkTransaction> trans_http_;
    DataVector data_vector_;
    const BoundNetLog& log_;
    bool add_data_allowed_;
  };

  void ConnectStatusHelperWithExpectedStatus(const MockRead& status,
                                             int expected_status);

  void ConnectStatusHelper(const MockRead& status);

  const HttpRequestInfo& CreateGetRequest() {
    if (!google_get_request_initialized_) {
      google_get_request_.method = "GET";
      google_get_request_.url = GURL("http://www.google.com/");
      google_get_request_.load_flags = 0;
      google_get_request_initialized_ = true;
    }
    return google_get_request_;
  }

 private:
  bool google_get_request_initialized_;
  HttpRequestInfo google_get_request_;
};

//-----------------------------------------------------------------------------

// Verify HttpNetworkTransaction constructor.
TEST_F(SpdyNetworkTransactionTest, Constructor) {
  SpdySessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session =
      SpdySessionDependencies::SpdyCreateSession(&session_deps);
  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(session));
}

TEST_F(SpdyNetworkTransactionTest, Get) {
  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Start three gets simultaniously; making sure that multiplexed
// streams work properly.

// This can't use the TransactionHelper method, since it only
// handles a single transaction, and finishes them as soon
// as it launches them.

// TODO(gavinp): create a working generalized TransactionHelper that
// can allow multiple streams in flight.

TEST_F(SpdyNetworkTransactionTest, ThreeGets) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> fbody(ConstructSpdyBodyFrame(1, true));

  scoped_ptr<spdy::SpdyFrame> req2(ConstructSpdyGet(NULL, 0, false, 3, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<spdy::SpdyFrame> body2(ConstructSpdyBodyFrame(3, false));
  scoped_ptr<spdy::SpdyFrame> fbody2(ConstructSpdyBodyFrame(3, true));

  scoped_ptr<spdy::SpdyFrame> req3(ConstructSpdyGet(NULL, 0, false, 5, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp3(ConstructSpdyGetSynReply(NULL, 0, 5));
  scoped_ptr<spdy::SpdyFrame> body3(ConstructSpdyBodyFrame(5, false));
  scoped_ptr<spdy::SpdyFrame> fbody3(ConstructSpdyBodyFrame(5, true));

  MockWrite writes[] = { CreateMockWrite(*req),
                         CreateMockWrite(*req2),
                         CreateMockWrite(*req3),
  };
  MockRead reads[] = {
    CreateMockRead(*resp, 1),
    CreateMockRead(*body),
    CreateMockRead(*resp2, 4),
    CreateMockRead(*body2),
    CreateMockRead(*resp3, 7),
    CreateMockRead(*body3),

    CreateMockRead(*fbody),
    CreateMockRead(*fbody2),
    CreateMockRead(*fbody3),

    MockRead(true, 0, 0),  // EOF
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));

  BoundNetLog log;
  TransactionHelperResult out;
  {
    SpdySessionDependencies session_deps;
    HttpNetworkSession* session =
        SpdySessionDependencies::SpdyCreateSession(&session_deps);
    SpdySession::SetSSLMode(false);
    scoped_ptr<SpdyNetworkTransaction> trans1(
        new SpdyNetworkTransaction(session));
    scoped_ptr<SpdyNetworkTransaction> trans2(
        new SpdyNetworkTransaction(session));
    scoped_ptr<SpdyNetworkTransaction> trans3(
        new SpdyNetworkTransaction(session));

    session_deps.socket_factory.AddSocketDataProvider(data);

    TestCompletionCallback callback1;
    TestCompletionCallback callback2;
    TestCompletionCallback callback3;

    HttpRequestInfo httpreq1 = CreateGetRequest();
    HttpRequestInfo httpreq2 = CreateGetRequest();
    HttpRequestInfo httpreq3 = CreateGetRequest();

    out.rv = trans1->Start(&httpreq1, &callback1, log);
    ASSERT_EQ(ERR_IO_PENDING, out.rv);
    out.rv = trans2->Start(&httpreq2, &callback2, log);
    ASSERT_EQ(ERR_IO_PENDING, out.rv);
    out.rv = trans3->Start(&httpreq3, &callback3, log);
    ASSERT_EQ(ERR_IO_PENDING, out.rv);

    out.rv = callback1.WaitForResult();
    ASSERT_EQ(OK, out.rv);
    out.rv = callback3.WaitForResult();
    ASSERT_EQ(OK, out.rv);

    const HttpResponseInfo* response1 = trans1->GetResponseInfo();
    EXPECT_TRUE(response1->headers != NULL);
    EXPECT_TRUE(response1->was_fetched_via_spdy);
    out.status_line = response1->headers->GetStatusLine();
    out.response_info = *response1;

    trans2->GetResponseInfo();

    out.rv = ReadTransaction(trans1.get(), &out.response_data);
  }
  EXPECT_EQ(OK, out.rv);

  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);
}


// Similar to ThreeGets above, however this test adds a SETTINGS
// frame.  The SETTINGS frame is read during the IO loop waiting on
// the first transaction completion, and sets a maximum concurrent
// stream limit of 1.  This means that our IO loop exists after the
// second transaction completes, so we can assert on read_index().
TEST_F(SpdyNetworkTransactionTest, ThreeGetsWithMaxConcurrent) {
  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> fbody(ConstructSpdyBodyFrame(1, true));

  scoped_ptr<spdy::SpdyFrame> req2(ConstructSpdyGet(NULL, 0, false, 3, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<spdy::SpdyFrame> body2(ConstructSpdyBodyFrame(3, false));
  scoped_ptr<spdy::SpdyFrame> fbody2(ConstructSpdyBodyFrame(3, true));

  scoped_ptr<spdy::SpdyFrame> req3(ConstructSpdyGet(NULL, 0, false, 5, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp3(ConstructSpdyGetSynReply(NULL, 0, 5));
  scoped_ptr<spdy::SpdyFrame> body3(ConstructSpdyBodyFrame(5, false));
  scoped_ptr<spdy::SpdyFrame> fbody3(ConstructSpdyBodyFrame(5, true));

  spdy::SpdySettings settings;
  spdy::SettingsFlagsAndId id(0);
  id.set_id(spdy::SETTINGS_MAX_CONCURRENT_STREAMS);
  const size_t max_concurrent_streams = 1;

  settings.push_back(spdy::SpdySetting(id, max_concurrent_streams));
  scoped_ptr<spdy::SpdyFrame> settings_frame(ConstructSpdySettings(settings));

  MockWrite writes[] = { CreateMockWrite(*req),
                         CreateMockWrite(*req2),
                         CreateMockWrite(*req3),
  };
  MockRead reads[] = {
    CreateMockRead(*settings_frame, 0),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    CreateMockRead(*fbody),
    CreateMockRead(*resp2, 6),
    CreateMockRead(*body2),
    CreateMockRead(*fbody2),
    CreateMockRead(*resp3, 11),
    CreateMockRead(*body3),
    CreateMockRead(*fbody3),

    MockRead(true, 0, 0),  // EOF
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));

  BoundNetLog log;
  TransactionHelperResult out;
  {
    SpdySessionDependencies session_deps;
    HttpNetworkSession* session =
        SpdySessionDependencies::SpdyCreateSession(&session_deps);
    SpdySession::SetSSLMode(false);
    scoped_ptr<SpdyNetworkTransaction> trans1(
        new SpdyNetworkTransaction(session));
    scoped_ptr<SpdyNetworkTransaction> trans2(
        new SpdyNetworkTransaction(session));
    scoped_ptr<SpdyNetworkTransaction> trans3(
        new SpdyNetworkTransaction(session));

    session_deps.socket_factory.AddSocketDataProvider(data);

    TestCompletionCallback callback1;
    TestCompletionCallback callback2;
    TestCompletionCallback callback3;

    HttpRequestInfo httpreq1 = CreateGetRequest();
    HttpRequestInfo httpreq2 = CreateGetRequest();
    HttpRequestInfo httpreq3 = CreateGetRequest();

    out.rv = trans1->Start(&httpreq1, &callback1, log);
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    // run transaction 1 through quickly to force a read of our SETTINGS
    // frame
    out.rv = callback1.WaitForResult();

    out.rv = trans2->Start(&httpreq2, &callback2, log);
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    out.rv = trans3->Start(&httpreq3, &callback3, log);
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    out.rv = callback2.WaitForResult();
    ASSERT_EQ(OK, out.rv);
    EXPECT_EQ(7U, data->read_index());  // i.e. the third trans was queued

    out.rv = callback3.WaitForResult();
    ASSERT_EQ(OK, out.rv);

    const HttpResponseInfo* response1 = trans1->GetResponseInfo();
    EXPECT_TRUE(response1->headers != NULL);
    EXPECT_TRUE(response1->was_fetched_via_spdy);
    out.status_line = response1->headers->GetStatusLine();
    out.response_info = *response1;
    out.rv = ReadTransaction(trans1.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    const HttpResponseInfo* response2 = trans2->GetResponseInfo();
    out.status_line = response2->headers->GetStatusLine();
    out.response_info = *response2;
    out.rv = ReadTransaction(trans2.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    const HttpResponseInfo* response3 = trans3->GetResponseInfo();
    out.status_line = response3->headers->GetStatusLine();
    out.response_info = *response3;
    out.rv = ReadTransaction(trans3.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);
  }
  EXPECT_EQ(OK, out.rv);

  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());
}

// Similar to ThreeGetsWithMaxConcurrent above, however this test adds
// a fourth transaction.  The third and fourth transactions have
// different data ("hello!" vs "hello!hello!") and because of the
// user specified priority, we expect to see them inverted in
// the response from the server.
TEST_F(SpdyNetworkTransactionTest, FourGetsWithMaxConcurrentPriority) {
  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> fbody(ConstructSpdyBodyFrame(1, true));

  scoped_ptr<spdy::SpdyFrame> req2(ConstructSpdyGet(NULL, 0, false, 3, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<spdy::SpdyFrame> body2(ConstructSpdyBodyFrame(3, false));
  scoped_ptr<spdy::SpdyFrame> fbody2(ConstructSpdyBodyFrame(3, true));

  scoped_ptr<spdy::SpdyFrame> req4(
      ConstructSpdyGet(NULL, 0, false, 5, HIGHEST));
  scoped_ptr<spdy::SpdyFrame> resp4(ConstructSpdyGetSynReply(NULL, 0, 5));
  scoped_ptr<spdy::SpdyFrame> fbody4(ConstructSpdyBodyFrame(5, true));

  scoped_ptr<spdy::SpdyFrame> req3(ConstructSpdyGet(NULL, 0, false, 7, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp3(ConstructSpdyGetSynReply(NULL, 0, 7));
  scoped_ptr<spdy::SpdyFrame> body3(ConstructSpdyBodyFrame(7, false));
  scoped_ptr<spdy::SpdyFrame> fbody3(ConstructSpdyBodyFrame(7, true));


  spdy::SpdySettings settings;
  spdy::SettingsFlagsAndId id(0);
  id.set_id(spdy::SETTINGS_MAX_CONCURRENT_STREAMS);
  const size_t max_concurrent_streams = 1;

  settings.push_back(spdy::SpdySetting(id, max_concurrent_streams));
  scoped_ptr<spdy::SpdyFrame> settings_frame(ConstructSpdySettings(settings));

  MockWrite writes[] = { CreateMockWrite(*req),
                         CreateMockWrite(*req2),
                         CreateMockWrite(*req4),
                         CreateMockWrite(*req3),
  };
  MockRead reads[] = {
    CreateMockRead(*settings_frame, 0),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    CreateMockRead(*fbody),
    CreateMockRead(*resp2, 6),
    CreateMockRead(*body2),
    CreateMockRead(*fbody2),
    CreateMockRead(*resp4, 12),
    CreateMockRead(*fbody4),
    CreateMockRead(*resp3, 15),
    CreateMockRead(*body3),
    CreateMockRead(*fbody3),

    MockRead(true, 0, 0),  // EOF
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));

  BoundNetLog log;
  TransactionHelperResult out;
  {
    SpdySessionDependencies session_deps;
    HttpNetworkSession* session =
        SpdySessionDependencies::SpdyCreateSession(&session_deps);
    SpdySession::SetSSLMode(false);
    scoped_ptr<SpdyNetworkTransaction> trans1(
        new SpdyNetworkTransaction(session));
    scoped_ptr<SpdyNetworkTransaction> trans2(
        new SpdyNetworkTransaction(session));
    scoped_ptr<SpdyNetworkTransaction> trans3(
        new SpdyNetworkTransaction(session));
    scoped_ptr<SpdyNetworkTransaction> trans4(
        new SpdyNetworkTransaction(session));

    session_deps.socket_factory.AddSocketDataProvider(data);

    TestCompletionCallback callback1;
    TestCompletionCallback callback2;
    TestCompletionCallback callback3;
    TestCompletionCallback callback4;

    HttpRequestInfo httpreq1 = CreateGetRequest();
    HttpRequestInfo httpreq2 = CreateGetRequest();
    HttpRequestInfo httpreq3 = CreateGetRequest();
    HttpRequestInfo httpreq4 = CreateGetRequest();
    httpreq4.priority = HIGHEST;

    out.rv = trans1->Start(&httpreq1, &callback1, log);
    ASSERT_EQ(ERR_IO_PENDING, out.rv);
    // run transaction 1 through quickly to force a read of our SETTINGS
    // frame
    out.rv = callback1.WaitForResult();
    ASSERT_EQ(OK, out.rv);

    out.rv = trans2->Start(&httpreq2, &callback2, log);
    ASSERT_EQ(ERR_IO_PENDING, out.rv);
    out.rv = trans3->Start(&httpreq3, &callback3, log);
    ASSERT_EQ(ERR_IO_PENDING, out.rv);
    out.rv = trans4->Start(&httpreq4, &callback4, log);
    ASSERT_EQ(ERR_IO_PENDING, out.rv);

    out.rv = callback2.WaitForResult();
    ASSERT_EQ(OK, out.rv);
    EXPECT_EQ(data->read_index(), 7U);  // i.e. the third & fourth trans queued

    out.rv = callback3.WaitForResult();
    ASSERT_EQ(OK, out.rv);

    const HttpResponseInfo* response1 = trans1->GetResponseInfo();
    EXPECT_TRUE(response1->headers != NULL);
    EXPECT_TRUE(response1->was_fetched_via_spdy);
    out.status_line = response1->headers->GetStatusLine();
    out.response_info = *response1;
    out.rv = ReadTransaction(trans1.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    const HttpResponseInfo* response2 = trans2->GetResponseInfo();
    out.status_line = response2->headers->GetStatusLine();
    out.response_info = *response2;
    out.rv = ReadTransaction(trans2.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    // notice: response3 gets two hellos, response4 gets one
    // hello, so we know dequeuing priority was respected.
    const HttpResponseInfo* response3 = trans3->GetResponseInfo();
    out.status_line = response3->headers->GetStatusLine();
    out.response_info = *response3;
    out.rv = ReadTransaction(trans3.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    out.rv = callback4.WaitForResult();
    EXPECT_EQ(OK, out.rv);
    const HttpResponseInfo* response4 = trans4->GetResponseInfo();
    out.status_line = response4->headers->GetStatusLine();
    out.response_info = *response4;
    out.rv = ReadTransaction(trans4.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!", out.response_data);
  }
  EXPECT_EQ(OK, out.rv);

  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());
}

// Similar to ThreeGetsMaxConcurrrent above, however, this test
// deletes a session in the middle of the transaction to insure
// that we properly remove pendingcreatestream objects from
// the spdy_session
TEST_F(SpdyNetworkTransactionTest, ThreeGetsWithMaxConcurrentDelete) {
  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> fbody(ConstructSpdyBodyFrame(1, true));

  scoped_ptr<spdy::SpdyFrame> req2(ConstructSpdyGet(NULL, 0, false, 3, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<spdy::SpdyFrame> body2(ConstructSpdyBodyFrame(3, false));
  scoped_ptr<spdy::SpdyFrame> fbody2(ConstructSpdyBodyFrame(3, true));

  scoped_ptr<spdy::SpdyFrame> req3(ConstructSpdyGet(NULL, 0, false, 5, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp3(ConstructSpdyGetSynReply(NULL, 0, 5));
  scoped_ptr<spdy::SpdyFrame> body3(ConstructSpdyBodyFrame(5, false));
  scoped_ptr<spdy::SpdyFrame> fbody3(ConstructSpdyBodyFrame(5, true));

  spdy::SpdySettings settings;
  spdy::SettingsFlagsAndId id(0);
  id.set_id(spdy::SETTINGS_MAX_CONCURRENT_STREAMS);
  const size_t max_concurrent_streams = 1;

  settings.push_back(spdy::SpdySetting(id, max_concurrent_streams));
  scoped_ptr<spdy::SpdyFrame> settings_frame(ConstructSpdySettings(settings));

  MockWrite writes[] = { CreateMockWrite(*req),
                         CreateMockWrite(*req2),
  };
  MockRead reads[] = {
    CreateMockRead(*settings_frame, 0),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    CreateMockRead(*fbody),
    CreateMockRead(*resp2, 6),
    CreateMockRead(*body2),
    CreateMockRead(*fbody2),
    MockRead(true, 0, 0),  // EOF
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));

  BoundNetLog log;
  TransactionHelperResult out;
  {
    SpdySessionDependencies session_deps;
    HttpNetworkSession* session =
        SpdySessionDependencies::SpdyCreateSession(&session_deps);
    SpdySession::SetSSLMode(false);
    scoped_ptr<SpdyNetworkTransaction> trans1(
        new SpdyNetworkTransaction(session));
    scoped_ptr<SpdyNetworkTransaction> trans2(
        new SpdyNetworkTransaction(session));
    scoped_ptr<SpdyNetworkTransaction> trans3(
        new SpdyNetworkTransaction(session));

    session_deps.socket_factory.AddSocketDataProvider(data);

    TestCompletionCallback callback1;
    TestCompletionCallback callback2;
    TestCompletionCallback callback3;

    HttpRequestInfo httpreq1 = CreateGetRequest();
    HttpRequestInfo httpreq2 = CreateGetRequest();
    HttpRequestInfo httpreq3 = CreateGetRequest();

    out.rv = trans1->Start(&httpreq1, &callback1, log);
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    // run transaction 1 through quickly to force a read of our SETTINGS
    // frame
    out.rv = callback1.WaitForResult();

    out.rv = trans2->Start(&httpreq2, &callback2, log);
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    out.rv = trans3->Start(&httpreq3, &callback3, log);
    delete trans3.release();
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    out.rv = callback2.WaitForResult();

    EXPECT_EQ(8U, data->read_index());

    const HttpResponseInfo* response1 = trans1->GetResponseInfo();
    EXPECT_TRUE(response1->headers != NULL);
    EXPECT_TRUE(response1->was_fetched_via_spdy);
    out.status_line = response1->headers->GetStatusLine();
    out.response_info = *response1;
    out.rv = ReadTransaction(trans1.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    const HttpResponseInfo* response2 = trans2->GetResponseInfo();
    out.status_line = response2->headers->GetStatusLine();
    out.response_info = *response2;
    out.rv = ReadTransaction(trans2.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);
  }
  EXPECT_EQ(OK, out.rv);

  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());
}

// Test that a simple POST works.
TEST_F(SpdyNetworkTransactionTest, Post) {
  static const char upload[] = { "hello!" };

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data = new UploadData();
  request.upload_data->AppendBytes(upload, strlen(upload));

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPost(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*body),  // POST upload frame
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(request,
                                     BoundNetLog());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a simple POST works.
TEST_F(SpdyNetworkTransactionTest, EmptyPost) {
  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  // Create an empty UploadData.
  request.upload_data = new UploadData();

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPost(NULL, 0));
  // Set the FIN bit since there will be no body.
  req->set_flags(spdy::CONTROL_FLAG_FIN);
  MockWrite writes[] = {
    CreateMockWrite(*req),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  NormalSpdyTransactionHelper helper(request,
                                     BoundNetLog());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// While we're doing a post, the server sends back a SYN_REPLY.
TEST_F(SpdyNetworkTransactionTest, PostWithEarlySynReply) {
  static const char upload[] = { "hello!" };

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data = new UploadData();
  request.upload_data->AppendBytes(upload, sizeof(upload));

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPost(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
    MockWrite writes[] = {
    CreateMockWrite(*req.get(), 2),
    CreateMockWrite(*body.get(), 3),  // POST upload frame
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp.get(), 2),
    CreateMockRead(*body.get(), 3),
    MockRead(false, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(0, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(request,
                                     BoundNetLog());
  helper.AddData(data.get());
  helper.RunPreTestSetup();
  helper.RunDefaultTest();
  helper.VerifyDataNotConsumed();
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
}

// Test that the transaction doesn't crash when we don't have a reply.
TEST_F(SpdyNetworkTransactionTest, ResponseWithoutSynReply) {
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads), NULL, 0));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SYN_REPLY_NOT_RECEIVED, out.rv);
}

// Test that the transaction doesn't crash when we get two replies on the same
// stream ID. See http://crbug.com/45639.
TEST_F(SpdyNetworkTransactionTest, ResponseWithTwoSynReplies) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.AddData(data.get());
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  std::string response_data;
  rv = ReadTransaction(trans, &response_data);
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, rv);

  helper.VerifyDataConsumed();
}

// Test that WINDOW_UPDATE frames change window_size correctly.
TEST_F(SpdyNetworkTransactionTest, WindowUpdate) {
  SpdySessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session =
      SpdySessionDependencies::SpdyCreateSession(&session_deps);

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  // Setup the request
  static const char upload[] = { "hello!" };
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data = new UploadData();
  request.upload_data->AppendBytes(upload, strlen(upload));

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPost(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*body),
  };

  // Response frames, send WINDOW_UPDATE first
  static const int kDeltaWindowSize = 0xff;
  scoped_ptr<spdy::SpdyFrame> window_update(
      ConstructSpdyWindowUpdate(1, kDeltaWindowSize));
  scoped_ptr<spdy::SpdyFrame> reply(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*window_update),
    CreateMockRead(*reply),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  session_deps.socket_factory.AddSocketDataProvider(data.get());

  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(session));

  TestCompletionCallback callback;
  int rv = trans->Start(&request, &callback, BoundNetLog());

  ASSERT_TRUE(trans->stream_ != NULL);
  ASSERT_TRUE(trans->stream_->stream() != NULL);
  EXPECT_EQ(spdy::kInitialWindowSize, trans->stream_->stream()->window_size());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  ASSERT_TRUE(trans->stream_ != NULL);
  ASSERT_TRUE(trans->stream_->stream() != NULL);
  EXPECT_EQ(spdy::kInitialWindowSize + kDeltaWindowSize,
            trans->stream_->stream()->window_size());
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());
}

// Test that WINDOW_UPDATE frame causing overflow is handled correctly.
TEST_F(SpdyNetworkTransactionTest, WindowUpdateOverflow) {
  SpdySessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session =
      SpdySessionDependencies::SpdyCreateSession(&session_deps);

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  // Setup the request
  static const char upload[] = { "hello!" };
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data = new UploadData();
  request.upload_data->AppendBytes(upload, strlen(upload));

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPost(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  scoped_ptr<spdy::SpdyFrame> rst(
      ConstructSpdyRstStream(1, spdy::FLOW_CONTROL_ERROR));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*body),
    CreateMockWrite(*rst),
  };

  // Response frames, send WINDOW_UPDATE first
  static const int kDeltaWindowSize = 0x7fffffff; // cause an overflow
  scoped_ptr<spdy::SpdyFrame> window_update(
      ConstructSpdyWindowUpdate(1, kDeltaWindowSize));
  scoped_ptr<spdy::SpdyFrame> reply(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*window_update),
    CreateMockRead(*reply),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  session_deps.socket_factory.AddSocketDataProvider(data.get());

  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(session));

  TestCompletionCallback callback;
  int rv = trans->Start(&request, &callback, BoundNetLog());

  ASSERT_TRUE(trans->stream_ != NULL);
  ASSERT_TRUE(trans->stream_->stream() != NULL);
  EXPECT_EQ(spdy::kInitialWindowSize, trans->stream_->stream()->window_size());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, rv);

  ASSERT_TRUE(session != NULL);
  ASSERT_TRUE(session->spdy_session_pool() != NULL);
  session->spdy_session_pool()->ClearSessions();

  EXPECT_FALSE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());
}

TEST_F(SpdyNetworkTransactionTest, CancelledTransaction) {
  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    // This following read isn't used by the test, except during the
    // RunAllPending() call at the end since the SpdySession survives the
    // HttpNetworkTransaction and still tries to continue Read()'ing.  Any
    // MockRead will do here.
    MockRead(true, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, arraysize(reads),
                                writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.AddData(&data);
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  helper.ResetTrans();  // Cancel the transaction.

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();
  helper.VerifyDataNotConsumed();
}

class SpdyNetworkTransactionTest::StartTransactionCallback
    : public CallbackRunner< Tuple1<int> > {
 public:
  explicit StartTransactionCallback(
      scoped_refptr<HttpNetworkSession>& session,
      NormalSpdyTransactionHelper& helper)
      : session_(session), helper_(helper) {}

  // We try to start another transaction, which should succeed.
  virtual void RunWithParams(const Tuple1<int>& params) {
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(session_));
    TestCompletionCallback callback;
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;
    int rv = trans->Start(&request, &callback, BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    rv = callback.WaitForResult();
  }

 private:
  scoped_refptr<HttpNetworkSession>& session_;
  NormalSpdyTransactionHelper& helper_;
};

// Verify that the client can correctly deal with the user callback attempting
// to start another transaction on a session that is closing down. See
// http://crbug.com/47455
TEST_F(SpdyNetworkTransactionTest, StartTransactionOnReadCallback) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };
  MockWrite writes2[] = { CreateMockWrite(*req) };

  // The indicated length of this packet is longer than its actual length. When
  // the session receives an empty packet after this one, it shuts down the
  // session, and calls the read callback with the incomplete data.
  const uint8 kGetBodyFrame2[] = {
    0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x07,
    'h', 'e', 'l', 'l', 'o', '!',
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(true, ERR_IO_PENDING, 3),  // Force a pause
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame2),
             arraysize(kGetBodyFrame2), 4),
    MockRead(true, ERR_IO_PENDING, 5),  // Force a pause
    MockRead(true, 0, 0, 6),  // EOF
  };
  MockRead reads2[] = {
    CreateMockRead(*resp, 2),
    MockRead(true, 0, 0, 3),  // EOF
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  scoped_refptr<DelayedSocketData> data2(
      new DelayedSocketData(0, reads2, arraysize(reads2),
                            writes2, arraysize(writes2)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.AddData(data.get());
  helper.AddData(data2.get());
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();

  StartTransactionCallback callback2(helper.session(), helper);
  const int kSize = 3000;
  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kSize);
  rv = trans->Read(buf, kSize, &callback2);
  // This forces an err_IO_pending, which sets the callback.
  data->CompleteRead();
  // This finishes the read.
  data->CompleteRead();
  helper.VerifyDataConsumed();
}

class SpdyNetworkTransactionTest::DeleteSessionCallback
    : public CallbackRunner< Tuple1<int> > {
 public:
  explicit DeleteSessionCallback(NormalSpdyTransactionHelper& helper) :
      helper_(helper) {}

  // We kill the transaction, which deletes the session and stream.
  virtual void RunWithParams(const Tuple1<int>& params) {
    helper_.ResetTrans();
  }

 private:
  NormalSpdyTransactionHelper& helper_;
};

// Verify that the client can correctly deal with the user callback deleting the
// transaction. Failures will usually be valgrind errors. See
// http://crbug.com/46925
TEST_F(SpdyNetworkTransactionTest, DeleteSessionOnReadCallback) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp.get(), 2),
    MockRead(true, ERR_IO_PENDING, 3),  // Force a pause
    CreateMockRead(*body.get(), 4),
    MockRead(true, 0, 0, 5),  // EOF
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.AddData(data.get());
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();

  // Setup a user callback which will delete the session, and clear out the
  // memory holding the stream object. Note that the callback deletes trans.
  DeleteSessionCallback callback2(helper);
  const int kSize = 3000;
  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kSize);
  rv = trans->Read(buf, kSize, &callback2);
  ASSERT_EQ(ERR_IO_PENDING, rv);
  data->CompleteRead();

  // Finish running rest of tasks.
  MessageLoop::current()->RunAllPending();
  helper.VerifyDataConsumed();
}

// Verify that various SynReply headers parse correctly through the
// HTTP layer.
TEST_F(SpdyNetworkTransactionTest, SynReplyHeaders) {
  struct SynReplyHeadersTests {
    int num_headers;
    const char* extra_headers[5];
    const char* expected_headers;
  } test_cases[] = {
    // This uses a multi-valued cookie header.
    { 2,
      { "cookie", "val1",
        "cookie", "val2",  // will get appended separated by NULL
        NULL
      },
      "cookie: val1\n"
      "cookie: val2\n"
      "hello: bye\n"
      "status: 200\n"
      "url: /index.php\n"
      "version: HTTP/1.1\n"
    },
    // This is the minimalist set of headers.
    { 0,
      { NULL },
      "hello: bye\n"
      "status: 200\n"
      "url: /index.php\n"
      "version: HTTP/1.1\n"
    },
    // Headers with a comma separated list.
    { 1,
      { "cookie", "val1,val2",
        NULL
      },
      "cookie: val1,val2\n"
      "hello: bye\n"
      "status: 200\n"
      "url: /index.php\n"
      "version: HTTP/1.1\n"
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    scoped_ptr<spdy::SpdyFrame> req(
        ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
    MockWrite writes[] = { CreateMockWrite(*req) };

    scoped_ptr<spdy::SpdyFrame> resp(
        ConstructSpdyGetSynReply(test_cases[i].extra_headers,
                                 test_cases[i].num_headers,
                                 1));
    scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
    MockRead reads[] = {
      CreateMockRead(*resp),
      CreateMockRead(*body),
      MockRead(true, 0, 0)  // EOF
    };

    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                       BoundNetLog());
    helper.RunToCompletion(data.get());
    TransactionHelperResult out = helper.output();

    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!", out.response_data);

    scoped_refptr<HttpResponseHeaders> headers = out.response_info.headers;
    EXPECT_TRUE(headers.get() != NULL);
    void* iter = NULL;
    std::string name, value, lines;
    while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
      lines.append(name);
      lines.append(": ");
      lines.append(value);
      lines.append("\n");
    }
    EXPECT_EQ(std::string(test_cases[i].expected_headers), lines);
  }
}

// Verify that various SynReply headers parse vary fields correctly
// through the HTTP layer, and the response matches the request.
TEST_F(SpdyNetworkTransactionTest, SynReplyHeadersVary) {
  static const SpdyHeaderInfo syn_reply_info = {
    spdy::SYN_REPLY,                              // Syn Reply
    1,                                            // Stream ID
    0,                                            // Associated Stream ID
    SPDY_PRIORITY_LOWEST,                         // Priority
    spdy::CONTROL_FLAG_NONE,                      // Control Flags
    false,                                        // Compressed
    spdy::INVALID,                                // Status
    NULL,                                         // Data
    0,                                            // Data Length
    spdy::DATA_FLAG_NONE                          // Data Flags
  };
  // Modify the following data to change/add test cases:
  struct SynReplyTests {
    const SpdyHeaderInfo* syn_reply;
    bool vary_matches;
    int num_headers[2];
    const char* extra_headers[2][16];
  } test_cases[] = {
    // Test the case of a multi-valued cookie.  When the value is delimited
    // with NUL characters, it needs to be unfolded into multiple headers.
    {
      &syn_reply_info,
      true,
      { 1, 4 },
      { { "cookie",   "val1,val2",
          NULL
        },
        { "vary",     "cookie",
          "status",   "200",
          "url",      "/index.php",
          "version",  "HTTP/1.1",
          NULL
        }
      }
    }, {    // Multiple vary fields.
      &syn_reply_info,
      true,
      { 2, 5 },
      { { "friend",   "barney",
          "enemy",    "snaggletooth",
          NULL
        },
        { "vary",     "friend",
          "vary",     "enemy",
          "status",   "200",
          "url",      "/index.php",
          "version",  "HTTP/1.1",
          NULL
        }
      }
    }, {    // Test a '*' vary field.
      &syn_reply_info,
      false,
      { 1, 4 },
      { { "cookie",   "val1,val2",
          NULL
        },
        { "vary",     "*",
          "status",   "200",
          "url",      "/index.php",
          "version",  "HTTP/1.1",
          NULL
        }
      }
    }, {    // Multiple comma-separated vary fields.
      &syn_reply_info,
      true,
      { 2, 4 },
      { { "friend",   "barney",
          "enemy",    "snaggletooth",
          NULL
        },
        { "vary",     "friend,enemy",
          "status",   "200",
          "url",      "/index.php",
          "version",  "HTTP/1.1",
          NULL
        }
      }
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    // Construct the request.
    scoped_ptr<spdy::SpdyFrame> frame_req(
      ConstructSpdyGet(test_cases[i].extra_headers[0],
                       test_cases[i].num_headers[0],
                       false, 1, LOWEST));

    MockWrite writes[] = {
      CreateMockWrite(*frame_req),
    };

    // Construct the reply.
    scoped_ptr<spdy::SpdyFrame> frame_reply(
      ConstructSpdyPacket(*test_cases[i].syn_reply,
                          test_cases[i].extra_headers[1],
                          test_cases[i].num_headers[1],
                          NULL,
                          0));

    scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
    MockRead reads[] = {
      CreateMockRead(*frame_reply),
      CreateMockRead(*body),
      MockRead(true, 0, 0)  // EOF
    };

    // Attach the headers to the request.
    int header_count = test_cases[i].num_headers[0];

    HttpRequestInfo request = CreateGetRequest();
    for (int ct = 0; ct < header_count; ct++) {
      const char* header_key = test_cases[i].extra_headers[0][ct * 2];
      const char* header_value = test_cases[i].extra_headers[0][ct * 2 + 1];
      request.extra_headers.SetHeader(header_key, header_value);
    }

    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    NormalSpdyTransactionHelper helper(request,
                                       BoundNetLog());
    helper.RunToCompletion(data.get());
    TransactionHelperResult out = helper.output();

    EXPECT_EQ(OK, out.rv) << i;
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line) << i;
    EXPECT_EQ("hello!", out.response_data) << i;

    // Test the response information.
    EXPECT_TRUE(out.response_info.response_time >
                out.response_info.request_time) << i;
    base::TimeDelta test_delay = out.response_info.response_time -
                                 out.response_info.request_time;
    base::TimeDelta min_expected_delay;
    min_expected_delay.FromMilliseconds(10);
    EXPECT_GT(test_delay.InMillisecondsF(),
              min_expected_delay.InMillisecondsF()) << i;
    EXPECT_EQ(out.response_info.vary_data.is_valid(),
              test_cases[i].vary_matches) << i;

    // Check the headers.
    scoped_refptr<HttpResponseHeaders> headers = out.response_info.headers;
    ASSERT_TRUE(headers.get() != NULL) << i;
    void* iter = NULL;
    std::string name, value, lines;
    while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
      lines.append(name);
      lines.append(": ");
      lines.append(value);
      lines.append("\n");
    }

    // Construct the expected header reply string.
    char reply_buffer[256] = "";
    ConstructSpdyReplyString(test_cases[i].extra_headers[1],
                             test_cases[i].num_headers[1],
                             reply_buffer,
                             256);

    EXPECT_EQ(std::string(reply_buffer), lines) << i;
  }
}

// Verify that we don't crash on invalid SynReply responses.
TEST_F(SpdyNetworkTransactionTest, InvalidSynReply) {
  const SpdyHeaderInfo kSynStartHeader = {
    spdy::SYN_REPLY,              // Kind = SynReply
    1,                            // Stream ID
    0,                            // Associated stream ID
    SPDY_PRIORITY_LOWEST,         // Priority
    spdy::CONTROL_FLAG_NONE,      // Control Flags
    false,                        // Compressed
    spdy::INVALID,                // Status
    NULL,                         // Data
    0,                            // Length
    spdy::DATA_FLAG_NONE          // Data Flags
  };

  struct InvalidSynReplyTests {
    int num_headers;
    const char* headers[10];
  } test_cases[] = {
    // SYN_REPLY missing status header
    { 4,
      { "cookie", "val1",
        "cookie", "val2",
        "url", "/index.php",
        "version", "HTTP/1.1",
        NULL
      },
    },
    // SYN_REPLY missing version header
    { 2,
      { "status", "200",
        "url", "/index.php",
        NULL
      },
    },
    // SYN_REPLY with no headers
    { 0, { NULL }, },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    scoped_ptr<spdy::SpdyFrame> req(
        ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
    MockWrite writes[] = {
      CreateMockWrite(*req),
    };

    scoped_ptr<spdy::SpdyFrame> resp(
        ConstructSpdyPacket(kSynStartHeader,
                            NULL, 0,
                            test_cases[i].headers,
                            test_cases[i].num_headers));
    scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
    MockRead reads[] = {
      CreateMockRead(*resp),
      CreateMockRead(*body),
      MockRead(true, 0, 0)  // EOF
    };

    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                       BoundNetLog());
    helper.RunToCompletion(data.get());
    TransactionHelperResult out = helper.output();
    EXPECT_EQ(ERR_INVALID_RESPONSE, out.rv);
  }
}

// Verify that we don't crash on some corrupt frames.
// TODO(eroman): Renable this test, see http://crbug.com/48588
TEST_F(SpdyNetworkTransactionTest, DISABLED_CorruptFrameSessionError) {
  // This is the length field with a big number
  scoped_ptr<spdy::SpdyFrame> syn_reply_massive_length(
      ConstructSpdyGetSynReply(NULL, 0, 1));
  syn_reply_massive_length->set_length(0x111126);

  struct SynReplyTests {
    const spdy::SpdyFrame* syn_reply;
  } test_cases[] = {
    { syn_reply_massive_length.get(), },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    scoped_ptr<spdy::SpdyFrame> req(
        ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
    MockWrite writes[] = {
      CreateMockWrite(*req),
      MockWrite(true, 0, 0)  // EOF
    };

    scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
    MockRead reads[] = {
      CreateMockRead(*test_cases[i].syn_reply),
      CreateMockRead(*body),
      MockRead(true, 0, 0)  // EOF
    };

    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                       BoundNetLog());
    helper.RunToCompletion(data.get());
    TransactionHelperResult out = helper.output();
    EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
  }
}

// Test that we shutdown correctly on write errors.
TEST_F(SpdyNetworkTransactionTest, WriteError) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = {
    // We'll write 10 bytes successfully
    MockWrite(true, req->data(), 10),
    // Followed by ERROR!
    MockWrite(true, ERR_FAILED),
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, NULL, 0,
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_FAILED, out.rv);
  data->Reset();
}

// Test that partial writes work.
TEST_F(SpdyNetworkTransactionTest, PartialWrite) {
  // Chop the SYN_STREAM frame into 5 chunks.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  const int kChunks = 5;
  scoped_array<MockWrite> writes(ChopWriteFrame(*req.get(), kChunks));

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(kChunks, reads, arraysize(reads),
                            writes.get(), kChunks));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// In this test, we enable compression, but get a uncompressed SynReply from
// the server.  Verify that teardown is all clean.
TEST_F(SpdyNetworkTransactionTest, DecompressFailureOnSynReply) {
  // For this test, we turn on the normal compression.
  EnableCompression(true);

  scoped_ptr<spdy::SpdyFrame> compressed(
      ConstructSpdyGet(NULL, 0, true, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*compressed),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SYN_REPLY_NOT_RECEIVED, out.rv);
  data->Reset();

  EnableCompression(false);
}

// Test that the NetLog contains good data for a simple GET request.
TEST_F(SpdyNetworkTransactionTest, NetLog) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  net::CapturingBoundNetLog log(net::CapturingNetLog::kUnbounded);

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     log.bound());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // Check that the NetLog was filled reasonably.
  // This test is intentionally non-specific about the exact ordering of the
  // log; instead we just check to make sure that certain events exist, and that
  // they are in the right order.
  EXPECT_LT(0u, log.entries().size());
  int pos = 0;
  pos = net::ExpectLogContainsSomewhere(log.entries(), 0,
      net::NetLog::TYPE_SPDY_TRANSACTION_SEND_REQUEST,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
      net::NetLog::TYPE_SPDY_TRANSACTION_SEND_REQUEST,
      net::NetLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
      net::NetLog::TYPE_SPDY_TRANSACTION_READ_HEADERS,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
      net::NetLog::TYPE_SPDY_TRANSACTION_READ_HEADERS,
      net::NetLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
      net::NetLog::TYPE_SPDY_TRANSACTION_READ_BODY,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
      net::NetLog::TYPE_SPDY_TRANSACTION_READ_BODY,
      net::NetLog::PHASE_END);
}

// Since we buffer the IO from the stream to the renderer, this test verifies
// that when we read out the maximum amount of data (e.g. we received 50 bytes
// on the network, but issued a Read for only 5 of those bytes) that the data
// flow still works correctly.
TEST_F(SpdyNetworkTransactionTest, BufferFull) {
  spdy::SpdyFramer framer;

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // 2 data frames in a single read.
  scoped_ptr<spdy::SpdyFrame> data_frame_1(
      framer.CreateDataFrame(1, "goodby", 6, spdy::DATA_FLAG_NONE));
  scoped_ptr<spdy::SpdyFrame> data_frame_2(
      framer.CreateDataFrame(1, "e worl", 6, spdy::DATA_FLAG_NONE));
  const spdy::SpdyFrame* data_frames[2] = {
    data_frame_1.get(),
    data_frame_2.get(),
  };
  char combined_data_frames[100];
  int combined_data_frames_len =
      CombineFrames(data_frames, arraysize(data_frames),
                    combined_data_frames, arraysize(combined_data_frames));
  scoped_ptr<spdy::SpdyFrame> last_frame(
      framer.CreateDataFrame(1, "d", 1, spdy::DATA_FLAG_FIN));

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(true, ERR_IO_PENDING),  // Force a pause
    MockRead(true, combined_data_frames, combined_data_frames_len),
    MockRead(true, ERR_IO_PENDING),  // Force a pause
    CreateMockRead(*last_frame),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));


  TestCompletionCallback callback;

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.AddData(data.get());
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();
  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 3;
    scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kSmallReadSize);
    rv = trans->Read(buf, kSmallReadSize, &read_callback);
    if (rv == net::ERR_IO_PENDING) {
      data->CompleteRead();
      rv = read_callback.WaitForResult();
    }
    if (rv > 0) {
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      NOTREACHED();
    }
  } while (rv > 0);

  out.response_data.swap(content);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("goodbye world", out.response_data);
}

TEST_F(SpdyNetworkTransactionTest, ConnectFailureFallbackToHttp) {
  MockConnect connects[]  = {
    MockConnect(true, ERR_NAME_NOT_RESOLVED),
    MockConnect(false, ERR_NAME_NOT_RESOLVED),
    MockConnect(true, ERR_INTERNET_DISCONNECTED),
    MockConnect(false, ERR_INTERNET_DISCONNECTED)
  };

  for (size_t index = 0; index < arraysize(connects); ++index) {
    scoped_ptr<spdy::SpdyFrame> req(
        ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
    MockWrite writes[] = {
      CreateMockWrite(*req),
      MockWrite(true, 0, 0)  // EOF
    };

    scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
    scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
    MockRead reads[] = {
      CreateMockRead(*resp),
      CreateMockRead(*body),
      MockRead(true, 0, 0)  // EOF
    };

    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(connects[index], 1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                       BoundNetLog());
    helper.AddData(data.get());
    helper.RunPreTestSetup();

    // Set up http fallback data.
    MockRead http_fallback_data[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"),
      MockRead("hello world!!!"),
      MockRead(true, OK),
    };

    scoped_ptr<StaticSocketDataProvider> http_fallback(
          new StaticSocketDataProvider(http_fallback_data,
                                       arraysize(http_fallback_data),
                                       NULL, 0));
    helper.AddDataNoSSL(http_fallback.get());
    HttpNetworkTransaction* trans = helper.trans();
    TestCompletionCallback callback;

    int rv = trans->Start(&helper.request(), &callback, BoundNetLog());
    EXPECT_EQ(rv, ERR_IO_PENDING);
    rv = callback.WaitForResult();
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != NULL);
    ASSERT_TRUE(response->headers != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    std::string response_data;
    rv = ReadTransaction(trans, &response_data);
    EXPECT_EQ(OK, rv);

    EXPECT_TRUE(!response->was_fetched_via_spdy);
    EXPECT_TRUE(!response->was_npn_negotiated);
    EXPECT_TRUE(response->was_alternate_protocol_available);
    EXPECT_TRUE(http_fallback->at_read_eof());
    EXPECT_EQ(0u, data->read_index());
    EXPECT_EQ(0u, data->write_index());
    EXPECT_EQ("hello world!!!", response_data);
  }
}

// Verify that basic buffering works; when multiple data frames arrive
// at the same time, ensure that we don't notify a read completion for
// each data frame individually.
TEST_F(SpdyNetworkTransactionTest, Buffering) {
  spdy::SpdyFramer framer;

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // 4 data frames in a single read.
  scoped_ptr<spdy::SpdyFrame> data_frame(
      framer.CreateDataFrame(1, "message", 7, spdy::DATA_FLAG_NONE));
  scoped_ptr<spdy::SpdyFrame> data_frame_fin(
      framer.CreateDataFrame(1, "message", 7, spdy::DATA_FLAG_FIN));
  const spdy::SpdyFrame* data_frames[4] = {
    data_frame.get(),
    data_frame.get(),
    data_frame.get(),
    data_frame_fin.get()
  };
  char combined_data_frames[100];
  int combined_data_frames_len =
      CombineFrames(data_frames, arraysize(data_frames),
                    combined_data_frames, arraysize(combined_data_frames));

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(true, ERR_IO_PENDING),  // Force a pause
    MockRead(true, combined_data_frames, combined_data_frames_len),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.AddData(data.get());
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 14;
    scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kSmallReadSize);
    rv = trans->Read(buf, kSmallReadSize, &read_callback);
    if (rv == net::ERR_IO_PENDING) {
      data->CompleteRead();
      rv = read_callback.WaitForResult();
    }
    if (rv > 0) {
      EXPECT_EQ(kSmallReadSize, rv);
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      FAIL() << "Unexpected read error: " << rv;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(3, reads_completed);  // Reads are: 14 bytes, 14 bytes, 0 bytes.

  out.response_data.swap(content);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("messagemessagemessagemessage", out.response_data);
}

// Verify the case where we buffer data but read it after it has been buffered.
TEST_F(SpdyNetworkTransactionTest, BufferedAll) {
  spdy::SpdyFramer framer;

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // 5 data frames in a single read.
  scoped_ptr<spdy::SpdyFrame> syn_reply(
      ConstructSpdyGetSynReply(NULL, 0, 1));
  syn_reply->set_flags(spdy::CONTROL_FLAG_NONE);  // turn off FIN bit
  scoped_ptr<spdy::SpdyFrame> data_frame(
      framer.CreateDataFrame(1, "message", 7, spdy::DATA_FLAG_NONE));
  scoped_ptr<spdy::SpdyFrame> data_frame_fin(
      framer.CreateDataFrame(1, "message", 7, spdy::DATA_FLAG_FIN));
  const spdy::SpdyFrame* frames[5] = {
    syn_reply.get(),
    data_frame.get(),
    data_frame.get(),
    data_frame.get(),
    data_frame_fin.get()
  };
  char combined_frames[200];
  int combined_frames_len =
      CombineFrames(frames, arraysize(frames),
                    combined_frames, arraysize(combined_frames));

  MockRead reads[] = {
    MockRead(true, combined_frames, combined_frames_len),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.AddData(data.get());
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 14;
    scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kSmallReadSize);
    rv = trans->Read(buf, kSmallReadSize, &read_callback);
    if (rv > 0) {
      EXPECT_EQ(kSmallReadSize, rv);
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      FAIL() << "Unexpected read error: " << rv;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(3, reads_completed);

  out.response_data.swap(content);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("messagemessagemessagemessage", out.response_data);
}

// Verify the case where we buffer data and close the connection.
TEST_F(SpdyNetworkTransactionTest, BufferedClosed) {
  spdy::SpdyFramer framer;

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // All data frames in a single read.
  // NOTE: We don't FIN the stream.
  scoped_ptr<spdy::SpdyFrame> data_frame(
      framer.CreateDataFrame(1, "message", 7, spdy::DATA_FLAG_NONE));
  const spdy::SpdyFrame* data_frames[4] = {
    data_frame.get(),
    data_frame.get(),
    data_frame.get(),
    data_frame.get()
  };
  char combined_data_frames[100];
  int combined_data_frames_len =
      CombineFrames(data_frames, arraysize(data_frames),
                    combined_data_frames, arraysize(combined_data_frames));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(true, ERR_IO_PENDING),  // Force a wait
    MockRead(true, combined_data_frames, combined_data_frames_len),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.AddData(data.get());
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;

  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 14;
    scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kSmallReadSize);
    rv = trans->Read(buf, kSmallReadSize, &read_callback);
    if (rv == net::ERR_IO_PENDING) {
      data->CompleteRead();
      rv = read_callback.WaitForResult();
    }
    if (rv > 0) {
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      // This test intentionally closes the connection, and will get an error.
      EXPECT_EQ(ERR_CONNECTION_CLOSED, rv);
      break;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(0, reads_completed);

  out.response_data.swap(content);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();
}

// Verify the case where we buffer data and cancel the transaction.
TEST_F(SpdyNetworkTransactionTest, BufferedCancelled) {
  spdy::SpdyFramer framer;

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // NOTE: We don't FIN the stream.
  scoped_ptr<spdy::SpdyFrame> data_frame(
      framer.CreateDataFrame(1, "message", 7, spdy::DATA_FLAG_NONE));

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(true, ERR_IO_PENDING),  // Force a wait
    CreateMockRead(*data_frame),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.AddData(data.get());
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();
  TestCompletionCallback callback;

  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  do {
    const int kReadSize = 256;
    scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kReadSize);
    rv = trans->Read(buf, kReadSize, &read_callback);
    if (rv == net::ERR_IO_PENDING) {
      // Complete the read now, which causes buffering to start.
      data->CompleteRead();
      // Destroy the transaction, causing the stream to get cancelled
      // and orphaning the buffered IO task.
      helper.ResetTrans();
      break;
    }
    // We shouldn't get here in this test.
    FAIL() << "Unexpected read: " << rv;
  } while (rv > 0);

  // Flush the MessageLoop; this will cause the buffered IO task
  // to run for the final time.
  MessageLoop::current()->RunAllPending();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();
}

// Test that if the server requests persistence of settings, that we save
// the settings in the SpdySettingsStorage.
TEST_F(SpdyNetworkTransactionTest, SettingsSaved) {
  static const SpdyHeaderInfo kSynReplyInfo = {
    spdy::SYN_REPLY,                              // Syn Reply
    1,                                            // Stream ID
    0,                                            // Associated Stream ID
    SPDY_PRIORITY_LOWEST,                         // Priority
    spdy::CONTROL_FLAG_NONE,                      // Control Flags
    false,                                        // Compressed
    spdy::INVALID,                                // Status
    NULL,                                         // Data
    0,                                            // Data Length
    spdy::DATA_FLAG_NONE                          // Data Flags
  };
  static const char* const kExtraHeaders[] = {
    "status",   "200",
    "version",  "HTTP/1.1"
  };

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());

  // Verify that no settings exist initially.
  HostPortPair host_port_pair("www.google.com", 443);
  EXPECT_TRUE(helper.session()->spdy_settings().Get(host_port_pair).empty());

  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // Construct the reply.
  scoped_ptr<spdy::SpdyFrame> reply(
    ConstructSpdyPacket(kSynReplyInfo,
                        kExtraHeaders,
                        arraysize(kExtraHeaders) / 2,
                        NULL,
                        0));

  unsigned int kSampleId1 = 0x1;
  unsigned int kSampleValue1 = 0x0a0a0a0a;
  unsigned int kSampleId2 = 0x2;
  unsigned int kSampleValue2 = 0x0b0b0b0b;
  unsigned int kSampleId3 = 0xababab;
  unsigned int kSampleValue3 = 0x0c0c0c0c;
  scoped_ptr<spdy::SpdyFrame> settings_frame;
  {
    // Construct the SETTINGS frame.
    spdy::SpdySettings settings;
    spdy::SettingsFlagsAndId setting(0);
    // First add a persisted setting
    setting.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
    setting.set_id(kSampleId1);
    settings.push_back(std::make_pair(setting, kSampleValue1));
    // Next add a non-persisted setting
    setting.set_flags(0);
    setting.set_id(kSampleId2);
    settings.push_back(std::make_pair(setting, kSampleValue2));
    // Next add another persisted setting
    setting.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
    setting.set_id(kSampleId3);
    settings.push_back(std::make_pair(setting, kSampleValue3));
    settings_frame.reset(ConstructSpdySettings(settings));
  }

  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*reply),
    CreateMockRead(*body),
    CreateMockRead(*settings_frame),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  {
    // Verify we had two persisted settings.
    spdy::SpdySettings saved_settings =
        helper.session()->spdy_settings().Get(host_port_pair);
    ASSERT_EQ(2u, saved_settings.size());

    // Verify the first persisted setting.
    spdy::SpdySetting setting = saved_settings.front();
    saved_settings.pop_front();
    EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, setting.first.flags());
    EXPECT_EQ(kSampleId1, setting.first.id());
    EXPECT_EQ(kSampleValue1, setting.second);

    // Verify the second persisted setting.
    setting = saved_settings.front();
    saved_settings.pop_front();
    EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, setting.first.flags());
    EXPECT_EQ(kSampleId3, setting.first.id());
    EXPECT_EQ(kSampleValue3, setting.second);
  }
}

// Test that when there are settings saved that they are sent back to the
// server upon session establishment.
TEST_F(SpdyNetworkTransactionTest, SettingsPlayback) {
  static const SpdyHeaderInfo kSynReplyInfo = {
    spdy::SYN_REPLY,                              // Syn Reply
    1,                                            // Stream ID
    0,                                            // Associated Stream ID
    SPDY_PRIORITY_LOWEST,                         // Priority
    spdy::CONTROL_FLAG_NONE,                      // Control Flags
    false,                                        // Compressed
    spdy::INVALID,                                // Status
    NULL,                                         // Data
    0,                                            // Data Length
    spdy::DATA_FLAG_NONE                          // Data Flags
  };
  static const char* kExtraHeaders[] = {
    "status",   "200",
    "version",  "HTTP/1.1"
  };

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());

  // Verify that no settings exist initially.
  HostPortPair host_port_pair("www.google.com", 443);
  EXPECT_TRUE(helper.session()->spdy_settings().Get(host_port_pair).empty());

  unsigned int kSampleId1 = 0x1;
  unsigned int kSampleValue1 = 0x0a0a0a0a;
  unsigned int kSampleId2 = 0xababab;
  unsigned int kSampleValue2 = 0x0c0c0c0c;
  // Manually insert settings into the SpdySettingsStorage here.
  {
    spdy::SpdySettings settings;
    spdy::SettingsFlagsAndId setting(0);
    // First add a persisted setting
    setting.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
    setting.set_id(kSampleId1);
    settings.push_back(std::make_pair(setting, kSampleValue1));
    // Next add another persisted setting
    setting.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
    setting.set_id(kSampleId2);
    settings.push_back(std::make_pair(setting, kSampleValue2));

    helper.session()->mutable_spdy_settings()->Set(host_port_pair, settings);
  }

  EXPECT_EQ(2u, helper.session()->spdy_settings().Get(host_port_pair).size());

  // Construct the SETTINGS frame.
  const spdy::SpdySettings& settings =
      helper.session()->spdy_settings().Get(host_port_pair);
  scoped_ptr<spdy::SpdyFrame> settings_frame(ConstructSpdySettings(settings));

  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));

  MockWrite writes[] = {
    CreateMockWrite(*settings_frame),
    CreateMockWrite(*req),
  };

  // Construct the reply.
  scoped_ptr<spdy::SpdyFrame> reply(
    ConstructSpdyPacket(kSynReplyInfo,
                        kExtraHeaders,
                        arraysize(kExtraHeaders) / 2,
                        NULL,
                        0));

  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*reply),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  {
    // Verify we had two persisted settings.
    spdy::SpdySettings saved_settings =
        helper.session()->spdy_settings().Get(host_port_pair);
    ASSERT_EQ(2u, saved_settings.size());

    // Verify the first persisted setting.
    spdy::SpdySetting setting = saved_settings.front();
    saved_settings.pop_front();
    EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, setting.first.flags());
    EXPECT_EQ(kSampleId1, setting.first.id());
    EXPECT_EQ(kSampleValue1, setting.second);

    // Verify the second persisted setting.
    setting = saved_settings.front();
    saved_settings.pop_front();
    EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, setting.first.flags());
    EXPECT_EQ(kSampleId2, setting.first.id());
    EXPECT_EQ(kSampleValue2, setting.second);
  }
}

TEST_F(SpdyNetworkTransactionTest, GoAwayWithActiveStream) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> go_away(ConstructSpdyGoAway());
  MockRead reads[] = {
    CreateMockRead(*go_away),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_CONNECTION_CLOSED, out.rv);
}

TEST_F(SpdyNetworkTransactionTest, CloseWithActiveStream) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(false, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  BoundNetLog log;
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     log);
  helper.AddData(data.get());
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  TransactionHelperResult out;
  out.rv = trans->Start(&CreateGetRequest(), &callback, log);

  EXPECT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.rv = ReadTransaction(trans, &out.response_data);
  EXPECT_EQ(ERR_CONNECTION_CLOSED, out.rv);

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();
}

}  // namespace net
