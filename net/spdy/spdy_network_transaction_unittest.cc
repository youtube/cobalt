// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_transaction.h"

#include <string>
#include <vector>

#include "net/base/auth.h"
#include "net/base/net_log_unittest.h"
#include "net/http/http_network_session_peer.h"
#include "net/http/http_transaction_unittest.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

// This is the expected list of advertised protocols from the browser's NPN
// list.
static const char kExpectedNPNString[] = "\x08http/1.1\x06spdy/2";

enum SpdyNetworkTransactionTestTypes {
  SPDYNPN,
  SPDYNOSSL,
  SPDYSSL,
};
class SpdyNetworkTransactionTest
    : public ::testing::TestWithParam<SpdyNetworkTransactionTestTypes> {
 protected:
  virtual void SetUp() {
    // By default, all tests turn off compression.
    EnableCompression(false);
    google_get_request_initialized_ = false;
    google_post_request_initialized_ = false;
    google_chunked_post_request_initialized_ = false;
  }

  virtual void TearDown() {
    // Empty the current queue.
    MessageLoop::current()->RunAllPending();
  }

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
                                const BoundNetLog& log,
                                SpdyNetworkTransactionTestTypes test_type)
        : request_(request),
          session_deps_(new SpdySessionDependencies()),
          session_(SpdySessionDependencies::SpdyCreateSession(
              session_deps_.get())),
          log_(log),
          test_type_(test_type),
          deterministic_(false),
          spdy_enabled_(true) {
            switch (test_type_) {
              case SPDYNOSSL:
              case SPDYSSL:
                port_ = 80;
                break;
              case SPDYNPN:
                port_ = 443;
                break;
              default:
                NOTREACHED();
            }
          }

    ~NormalSpdyTransactionHelper() {
      // Any test which doesn't close the socket by sending it an EOF will
      // have a valid session left open, which leaks the entire session pool.
      // This is just fine - in fact, some of our tests intentionally do this
      // so that we can check consistency of the SpdySessionPool as the test
      // finishes.  If we had put an EOF on the socket, the SpdySession would
      // have closed and we wouldn't be able to check the consistency.

      // Forcefully close existing sessions here.
      session()->spdy_session_pool()->CloseAllSessions();
    }

    void SetDeterministic() {
      session_ = SpdySessionDependencies::SpdyCreateSessionDeterministic(
          session_deps_.get());
      deterministic_ = true;
    }

    void SetSpdyDisabled() {
      spdy_enabled_ = false;
    }

    void RunPreTestSetup() {
      if (!session_deps_.get())
        session_deps_.reset(new SpdySessionDependencies());
      if (!session_.get())
        session_ = SpdySessionDependencies::SpdyCreateSession(
            session_deps_.get());
      HttpStreamFactory::set_use_alternate_protocols(false);
      HttpStreamFactory::set_force_spdy_over_ssl(false);
      HttpStreamFactory::set_force_spdy_always(false);
      switch (test_type_) {
        case SPDYNPN:
          session_->mutable_alternate_protocols()->SetAlternateProtocolFor(
              HostPortPair("www.google.com", 80), 443,
              HttpAlternateProtocols::NPN_SPDY_2);
          HttpStreamFactory::set_use_alternate_protocols(true);
          HttpStreamFactory::set_next_protos(kExpectedNPNString);
          break;
        case SPDYNOSSL:
          HttpStreamFactory::set_force_spdy_over_ssl(false);
          HttpStreamFactory::set_force_spdy_always(true);
          break;
        case SPDYSSL:
          HttpStreamFactory::set_force_spdy_over_ssl(true);
          HttpStreamFactory::set_force_spdy_always(true);
          break;
        default:
          NOTREACHED();
      }

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
        session_->spdy_session_pool()->CloseCurrentSessions();
        return;
      }

      // Verify responses.
      const HttpResponseInfo* response = trans_->GetResponseInfo();
      ASSERT_TRUE(response != NULL);
      ASSERT_TRUE(response->headers != NULL);
      EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
      EXPECT_EQ(spdy_enabled_, response->was_fetched_via_spdy);
      if (test_type_ == SPDYNPN && spdy_enabled_) {
        EXPECT_TRUE(response->was_npn_negotiated);
        EXPECT_TRUE(response->was_alternate_protocol_available);
      } else {
        EXPECT_TRUE(!response->was_npn_negotiated);
        EXPECT_TRUE(!response->was_alternate_protocol_available);
      }
      // If SPDY is not enabled, a HTTP request should not be diverted
      // over a SSL session.
      if (!spdy_enabled_) {
        EXPECT_EQ(request_.url.SchemeIs("https"),
                  response->was_npn_negotiated);
      }
      EXPECT_EQ("192.0.2.33", response->socket_address.host());
      EXPECT_EQ(0, response->socket_address.port());
      output_.status_line = response->headers->GetStatusLine();
      output_.response_info = *response;  // Make a copy so we can verify.
      output_.rv = ReadTransaction(trans_.get(), &output_.response_data);
    }

    // Most tests will want to call this function. In particular, the MockReads
    // should end with an empty read, and that read needs to be processed to
    // ensure proper deletion of the spdy_session_pool.
    void VerifyDataConsumed() {
      for (DataVector::iterator it = data_vector_.begin();
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
      for (DataVector::iterator it = data_vector_.begin();
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
      RunPreTestSetup();
      AddData(data);
      RunDefaultTest();
      VerifyDataConsumed();
    }

    void AddData(StaticSocketDataProvider* data) {
      DCHECK(!deterministic_);
      data_vector_.push_back(data);
      linked_ptr<SSLSocketDataProvider> ssl_(
          new SSLSocketDataProvider(true, OK));
      if (test_type_ == SPDYNPN) {
        ssl_->next_proto_status = SSLClientSocket::kNextProtoNegotiated;
        ssl_->next_proto = "spdy/2";
        ssl_->was_npn_negotiated = true;
      }
      ssl_vector_.push_back(ssl_);
      if(test_type_ == SPDYNPN || test_type_ == SPDYSSL)
        session_deps_->socket_factory->AddSSLSocketDataProvider(ssl_.get());
      session_deps_->socket_factory->AddSocketDataProvider(data);
    }

    void AddDeterministicData(DeterministicSocketData* data) {
      DCHECK(deterministic_);
      data_vector_.push_back(data);
      linked_ptr<SSLSocketDataProvider> ssl_(
          new SSLSocketDataProvider(true, OK));
      if (test_type_ == SPDYNPN) {
        ssl_->next_proto_status = SSLClientSocket::kNextProtoNegotiated;
        ssl_->next_proto = "spdy/2";
        ssl_->was_npn_negotiated = true;
      }
      ssl_vector_.push_back(ssl_);
      if(test_type_ == SPDYNPN || test_type_ == SPDYSSL)
        session_deps_->deterministic_socket_factory->
            AddSSLSocketDataProvider(ssl_.get());
      session_deps_->deterministic_socket_factory->AddSocketDataProvider(data);
    }

    // This can only be called after RunPreTestSetup. It adds a Data Provider,
    // but not a corresponding SSL data provider
    void AddDataNoSSL(StaticSocketDataProvider* data) {
      DCHECK(!deterministic_);
      session_deps_->socket_factory->AddSocketDataProvider(data);
    }
    void AddDataNoSSL(DeterministicSocketData* data) {
      DCHECK(deterministic_);
      session_deps_->deterministic_socket_factory->AddSocketDataProvider(data);
    }

    void SetSession(const scoped_refptr<HttpNetworkSession>& session) {
      session_ = session;
    }
    HttpNetworkTransaction* trans() { return trans_.get(); }
    void ResetTrans() { trans_.reset(); }
    TransactionHelperResult& output() { return output_; }
    const HttpRequestInfo& request() const { return request_; }
    const scoped_refptr<HttpNetworkSession>& session() const {
      return session_;
    }
    scoped_ptr<SpdySessionDependencies>& session_deps() {
      return session_deps_;
    }
    int port() const { return port_; }
    SpdyNetworkTransactionTestTypes test_type() const { return test_type_; }

   private:
    typedef std::vector<StaticSocketDataProvider*> DataVector;
    typedef std::vector<linked_ptr<SSLSocketDataProvider> > SSLVector;
    HttpRequestInfo request_;
    scoped_ptr<SpdySessionDependencies> session_deps_;
    scoped_refptr<HttpNetworkSession> session_;
    TransactionHelperResult output_;
    scoped_ptr<StaticSocketDataProvider> first_transaction_;
    SSLVector ssl_vector_;
    TestCompletionCallback callback;
    scoped_ptr<HttpNetworkTransaction> trans_;
    scoped_ptr<HttpNetworkTransaction> trans_http_;
    DataVector data_vector_;
    const BoundNetLog& log_;
    SpdyNetworkTransactionTestTypes test_type_;
    int port_;
    bool deterministic_;
    bool spdy_enabled_;
  };

  void ConnectStatusHelperWithExpectedStatus(const MockRead& status,
                                             int expected_status);

  void ConnectStatusHelper(const MockRead& status);

  const HttpRequestInfo& CreateGetPushRequest() {
    google_get_push_request_.method = "GET";
    google_get_push_request_.url = GURL("http://www.google.com/foo.dat");
    google_get_push_request_.load_flags = 0;
    return google_get_push_request_;
  }

  const HttpRequestInfo& CreateGetRequest() {
    if (!google_get_request_initialized_) {
      google_get_request_.method = "GET";
      google_get_request_.url = GURL(kDefaultURL);
      google_get_request_.load_flags = 0;
      google_get_request_initialized_ = true;
    }
    return google_get_request_;
  }

  const HttpRequestInfo& CreateGetRequestWithUserAgent() {
    if (!google_get_request_initialized_) {
      google_get_request_.method = "GET";
      google_get_request_.url = GURL(kDefaultURL);
      google_get_request_.load_flags = 0;
      google_get_request_.extra_headers.SetHeader("User-Agent", "Chrome");
      google_get_request_initialized_ = true;
    }
    return google_get_request_;
  }

  const HttpRequestInfo& CreatePostRequest() {
    if (!google_post_request_initialized_) {
      google_post_request_.method = "POST";
      google_post_request_.url = GURL(kDefaultURL);
      google_post_request_.upload_data = new UploadData();
      google_post_request_.upload_data->AppendBytes(kUploadData,
                                                    kUploadDataSize);
      google_post_request_initialized_ = true;
    }
    return google_post_request_;
  }

  const HttpRequestInfo& CreateChunkedPostRequest() {
    if (!google_chunked_post_request_initialized_) {
      google_chunked_post_request_.method = "POST";
      google_chunked_post_request_.url = GURL(kDefaultURL);
      google_chunked_post_request_.upload_data = new UploadData();
      google_chunked_post_request_.upload_data->set_is_chunked(true);
      google_chunked_post_request_.upload_data->AppendChunk(
          kUploadData, kUploadDataSize, false);
      google_chunked_post_request_.upload_data->AppendChunk(
          kUploadData, kUploadDataSize, true);
      google_chunked_post_request_initialized_ = true;
    }
    return google_chunked_post_request_;
  }

  // Read the result of a particular transaction, knowing that we've got
  // multiple transactions in the read pipeline; so as we read, we may have
  // to skip over data destined for other transactions while we consume
  // the data for |trans|.
  int ReadResult(HttpNetworkTransaction* trans,
                 StaticSocketDataProvider* data,
                 std::string* result) {
    const int kSize = 3000;

    int bytes_read = 0;
    scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(kSize));
    TestCompletionCallback callback;
    while (true) {
      int rv = trans->Read(buf, kSize, &callback);
      if (rv == ERR_IO_PENDING) {
        // Multiple transactions may be in the data set.  Keep pulling off
        // reads until we complete our callback.
        while (!callback.have_result()) {
          data->CompleteRead();
          MessageLoop::current()->RunAllPending();
        }
        rv = callback.WaitForResult();
      } else if (rv <= 0) {
        break;
      }
      result->append(buf->data(), rv);
      bytes_read += rv;
    }
    return bytes_read;
  }

  void VerifyStreamsClosed(const NormalSpdyTransactionHelper& helper) {
    // This lengthy block is reaching into the pool to dig out the active
    // session.  Once we have the session, we verify that the streams are
    // all closed and not leaked at this point.
    const GURL& url = helper.request().url;
    int port = helper.test_type() == SPDYNPN ? 443 : 80;
    HostPortPair host_port_pair(url.host(), port);
    HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
    BoundNetLog log;
    const scoped_refptr<HttpNetworkSession>& session = helper.session();
    SpdySessionPool* pool(session->spdy_session_pool());
    EXPECT_TRUE(pool->HasSession(pair));
    scoped_refptr<SpdySession> spdy_session(pool->Get(pair, log));
    ASSERT_TRUE(spdy_session.get() != NULL);
    EXPECT_EQ(0u, spdy_session->num_active_streams());
    EXPECT_EQ(0u, spdy_session->num_unclaimed_pushed_streams());
  }

  void RunServerPushTest(OrderedSocketData* data,
                         HttpResponseInfo* response,
                         HttpResponseInfo* push_response,
                         std::string& expected) {
    NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                       BoundNetLog(), GetParam());
    helper.RunPreTestSetup();
    helper.AddData(data);

    HttpNetworkTransaction* trans = helper.trans();

    // Start the transaction with basic parameters.
    TestCompletionCallback callback;
    int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    rv = callback.WaitForResult();

    // Request the pushed path.
    scoped_ptr<HttpNetworkTransaction> trans2(
        new HttpNetworkTransaction(helper.session()));
    rv = trans2->Start(&CreateGetPushRequest(), &callback, BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    MessageLoop::current()->RunAllPending();

    // The data for the pushed path may be coming in more than 1 packet. Compile
    // the results into a single string.

    // Read the server push body.
    std::string result2;
    ReadResult(trans2.get(), data, &result2);
    // Read the response body.
    std::string result;
    ReadResult(trans, data, &result);

    // Verify that we consumed all test data.
    EXPECT_TRUE(data->at_read_eof());
    EXPECT_TRUE(data->at_write_eof());

    // Verify that the received push data is same as the expected push data.
    EXPECT_EQ(result2.compare(expected), 0) << "Received data: "
                                            << result2
                                            << "||||| Expected data: "
                                            << expected;

    // Verify the SYN_REPLY.
    // Copy the response info, because trans goes away.
    *response = *trans->GetResponseInfo();
    *push_response = *trans2->GetResponseInfo();

    VerifyStreamsClosed(helper);
  }

 private:
  bool google_get_request_initialized_;
  bool google_post_request_initialized_;
  bool google_chunked_post_request_initialized_;
  HttpRequestInfo google_get_request_;
  HttpRequestInfo google_post_request_;
  HttpRequestInfo google_chunked_post_request_;
  HttpRequestInfo google_get_push_request_;
};

//-----------------------------------------------------------------------------
// All tests are run with three different connection types: SPDY after NPN
// negotiation, SPDY without SSL, and SPDY with SSL.
INSTANTIATE_TEST_CASE_P(Spdy,
                        SpdyNetworkTransactionTest,
                        ::testing::Values(SPDYNOSSL, SPDYSSL, SPDYNPN));


// Verify HttpNetworkTransaction constructor.
TEST_P(SpdyNetworkTransactionTest, Constructor) {
  SpdySessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));
  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(session));
}

TEST_P(SpdyNetworkTransactionTest, Get) {
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
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, GetAtEachPriority) {
  for (RequestPriority p = HIGHEST; p < NUM_PRIORITIES;
       p = RequestPriority(p+1)) {
    // Construct the request.
    scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, p));
    MockWrite writes[] = { CreateMockWrite(*req) };

    const int spdy_prio = reinterpret_cast<spdy::SpdySynStreamControlFrame*>(
        req.get())->priority();
    // this repeats the RequestPriority-->SpdyPriority mapping from
    // SpdyFramer::ConvertRequestPriorityToSpdyPriority to make
    // sure it's being done right.
    switch(p) {
      case HIGHEST:
        EXPECT_EQ(0, spdy_prio);
        break;
      case MEDIUM:
        EXPECT_EQ(1, spdy_prio);
        break;
      case LOW:
      case LOWEST:
        EXPECT_EQ(2, spdy_prio);
        break;
      case IDLE:
        EXPECT_EQ(3, spdy_prio);
        break;
      default:
        FAIL();
    }

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
    HttpRequestInfo http_req = CreateGetRequest();
    http_req.priority = p;

    NormalSpdyTransactionHelper helper(http_req, BoundNetLog(), GetParam());
    helper.RunToCompletion(data.get());
    TransactionHelperResult out = helper.output();
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!", out.response_data);
  }
}

// Start three gets simultaniously; making sure that multiplexed
// streams work properly.

// This can't use the TransactionHelper method, since it only
// handles a single transaction, and finishes them as soon
// as it launches them.

// TODO(gavinp): create a working generalized TransactionHelper that
// can allow multiple streams in flight.

TEST_P(SpdyNetworkTransactionTest, ThreeGets) {
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

  MockWrite writes[] = {
    CreateMockWrite(*req),
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
  scoped_refptr<OrderedSocketData> data_placeholder(
      new OrderedSocketData(NULL, 0, NULL, 0));

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
  // We require placeholder data because three get requests are sent out, so
  // there needs to be three sets of SSL connection data.
  helper.AddData(data_placeholder.get());
  helper.AddData(data_placeholder.get());
  scoped_ptr<HttpNetworkTransaction> trans1(
      new HttpNetworkTransaction(helper.session()));
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(helper.session()));
  scoped_ptr<HttpNetworkTransaction> trans3(
      new HttpNetworkTransaction(helper.session()));

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
  helper.VerifyDataConsumed();
  EXPECT_EQ(OK, out.rv);

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, TwoGetsLateBinding) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> fbody(ConstructSpdyBodyFrame(1, true));

  scoped_ptr<spdy::SpdyFrame> req2(ConstructSpdyGet(NULL, 0, false, 3, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<spdy::SpdyFrame> body2(ConstructSpdyBodyFrame(3, false));
  scoped_ptr<spdy::SpdyFrame> fbody2(ConstructSpdyBodyFrame(3, true));

  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*req2),
  };
  MockRead reads[] = {
    CreateMockRead(*resp, 1),
    CreateMockRead(*body),
    CreateMockRead(*resp2, 4),
    CreateMockRead(*body2),
    CreateMockRead(*fbody),
    CreateMockRead(*fbody2),
    MockRead(true, 0, 0),  // EOF
  };
  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));

  MockConnect never_finishing_connect(true, ERR_IO_PENDING);

  scoped_refptr<OrderedSocketData> data_placeholder(
      new OrderedSocketData(NULL, 0, NULL, 0));
  data_placeholder->set_connect_data(never_finishing_connect);

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
  // We require placeholder data because two get requests are sent out, so
  // there needs to be two sets of SSL connection data.
  helper.AddData(data_placeholder.get());
  scoped_ptr<HttpNetworkTransaction> trans1(
      new HttpNetworkTransaction(helper.session()));
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(helper.session()));

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;

  HttpRequestInfo httpreq1 = CreateGetRequest();
  HttpRequestInfo httpreq2 = CreateGetRequest();

  out.rv = trans1->Start(&httpreq1, &callback1, log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);
  out.rv = trans2->Start(&httpreq2, &callback2, log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);

  out.rv = callback1.WaitForResult();
  ASSERT_EQ(OK, out.rv);
  out.rv = callback2.WaitForResult();
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
  EXPECT_TRUE(response2->headers != NULL);
  EXPECT_TRUE(response2->was_fetched_via_spdy);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(trans2.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, TwoGetsLateBindingFromPreconnect) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> fbody(ConstructSpdyBodyFrame(1, true));

  scoped_ptr<spdy::SpdyFrame> req2(ConstructSpdyGet(NULL, 0, false, 3, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<spdy::SpdyFrame> body2(ConstructSpdyBodyFrame(3, false));
  scoped_ptr<spdy::SpdyFrame> fbody2(ConstructSpdyBodyFrame(3, true));

  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*req2),
  };
  MockRead reads[] = {
    CreateMockRead(*resp, 1),
    CreateMockRead(*body),
    CreateMockRead(*resp2, 4),
    CreateMockRead(*body2),
    CreateMockRead(*fbody),
    CreateMockRead(*fbody2),
    MockRead(true, 0, 0),  // EOF
  };
  scoped_refptr<OrderedSocketData> preconnect_data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));

  MockConnect never_finishing_connect(true, ERR_IO_PENDING);

  scoped_refptr<OrderedSocketData> data_placeholder(
      new OrderedSocketData(NULL, 0, NULL, 0));
  data_placeholder->set_connect_data(never_finishing_connect);

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(preconnect_data.get());
  // We require placeholder data because 3 connections are attempted (first is
  // the preconnect, 2nd and 3rd are the never finished connections.
  helper.AddData(data_placeholder.get());
  helper.AddData(data_placeholder.get());

  scoped_ptr<HttpNetworkTransaction> trans1(
      new HttpNetworkTransaction(helper.session()));
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(helper.session()));

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;

  HttpRequestInfo httpreq = CreateGetRequest();

  // Preconnect the first.
  SSLConfig preconnect_ssl_config;
  helper.session()->ssl_config_service()->GetSSLConfig(&preconnect_ssl_config);
  HttpStreamFactory* http_stream_factory =
      helper.session()->http_stream_factory();
  if (http_stream_factory->next_protos()) {
    preconnect_ssl_config.next_protos = *http_stream_factory->next_protos();
  }

  http_stream_factory->PreconnectStreams(
      1, httpreq, preconnect_ssl_config, log);

  out.rv = trans1->Start(&httpreq, &callback1, log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);
  out.rv = trans2->Start(&httpreq, &callback2, log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);

  out.rv = callback1.WaitForResult();
  ASSERT_EQ(OK, out.rv);
  out.rv = callback2.WaitForResult();
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
  EXPECT_TRUE(response2->headers != NULL);
  EXPECT_TRUE(response2->was_fetched_via_spdy);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(trans2.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  helper.VerifyDataConsumed();
}

// Similar to ThreeGets above, however this test adds a SETTINGS
// frame.  The SETTINGS frame is read during the IO loop waiting on
// the first transaction completion, and sets a maximum concurrent
// stream limit of 1.  This means that our IO loop exists after the
// second transaction completes, so we can assert on read_index().
TEST_P(SpdyNetworkTransactionTest, ThreeGetsWithMaxConcurrent) {
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
    CreateMockRead(*settings_frame, 1),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    CreateMockRead(*fbody),
    CreateMockRead(*resp2, 7),
    CreateMockRead(*body2),
    CreateMockRead(*fbody2),
    CreateMockRead(*resp3, 12),
    CreateMockRead(*body3),
    CreateMockRead(*fbody3),

    MockRead(true, 0, 0),  // EOF
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  scoped_refptr<OrderedSocketData> data_placeholder(
      new OrderedSocketData(NULL, 0, NULL, 0));

  BoundNetLog log;
  TransactionHelperResult out;
  {
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
  // We require placeholder data because three get requests are sent out, so
  // there needs to be three sets of SSL connection data.
  helper.AddData(data_placeholder.get());
  helper.AddData(data_placeholder.get());
    scoped_ptr<HttpNetworkTransaction> trans1(
        new HttpNetworkTransaction(helper.session()));
    scoped_ptr<HttpNetworkTransaction> trans2(
        new HttpNetworkTransaction(helper.session()));
    scoped_ptr<HttpNetworkTransaction> trans3(
        new HttpNetworkTransaction(helper.session()));

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
    ASSERT_EQ(OK, out.rv);

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
    ASSERT_TRUE(response1 != NULL);
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

    helper.VerifyDataConsumed();
  }
  EXPECT_EQ(OK, out.rv);
}

// Similar to ThreeGetsWithMaxConcurrent above, however this test adds
// a fourth transaction.  The third and fourth transactions have
// different data ("hello!" vs "hello!hello!") and because of the
// user specified priority, we expect to see them inverted in
// the response from the server.
TEST_P(SpdyNetworkTransactionTest, FourGetsWithMaxConcurrentPriority) {
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
    CreateMockRead(*settings_frame, 1),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    CreateMockRead(*fbody),
    CreateMockRead(*resp2, 7),
    CreateMockRead(*body2),
    CreateMockRead(*fbody2),
    CreateMockRead(*resp4, 13),
    CreateMockRead(*fbody4),
    CreateMockRead(*resp3, 16),
    CreateMockRead(*body3),
    CreateMockRead(*fbody3),

    MockRead(true, 0, 0),  // EOF
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
        writes, arraysize(writes)));
  scoped_refptr<OrderedSocketData> data_placeholder(
      new OrderedSocketData(NULL, 0, NULL, 0));

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
      BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
  // We require placeholder data because four get requests are sent out, so
  // there needs to be four sets of SSL connection data.
  helper.AddData(data_placeholder.get());
  helper.AddData(data_placeholder.get());
  helper.AddData(data_placeholder.get());
  scoped_ptr<HttpNetworkTransaction> trans1(
      new HttpNetworkTransaction(helper.session()));
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(helper.session()));
  scoped_ptr<HttpNetworkTransaction> trans3(
      new HttpNetworkTransaction(helper.session()));
  scoped_ptr<HttpNetworkTransaction> trans4(
      new HttpNetworkTransaction(helper.session()));

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
  helper.VerifyDataConsumed();
  EXPECT_EQ(OK, out.rv);
}

// Similar to ThreeGetsMaxConcurrrent above, however, this test
// deletes a session in the middle of the transaction to insure
// that we properly remove pendingcreatestream objects from
// the spdy_session
TEST_P(SpdyNetworkTransactionTest, ThreeGetsWithMaxConcurrentDelete) {
  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> fbody(ConstructSpdyBodyFrame(1, true));

  scoped_ptr<spdy::SpdyFrame> req2(ConstructSpdyGet(NULL, 0, false, 3, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<spdy::SpdyFrame> body2(ConstructSpdyBodyFrame(3, false));
  scoped_ptr<spdy::SpdyFrame> fbody2(ConstructSpdyBodyFrame(3, true));

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
    CreateMockRead(*settings_frame, 1),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    CreateMockRead(*fbody),
    CreateMockRead(*resp2, 7),
    CreateMockRead(*body2),
    CreateMockRead(*fbody2),
    MockRead(true, 0, 0),  // EOF
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
        writes, arraysize(writes)));
  scoped_refptr<OrderedSocketData> data_placeholder(
      new OrderedSocketData(NULL, 0, NULL, 0));

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
      BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
  // We require placeholder data because three get requests are sent out, so
  // there needs to be three sets of SSL connection data.
  helper.AddData(data_placeholder.get());
  helper.AddData(data_placeholder.get());
  scoped_ptr<HttpNetworkTransaction> trans1(
      new HttpNetworkTransaction(helper.session()));
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(helper.session()));
  scoped_ptr<HttpNetworkTransaction> trans3(
      new HttpNetworkTransaction(helper.session()));

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
  ASSERT_EQ(OK, out.rv);

  out.rv = trans2->Start(&httpreq2, &callback2, log);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = trans3->Start(&httpreq3, &callback3, log);
  delete trans3.release();
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = callback2.WaitForResult();
  ASSERT_EQ(OK, out.rv);

  EXPECT_EQ(8U, data->read_index());

  const HttpResponseInfo* response1 = trans1->GetResponseInfo();
  ASSERT_TRUE(response1 != NULL);
  EXPECT_TRUE(response1->headers != NULL);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(trans1.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  const HttpResponseInfo* response2 = trans2->GetResponseInfo();
  ASSERT_TRUE(response2 != NULL);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(trans2.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);
  helper.VerifyDataConsumed();
  EXPECT_EQ(OK, out.rv);
}

// The KillerCallback will delete the transaction on error as part of the
// callback.
class KillerCallback : public TestCompletionCallback {
 public:
  explicit KillerCallback(HttpNetworkTransaction* transaction)
      : transaction_(transaction) {}

  virtual void RunWithParams(const Tuple1<int>& params) {
    if (params.a < 0)
      delete transaction_;
    TestCompletionCallback::RunWithParams(params);
  }

 private:
  HttpNetworkTransaction* transaction_;
};

// Similar to ThreeGetsMaxConcurrrentDelete above, however, this test
// closes the socket while we have a pending transaction waiting for
// a pending stream creation.  http://crbug.com/52901
TEST_P(SpdyNetworkTransactionTest, ThreeGetsWithMaxConcurrentSocketClose) {
  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> fin_body(ConstructSpdyBodyFrame(1, true));

  scoped_ptr<spdy::SpdyFrame> req2(ConstructSpdyGet(NULL, 0, false, 3, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 3));

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
    CreateMockRead(*settings_frame, 1),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    CreateMockRead(*fin_body),
    CreateMockRead(*resp2, 7),
    MockRead(true, ERR_CONNECTION_RESET, 0),  // Abort!
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
        writes, arraysize(writes)));
  scoped_refptr<OrderedSocketData> data_placeholder(
      new OrderedSocketData(NULL, 0, NULL, 0));

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
      BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
  // We require placeholder data because three get requests are sent out, so
  // there needs to be three sets of SSL connection data.
  helper.AddData(data_placeholder.get());
  helper.AddData(data_placeholder.get());
  HttpNetworkTransaction trans1(helper.session());
  HttpNetworkTransaction trans2(helper.session());
  HttpNetworkTransaction* trans3(new HttpNetworkTransaction(helper.session()));

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;
  KillerCallback callback3(trans3);

  HttpRequestInfo httpreq1 = CreateGetRequest();
  HttpRequestInfo httpreq2 = CreateGetRequest();
  HttpRequestInfo httpreq3 = CreateGetRequest();

  out.rv = trans1.Start(&httpreq1, &callback1, log);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  // run transaction 1 through quickly to force a read of our SETTINGS
  // frame
  out.rv = callback1.WaitForResult();
  ASSERT_EQ(OK, out.rv);

  out.rv = trans2.Start(&httpreq2, &callback2, log);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = trans3->Start(&httpreq3, &callback3, log);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = callback3.WaitForResult();
  ASSERT_EQ(ERR_ABORTED, out.rv);

  EXPECT_EQ(6U, data->read_index());

  const HttpResponseInfo* response1 = trans1.GetResponseInfo();
  ASSERT_TRUE(response1 != NULL);
  EXPECT_TRUE(response1->headers != NULL);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(&trans1, &out.response_data);
  EXPECT_EQ(OK, out.rv);

  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  ASSERT_TRUE(response2 != NULL);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(&trans2, &out.response_data);
  EXPECT_EQ(ERR_CONNECTION_RESET, out.rv);

  helper.VerifyDataConsumed();
}

// Test that a simple PUT request works.
TEST_P(SpdyNetworkTransactionTest, Put) {
  // Setup the request
  HttpRequestInfo request;
  request.method = "PUT";
  request.url = GURL("http://www.google.com/");

  const SpdyHeaderInfo kSynStartHeader = {
    spdy::SYN_STREAM,             // Kind = Syn
    1,                            // Stream ID
    0,                            // Associated stream ID
    net::ConvertRequestPriorityToSpdyPriority(LOWEST),  // Priority
    spdy::CONTROL_FLAG_FIN,       // Control Flags
    false,                        // Compressed
    spdy::INVALID,                // Status
    NULL,                         // Data
    0,                            // Length
    spdy::DATA_FLAG_NONE          // Data Flags
  };
  const char* const kPutHeaders[] = {
    "method", "PUT",
    "url", "/",
    "host", "www.google.com",
    "scheme", "http",
    "version", "HTTP/1.1",
    "content-length", "0"
  };
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPacket(kSynStartHeader, NULL, 0,
    kPutHeaders, arraysize(kPutHeaders) / 2));
  MockWrite writes[] = {
    CreateMockWrite(*req)
  };

  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  const SpdyHeaderInfo kSynReplyHeader = {
    spdy::SYN_REPLY,              // Kind = SynReply
    1,                            // Stream ID
    0,                            // Associated stream ID
    net::ConvertRequestPriorityToSpdyPriority(LOWEST),  // Priority
    spdy::CONTROL_FLAG_NONE,      // Control Flags
    false,                        // Compressed
    spdy::INVALID,                // Status
    NULL,                         // Data
    0,                            // Length
    spdy::DATA_FLAG_NONE          // Data Flags
  };
  static const char* const kStandardGetHeaders[] = {
    "status", "200",
    "version", "HTTP/1.1"
    "content-length", "1234"
  };
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyPacket(kSynReplyHeader,
      NULL, 0, kStandardGetHeaders, arraysize(kStandardGetHeaders) / 2));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(request,
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
}

// Test that a simple HEAD request works.
TEST_P(SpdyNetworkTransactionTest, Head) {
  // Setup the request
  HttpRequestInfo request;
  request.method = "HEAD";
  request.url = GURL("http://www.google.com/");

  const SpdyHeaderInfo kSynStartHeader = {
    spdy::SYN_STREAM,             // Kind = Syn
    1,                            // Stream ID
    0,                            // Associated stream ID
    net::ConvertRequestPriorityToSpdyPriority(LOWEST),  // Priority
    spdy::CONTROL_FLAG_FIN,       // Control Flags
    false,                        // Compressed
    spdy::INVALID,                // Status
    NULL,                         // Data
    0,                            // Length
    spdy::DATA_FLAG_NONE          // Data Flags
  };
  const char* const kHeadHeaders[] = {
    "method", "HEAD",
    "url", "/",
    "host", "www.google.com",
    "scheme", "http",
    "version", "HTTP/1.1",
    "content-length", "0"
  };
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPacket(kSynStartHeader, NULL, 0,
    kHeadHeaders, arraysize(kHeadHeaders) / 2));
  MockWrite writes[] = {
    CreateMockWrite(*req)
  };

  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  const SpdyHeaderInfo kSynReplyHeader = {
    spdy::SYN_REPLY,              // Kind = SynReply
    1,                            // Stream ID
    0,                            // Associated stream ID
    net::ConvertRequestPriorityToSpdyPriority(LOWEST),  // Priority
    spdy::CONTROL_FLAG_NONE,      // Control Flags
    false,                        // Compressed
    spdy::INVALID,                // Status
    NULL,                         // Data
    0,                            // Length
    spdy::DATA_FLAG_NONE          // Data Flags
  };
  static const char* const kStandardGetHeaders[] = {
    "status", "200",
    "version", "HTTP/1.1"
    "content-length", "1234"
  };
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyPacket(kSynReplyHeader,
      NULL, 0, kStandardGetHeaders, arraysize(kStandardGetHeaders) / 2));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(request,
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
}

// Test that a simple POST works.
TEST_P(SpdyNetworkTransactionTest, Post) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPost(kUploadDataSize, NULL, 0));
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
  NormalSpdyTransactionHelper helper(CreatePostRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a chunked POST works.
TEST_P(SpdyNetworkTransactionTest, ChunkedPost) {
  UploadDataStream::set_merge_chunks(false);
  scoped_ptr<spdy::SpdyFrame> req(ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> chunk1(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> chunk2(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*chunk1),
    CreateMockWrite(*chunk2),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*chunk1),
    CreateMockRead(*chunk2),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateChunkedPostRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);
}

// Test that a POST without any post data works.
TEST_P(SpdyNetworkTransactionTest, NullPost) {
  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  // Create an empty UploadData.
  request.upload_data = NULL;

  // When request.upload_data is NULL for post, content-length is
  // expected to be 0.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPost(0, NULL, 0));
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
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a simple POST works.
TEST_P(SpdyNetworkTransactionTest, EmptyPost) {
  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  // Create an empty UploadData.
  request.upload_data = new UploadData();

  // Http POST Content-Length is using UploadDataStream::size().
  // It is the same as request.upload_data->GetContentLength().
  scoped_ptr<UploadDataStream> stream(UploadDataStream::Create(
      request.upload_data, NULL));
  ASSERT_EQ(request.upload_data->GetContentLength(), stream->size());

  scoped_ptr<spdy::SpdyFrame>
      req(ConstructSpdyPost(request.upload_data->GetContentLength(), NULL, 0));
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
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// While we're doing a post, the server sends back a SYN_REPLY.
TEST_P(SpdyNetworkTransactionTest, PostWithEarlySynReply) {
  static const char upload[] = { "hello!" };

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data = new UploadData();
  request.upload_data->AppendBytes(upload, sizeof(upload));

  // Http POST Content-Length is using UploadDataStream::size().
  // It is the same as request.upload_data->GetContentLength().
  scoped_ptr<UploadDataStream> stream(UploadDataStream::Create(
      request.upload_data, NULL));
  ASSERT_EQ(request.upload_data->GetContentLength(), stream->size());
  scoped_ptr<spdy::SpdyFrame> stream_reply(ConstructSpdyPostSynReply(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> stream_body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*stream_reply, 2),
    CreateMockRead(*stream_body, 3),
    MockRead(false, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(0, reads, arraysize(reads), NULL, 0));
  NormalSpdyTransactionHelper helper(request,
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
  helper.RunDefaultTest();
  helper.VerifyDataConsumed();

  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SYN_REPLY_NOT_RECEIVED, out.rv);
}

// The client upon cancellation tries to send a RST_STREAM frame. The mock
// socket causes the TCP write to return zero. This test checks that the client
// tries to queue up the RST_STREAM frame again.
TEST_P(SpdyNetworkTransactionTest, SocketWriteReturnsZero) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> rst(
      ConstructSpdyRstStream(1, spdy::CANCEL));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 0, false),
    MockWrite(false, 0, 0, 2),
    CreateMockWrite(*rst.get(), 3, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp.get(), 1, true),
    MockRead(true, 0, 0, 4)  // EOF
  };

  scoped_refptr<DeterministicSocketData> data(
      new DeterministicSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.SetDeterministic();
  helper.RunPreTestSetup();
  helper.AddDeterministicData(data.get());
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  data->SetStop(2);
  data->Run();
  helper.ResetTrans();
  data->SetStop(20);
  data->Run();

  helper.VerifyDataConsumed();
}

// Test that the transaction doesn't crash when we don't have a reply.
TEST_P(SpdyNetworkTransactionTest, ResponseWithoutSynReply) {
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads), NULL, 0));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SYN_REPLY_NOT_RECEIVED, out.rv);
}

// Test that the transaction doesn't crash when we get two replies on the same
// stream ID. See http://crbug.com/45639.
TEST_P(SpdyNetworkTransactionTest, ResponseWithTwoSynReplies) {
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
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  std::string response_data;
  rv = ReadTransaction(trans, &response_data);
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, rv);

  helper.VerifyDataConsumed();
}

// Test that sent data frames and received WINDOW_UPDATE frames change
// the send_window_size_ correctly.

// WINDOW_UPDATE is different than most other frames in that it can arrive
// while the client is still sending the request body.  In order to enforce
// this scenario, we feed a couple of dummy frames and give a delay of 0 to
// socket data provider, so that initial read that is done as soon as the
// stream is created, succeeds and schedules another read.  This way reads
// and writes are interleaved; after doing a full frame write, SpdyStream
// will break out of DoLoop and will read and process a WINDOW_UPDATE.
// Once our WINDOW_UPDATE is read, we cannot send SYN_REPLY right away
// since request has not been completely written, therefore we feed
// enough number of WINDOW_UPDATEs to finish the first read and cause a
// write, leading to a complete write of request body; after that we send
// a reply with a body, to cause a graceful shutdown.

// TODO(agayev): develop a socket data provider where both, reads and
// writes are ordered so that writing tests like these are easy and rewrite
// all these tests using it.  Right now we are working around the
// limitations as described above and it's not deterministic, tests may
// fail under specific circumstances.
TEST_P(SpdyNetworkTransactionTest, WindowUpdateReceived) {
  SpdySession::set_flow_control(true);

  static int kFrameCount = 2;
  scoped_ptr<std::string> content(
      new std::string(kMaxSpdyFrameChunkSize, 'a'));
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPost(
      kMaxSpdyFrameChunkSize * kFrameCount, NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(
      ConstructSpdyBodyFrame(1, content->c_str(), content->size(), false));
  scoped_ptr<spdy::SpdyFrame> body_end(
      ConstructSpdyBodyFrame(1, content->c_str(), content->size(), true));

  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*body),
    CreateMockWrite(*body_end),
  };

  static const int kDeltaWindowSize = 0xff;
  static const int kDeltaCount = 4;
  scoped_ptr<spdy::SpdyFrame> window_update(
      ConstructSpdyWindowUpdate(1, kDeltaWindowSize));
  scoped_ptr<spdy::SpdyFrame> window_update_dummy(
      ConstructSpdyWindowUpdate(2, kDeltaWindowSize));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*window_update_dummy),
    CreateMockRead(*window_update_dummy),
    CreateMockRead(*window_update_dummy),
    CreateMockRead(*window_update),     // Four updates, therefore window
    CreateMockRead(*window_update),     // size should increase by
    CreateMockRead(*window_update),     // kDeltaWindowSize * 4
    CreateMockRead(*window_update),
    CreateMockRead(*resp),
    CreateMockRead(*body_end),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(0, reads, arraysize(reads),
                            writes, arraysize(writes)));

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL(kDefaultURL);
  request.upload_data = new UploadData();
  for (int i = 0; i < kFrameCount; ++i)
    request.upload_data->AppendBytes(content->c_str(), content->size());

  NormalSpdyTransactionHelper helper(request, BoundNetLog(), GetParam());
  helper.AddData(data.get());
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), &callback, BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  SpdyHttpStream* stream = static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream != NULL);
  ASSERT_TRUE(stream->stream() != NULL);
  EXPECT_EQ(static_cast<int>(spdy::kSpdyStreamInitialWindowSize) +
            kDeltaWindowSize * kDeltaCount -
            kMaxSpdyFrameChunkSize * kFrameCount,
            stream->stream()->send_window_size());
  helper.VerifyDataConsumed();
  SpdySession::set_flow_control(false);
}

// Test that received data frames and sent WINDOW_UPDATE frames change
// the recv_window_size_ correctly.
TEST_P(SpdyNetworkTransactionTest, WindowUpdateSent) {
  SpdySession::set_flow_control(true);

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> window_update(
      ConstructSpdyWindowUpdate(1, kUploadDataSize));

  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*window_update),
  };

  scoped_ptr<spdy::SpdyFrame> resp(
      ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body_no_fin(
      ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> body_fin(
      ConstructSpdyBodyFrame(1, NULL, 0, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body_no_fin),
    MockRead(true, ERR_IO_PENDING, 0),  // Force a pause
    CreateMockRead(*body_fin),
    MockRead(true, ERR_IO_PENDING, 0),  // Force a pause
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.AddData(data.get());
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), &callback, BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  SpdyHttpStream* stream =
      static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream != NULL);
  ASSERT_TRUE(stream->stream() != NULL);

  EXPECT_EQ(
      static_cast<int>(spdy::kSpdyStreamInitialWindowSize) - kUploadDataSize,
      stream->stream()->recv_window_size());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);

  // Issue a read which will cause a WINDOW_UPDATE to be sent and window
  // size increased to default.
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kUploadDataSize));
  rv = trans->Read(buf, kUploadDataSize, NULL);
  EXPECT_EQ(kUploadDataSize, rv);
  std::string content(buf->data(), buf->data()+kUploadDataSize);
  EXPECT_STREQ(kUploadData, content.c_str());

  // Schedule the reading of empty data frame with FIN
  data->CompleteRead();

  // Force write of WINDOW_UPDATE which was scheduled during the above
  // read.
  MessageLoop::current()->RunAllPending();

  // Read EOF.
  data->CompleteRead();

  helper.VerifyDataConsumed();
  SpdySession::set_flow_control(false);
}

// Test that WINDOW_UPDATE frame causing overflow is handled correctly.  We
// use the same trick as in the above test to enforce our scenario.
TEST_P(SpdyNetworkTransactionTest, WindowUpdateOverflow) {
  SpdySession::set_flow_control(true);

  // number of full frames we hope to write (but will not, used to
  // set content-length header correctly)
  static int kFrameCount = 3;

  scoped_ptr<std::string> content(
      new std::string(kMaxSpdyFrameChunkSize, 'a'));
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPost(
      kMaxSpdyFrameChunkSize * kFrameCount, NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(
      ConstructSpdyBodyFrame(1, content->c_str(), content->size(), false));
  scoped_ptr<spdy::SpdyFrame> rst(
      ConstructSpdyRstStream(1, spdy::FLOW_CONTROL_ERROR));

  // We're not going to write a data frame with FIN, we'll receive a bad
  // WINDOW_UPDATE while sending a request and will send a RST_STREAM frame.
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*body),
    CreateMockWrite(*rst),
  };

  static const int kDeltaWindowSize = 0x7fffffff;  // cause an overflow
  scoped_ptr<spdy::SpdyFrame> window_update(
      ConstructSpdyWindowUpdate(1, kDeltaWindowSize));
  scoped_ptr<spdy::SpdyFrame> window_update2(
      ConstructSpdyWindowUpdate(2, kDeltaWindowSize));
  scoped_ptr<spdy::SpdyFrame> reply(ConstructSpdyPostSynReply(NULL, 0));

  MockRead reads[] = {
    CreateMockRead(*window_update2),
    CreateMockRead(*window_update2),
    CreateMockRead(*window_update),
    CreateMockRead(*window_update),
    CreateMockRead(*window_update),
    MockRead(true, ERR_IO_PENDING, 0),  // Wait for the RST to be written.
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(0, reads, arraysize(reads),
                            writes, arraysize(writes)));

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data = new UploadData();
  for (int i = 0; i < kFrameCount; ++i)
    request.upload_data->AppendBytes(content->c_str(), content->size());

  NormalSpdyTransactionHelper helper(request,
                                     BoundNetLog(), GetParam());
  helper.AddData(data.get());
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), &callback, BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, rv);

  data->CompleteRead();

  ASSERT_TRUE(helper.session() != NULL);
  ASSERT_TRUE(helper.session()->spdy_session_pool() != NULL);
  helper.session()->spdy_session_pool()->CloseAllSessions();
  helper.VerifyDataConsumed();

  SpdySession::set_flow_control(false);
}

// Test that after hitting a send window size of 0, the write process
// stalls and upon receiving WINDOW_UPDATE frame write resumes.

// This test constructs a POST request followed by enough data frames
// containing 'a' that would make the window size 0, followed by another
// data frame containing default content (which is "hello!") and this frame
// also contains a FIN flag.  DelayedSocketData is used to enforce all
// writes go through before a read could happen.  However, the last frame
// ("hello!")  is not supposed to go through since by the time its turn
// arrives, window size is 0.  At this point MessageLoop::Run() called via
// callback would block.  Therefore we call MessageLoop::RunAllPending()
// which returns after performing all possible writes.  We use DCHECKS to
// ensure that last data frame is still there and stream has stalled.
// After that, next read is artifically enforced, which causes a
// WINDOW_UPDATE to be read and I/O process resumes.
TEST_P(SpdyNetworkTransactionTest, FlowControlStallResume) {
  SpdySession::set_flow_control(true);

  // Number of frames we need to send to zero out the window size: data
  // frames plus SYN_STREAM plus the last data frame; also we need another
  // data frame that we will send once the WINDOW_UPDATE is received,
  // therefore +3.
  size_t nwrites =
      spdy::kSpdyStreamInitialWindowSize / kMaxSpdyFrameChunkSize + 3;

  // Calculate last frame's size; 0 size data frame is legal.
  size_t last_frame_size =
      spdy::kSpdyStreamInitialWindowSize % kMaxSpdyFrameChunkSize;

  // Construct content for a data frame of maximum size.
  scoped_ptr<std::string> content(
      new std::string(kMaxSpdyFrameChunkSize, 'a'));

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPost(
      spdy::kSpdyStreamInitialWindowSize + kUploadDataSize, NULL, 0));

  // Full frames.
  scoped_ptr<spdy::SpdyFrame> body1(
      ConstructSpdyBodyFrame(1, content->c_str(), content->size(), false));

  // Last frame to zero out the window size.
  scoped_ptr<spdy::SpdyFrame> body2(
      ConstructSpdyBodyFrame(1, content->c_str(), last_frame_size, false));

  // Data frame to be sent once WINDOW_UPDATE frame is received.
  scoped_ptr<spdy::SpdyFrame> body3(ConstructSpdyBodyFrame(1, true));

  // Fill in mock writes.
  scoped_array<MockWrite> writes(new MockWrite[nwrites]);
  size_t i = 0;
  writes[i] = CreateMockWrite(*req);
  for (i = 1; i < nwrites-2; i++)
    writes[i] = CreateMockWrite(*body1);
  writes[i++] = CreateMockWrite(*body2);
  writes[i] = CreateMockWrite(*body3);

  // Construct read frame, give enough space to upload the rest of the
  // data.
  scoped_ptr<spdy::SpdyFrame> window_update(
      ConstructSpdyWindowUpdate(1, kUploadDataSize));
  scoped_ptr<spdy::SpdyFrame> reply(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*window_update),
    CreateMockRead(*window_update),
    CreateMockRead(*reply),
    CreateMockRead(*body2),
    CreateMockRead(*body3),
    MockRead(true, 0, 0)  // EOF
  };

  // Force all writes to happen before any read, last write will not
  // actually queue a frame, due to window size being 0.
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(nwrites, reads, arraysize(reads),
                            writes.get(), nwrites));

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data = new UploadData();
  scoped_ptr<std::string> upload_data(
      new std::string(spdy::kSpdyStreamInitialWindowSize, 'a'));
  upload_data->append(kUploadData, kUploadDataSize);
  request.upload_data->AppendBytes(upload_data->c_str(), upload_data->size());
  NormalSpdyTransactionHelper helper(request,
                                     BoundNetLog(), GetParam());
  helper.AddData(data.get());
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  MessageLoop::current()->RunAllPending(); // Write as much as we can.

  SpdyHttpStream* stream = static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream != NULL);
  ASSERT_TRUE(stream->stream() != NULL);
  EXPECT_EQ(0, stream->stream()->send_window_size());
  EXPECT_FALSE(stream->request_body_stream_->eof());

  data->ForceNextRead();   // Read in WINDOW_UPDATE frame.
  rv = callback.WaitForResult();
  helper.VerifyDataConsumed();

  SpdySession::set_flow_control(false);
}

TEST_P(SpdyNetworkTransactionTest, CancelledTransaction) {
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
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(&data);
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

// Verify that the client sends a Rst Frame upon cancelling the stream.
TEST_P(SpdyNetworkTransactionTest, CancelledTransactionSendRst) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> rst(
      ConstructSpdyRstStream(1, spdy::CANCEL));
  MockWrite writes[] = {
    CreateMockWrite(*req, 0, false),
    CreateMockWrite(*rst, 2, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 0, 3)  // EOF
  };

  scoped_refptr<DeterministicSocketData> data(
      new DeterministicSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(),
                                     GetParam());
  helper.SetDeterministic();
  helper.RunPreTestSetup();
  helper.AddDeterministicData(data.get());
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;

  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  data->SetStop(2);
  data->Run();
  helper.ResetTrans();
  data->SetStop(20);
  data->Run();

  helper.VerifyDataConsumed();
}

class SpdyNetworkTransactionTest::StartTransactionCallback
    : public CallbackRunner< Tuple1<int> > {
 public:
  explicit StartTransactionCallback(
      const scoped_refptr<HttpNetworkSession>& session,
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
  const scoped_refptr<HttpNetworkSession>& session_;
  NormalSpdyTransactionHelper& helper_;
};

// Verify that the client can correctly deal with the user callback attempting
// to start another transaction on a session that is closing down. See
// http://crbug.com/47455
TEST_P(SpdyNetworkTransactionTest, StartTransactionOnReadCallback) {
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
      new DelayedSocketData(1, reads2, arraysize(reads2),
                            writes2, arraysize(writes2)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
  helper.AddData(data2.get());
  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();

  StartTransactionCallback callback2(helper.session(), helper);
  const int kSize = 3000;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
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
TEST_P(SpdyNetworkTransactionTest, DeleteSessionOnReadCallback) {
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
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
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
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
  rv = trans->Read(buf, kSize, &callback2);
  ASSERT_EQ(ERR_IO_PENDING, rv);
  data->CompleteRead();

  // Finish running rest of tasks.
  MessageLoop::current()->RunAllPending();
  helper.VerifyDataConsumed();
}

// Send a spdy request to www.google.com that gets redirected to www.foo.com.
TEST_P(SpdyNetworkTransactionTest, RedirectGetRequest) {
  // These are headers which the net::URLRequest tacks on.
  const char* const kExtraHeaders[] = {
    "accept-charset",
    "",
    "accept-encoding",
    "gzip,deflate",
    "accept-language",
    "",
  };
  const SpdyHeaderInfo kSynStartHeader = make_spdy_header(spdy::SYN_STREAM);
  const char* const kStandardGetHeaders[] = {
    "host",
    "www.google.com",
    "method",
    "GET",
    "scheme",
    "http",
    "url",
    "/",
    "user-agent",
    "",
    "version",
    "HTTP/1.1"
  };
  const char* const kStandardGetHeaders2[] = {
    "host",
    "www.foo.com",
    "method",
    "GET",
    "scheme",
    "http",
    "url",
    "/index.php",
    "user-agent",
    "",
    "version",
    "HTTP/1.1"
  };

  // Setup writes/reads to www.google.com
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyPacket(
      kSynStartHeader, kExtraHeaders, arraysize(kExtraHeaders) / 2,
      kStandardGetHeaders, arraysize(kStandardGetHeaders) / 2));
  scoped_ptr<spdy::SpdyFrame> req2(ConstructSpdyPacket(
      kSynStartHeader, kExtraHeaders, arraysize(kExtraHeaders) / 2,
      kStandardGetHeaders2, arraysize(kStandardGetHeaders2) / 2));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReplyRedirect(1));
  MockWrite writes[] = {
    CreateMockWrite(*req, 1),
  };
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(true, 0, 0, 3)  // EOF
  };

  // Setup writes/reads to www.foo.com
  scoped_ptr<spdy::SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body2(ConstructSpdyBodyFrame(1, true));
  MockWrite writes2[] = {
    CreateMockWrite(*req2, 1),
  };
  MockRead reads2[] = {
    CreateMockRead(*resp2, 2),
    CreateMockRead(*body2, 3),
    MockRead(true, 0, 0, 4)  // EOF
  };
  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  scoped_refptr<OrderedSocketData> data2(
      new OrderedSocketData(reads2, arraysize(reads2),
                            writes2, arraysize(writes2)));

  // TODO(erikchen): Make test support SPDYSSL, SPDYNPN
  HttpStreamFactory::set_force_spdy_over_ssl(false);
  HttpStreamFactory::set_force_spdy_always(true);
  TestDelegate d;
  {
    net::URLRequest r(GURL("http://www.google.com/"), &d);
    SpdyURLRequestContext* spdy_url_request_context =
        new SpdyURLRequestContext();
    r.set_context(spdy_url_request_context);
    spdy_url_request_context->socket_factory().
        AddSocketDataProvider(data.get());
    spdy_url_request_context->socket_factory().
        AddSocketDataProvider(data2.get());

    d.set_quit_on_redirect(true);
    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.received_redirect_count());

    r.FollowDeferredRedirect();
    MessageLoop::current()->Run();
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, r.status().status());
    std::string contents("hello!");
    EXPECT_EQ(contents, d.data_received());
  }
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());
  EXPECT_TRUE(data2->at_read_eof());
  EXPECT_TRUE(data2->at_write_eof());
}

// Send a spdy request to www.google.com. Get a pushed stream that redirects to
// www.foo.com.
TEST_P(SpdyNetworkTransactionTest, RedirectServerPush) {
  // These are headers which the net::URLRequest tacks on.
  const char* const kExtraHeaders[] = {
    "accept-charset",
    "",
    "accept-encoding",
    "gzip,deflate",
    "accept-language",
    "",
  };
  const SpdyHeaderInfo kSynStartHeader = make_spdy_header(spdy::SYN_STREAM);
  const char* const kStandardGetHeaders[] = {
    "host",
    "www.google.com",
    "method",
    "GET",
    "scheme",
    "http",
    "url",
    "/",
    "user-agent",
    "",
    "version",
    "HTTP/1.1"
  };
  const char* const kStandardGetHeaders2[] = {
    "host",
    "www.foo.com",
    "method",
    "GET",
    "scheme",
    "http",
    "url",
    "/index.php",
    "user-agent",
    "",
    "version",
    "HTTP/1.1"
  };

  // Setup writes/reads to www.google.com
  scoped_ptr<spdy::SpdyFrame> req(
      ConstructSpdyPacket(kSynStartHeader,
                          kExtraHeaders,
                          arraysize(kExtraHeaders) / 2,
                          kStandardGetHeaders,
                          arraysize(kStandardGetHeaders) / 2));
  scoped_ptr<spdy::SpdyFrame> req2(
      ConstructSpdyPacket(kSynStartHeader,
                          kExtraHeaders,
                          arraysize(kExtraHeaders) / 2,
                          kStandardGetHeaders2,
                          arraysize(kStandardGetHeaders2) / 2));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> rep(
      ConstructSpdyPush(NULL,
                        0,
                        2,
                        1,
                        "http://www.google.com/foo.dat",
                        "301 Moved Permanently",
                        "http://www.foo.com/index.php"));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req, 1),
  };
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    CreateMockRead(*rep, 3),
    CreateMockRead(*body, 4),
    MockRead(true, ERR_IO_PENDING, 5),  // Force a pause
    MockRead(true, 0, 0, 6)  // EOF
  };

  // Setup writes/reads to www.foo.com
  scoped_ptr<spdy::SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body2(ConstructSpdyBodyFrame(1, true));
  MockWrite writes2[] = {
    CreateMockWrite(*req2, 1),
  };
  MockRead reads2[] = {
    CreateMockRead(*resp2, 2),
    CreateMockRead(*body2, 3),
    MockRead(true, 0, 0, 5)  // EOF
  };
  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  scoped_refptr<OrderedSocketData> data2(
      new OrderedSocketData(reads2, arraysize(reads2),
                            writes2, arraysize(writes2)));

  // TODO(erikchen): Make test support SPDYSSL, SPDYNPN
  HttpStreamFactory::set_force_spdy_over_ssl(false);
  HttpStreamFactory::set_force_spdy_always(true);
  TestDelegate d;
  TestDelegate d2;
  scoped_refptr<SpdyURLRequestContext> spdy_url_request_context(
      new SpdyURLRequestContext());
  {
    net::URLRequest r(GURL("http://www.google.com/"), &d);
    r.set_context(spdy_url_request_context);
    spdy_url_request_context->socket_factory().
        AddSocketDataProvider(data.get());

    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(0, d.received_redirect_count());
    std::string contents("hello!");
    EXPECT_EQ(contents, d.data_received());

    net::URLRequest r2(GURL("http://www.google.com/foo.dat"), &d2);
    r2.set_context(spdy_url_request_context);
    spdy_url_request_context->socket_factory().
        AddSocketDataProvider(data2.get());

    d2.set_quit_on_redirect(true);
    r2.Start();
    MessageLoop::current()->Run();
    EXPECT_EQ(1, d2.received_redirect_count());

    r2.FollowDeferredRedirect();
    MessageLoop::current()->Run();
    EXPECT_EQ(1, d2.response_started_count());
    EXPECT_FALSE(d2.received_data_before_response());
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, r2.status().status());
    std::string contents2("hello!");
    EXPECT_EQ(contents2, d2.data_received());
  }
  data->CompleteRead();
  data2->CompleteRead();
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());
  EXPECT_TRUE(data2->at_read_eof());
  EXPECT_TRUE(data2->at_write_eof());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushSingleDataFrame) {
  static const unsigned char kPushBodyFrame[] = {
    0x00, 0x00, 0x00, 0x02,                                      // header, ID
    0x01, 0x00, 0x00, 0x06,                                      // FIN, length
    'p', 'u', 's', 'h', 'e', 'd'                                 // "pushed"
  };
  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_syn(ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream1_body, 4, false),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame),
             arraysize(kPushBodyFrame), 5),
    MockRead(true, ERR_IO_PENDING, 6),  // Force a pause
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  scoped_refptr<OrderedSocketData> data(new OrderedSocketData(
      reads,
      arraysize(reads),
      writes,
      arraysize(writes)));
  RunServerPushTest(data.get(),
                    &response,
                    &response2,
                    expected_push_result);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushSingleDataFrame2) {
  static const unsigned char kPushBodyFrame[] = {
    0x00, 0x00, 0x00, 0x02,                                      // header, ID
    0x01, 0x00, 0x00, 0x06,                                      // FIN, length
    'p', 'u', 's', 'h', 'e', 'd'                                 // "pushed"
  };
  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_syn(ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame),
             arraysize(kPushBodyFrame), 5),
    CreateMockRead(*stream1_body, 4, false),
    MockRead(true, ERR_IO_PENDING, 6),  // Force a pause
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  scoped_refptr<OrderedSocketData> data(new OrderedSocketData(
      reads,
      arraysize(reads),
      writes,
      arraysize(writes)));
  RunServerPushTest(data.get(),
                    &response,
                    &response2,
                    expected_push_result);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushServerAborted) {
  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_syn(ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  scoped_ptr<spdy::SpdyFrame>
    stream2_rst(ConstructSpdyRstStream(2, spdy::PROTOCOL_ERROR));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream2_rst, 4),
    CreateMockRead(*stream1_body, 5, false),
    MockRead(true, ERR_IO_PENDING, 6),  // Force a pause
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());

  helper.RunPreTestSetup();
  helper.AddData(data.get());

  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof()) << "Read count: "
                                   << data->read_count()
                                   << " Read index: "
                                   << data->read_index();
  EXPECT_TRUE(data->at_write_eof()) << "Write count: "
                                    << data->write_count()
                                    << " Write index: "
                                    << data->write_index();

  // Verify the SYN_REPLY.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushDuplicate) {
  // Verify that we don't leak streams and that we properly send a reset
  // if the server pushes the same stream twice.
  static const unsigned char kPushBodyFrame[] = {
    0x00, 0x00, 0x00, 0x02,                                      // header, ID
    0x01, 0x00, 0x00, 0x06,                                      // FIN, length
    'p', 'u', 's', 'h', 'e', 'd'                                 // "pushed"
  };

  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  scoped_ptr<spdy::SpdyFrame>
      stream3_rst(ConstructSpdyRstStream(4, spdy::PROTOCOL_ERROR));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
    CreateMockWrite(*stream3_rst, 5),
  };

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_syn(ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  scoped_ptr<spdy::SpdyFrame>
      stream3_syn(ConstructSpdyPush(NULL,
                                    0,
                                    4,
                                    1,
                                    "http://www.google.com/foo.dat"));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream3_syn, 4),
    CreateMockRead(*stream1_body, 6, false),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame),
             arraysize(kPushBodyFrame), 7),
    MockRead(true, ERR_IO_PENDING, 8),  // Force a pause
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  scoped_refptr<OrderedSocketData> data(new OrderedSocketData(
      reads,
      arraysize(reads),
      writes,
      arraysize(writes)));
  RunServerPushTest(data.get(),
                    &response,
                    &response2,
                    expected_push_result);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushMultipleDataFrame) {
  static const unsigned char kPushBodyFrame1[] = {
    0x00, 0x00, 0x00, 0x02,                                      // header, ID
    0x01, 0x00, 0x00, 0x1F,                                      // FIN, length
    'p', 'u', 's', 'h', 'e', 'd'                                 // "pushed"
  };
  static const char kPushBodyFrame2[] = " my darling";
  static const char kPushBodyFrame3[] = " hello";
  static const char kPushBodyFrame4[] = " my baby";

  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_syn(ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame1),
             arraysize(kPushBodyFrame1), 4),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame2),
             arraysize(kPushBodyFrame2) - 1, 5),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame3),
             arraysize(kPushBodyFrame3) - 1, 6),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame4),
             arraysize(kPushBodyFrame4) - 1, 7),
    CreateMockRead(*stream1_body, 8, false),
    MockRead(true, ERR_IO_PENDING, 9),  // Force a pause
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed my darling hello my baby");
  scoped_refptr<OrderedSocketData> data(new OrderedSocketData(
      reads,
      arraysize(reads),
      writes,
      arraysize(writes)));
  RunServerPushTest(data.get(),
                    &response,
                    &response2,
                    expected_push_result);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushMultipleDataFrameInterrupted) {
  static const unsigned char kPushBodyFrame1[] = {
    0x00, 0x00, 0x00, 0x02,                                      // header, ID
    0x01, 0x00, 0x00, 0x1F,                                      // FIN, length
    'p', 'u', 's', 'h', 'e', 'd'                                 // "pushed"
  };
  static const char kPushBodyFrame2[] = " my darling";
  static const char kPushBodyFrame3[] = " hello";
  static const char kPushBodyFrame4[] = " my baby";

  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_syn(ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame1),
             arraysize(kPushBodyFrame1), 4),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame2),
             arraysize(kPushBodyFrame2) - 1, 5),
    MockRead(true, ERR_IO_PENDING, 6),  // Force a pause
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame3),
             arraysize(kPushBodyFrame3) - 1, 7),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame4),
             arraysize(kPushBodyFrame4) - 1, 8),
    CreateMockRead(*stream1_body.get(), 9, false),
    MockRead(true, ERR_IO_PENDING, 10)  // Force a pause.
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed my darling hello my baby");
  scoped_refptr<OrderedSocketData> data(new OrderedSocketData(
      reads,
      arraysize(reads),
      writes,
      arraysize(writes)));
  RunServerPushTest(data.get(),
                    &response,
                    &response2,
                    expected_push_result);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushInvalidAssociatedStreamID0) {
  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  scoped_ptr<spdy::SpdyFrame>
      stream2_rst(ConstructSpdyRstStream(2, spdy::INVALID_STREAM));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
    CreateMockWrite(*stream2_rst, 4),
  };

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_syn(ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    0,
                                    "http://www.google.com/foo.dat"));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream1_body, 4),
    MockRead(true, ERR_IO_PENDING, 5)  // Force a pause
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());

  helper.RunPreTestSetup();
  helper.AddData(data.get());

  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof()) << "Read count: "
                                   << data->read_count()
                                   << " Read index: "
                                   << data->read_index();
  EXPECT_TRUE(data->at_write_eof()) << "Write count: "
                                    << data->write_count()
                                    << " Write index: "
                                    << data->write_index();

  // Verify the SYN_REPLY.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushInvalidAssociatedStreamID9) {
  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  scoped_ptr<spdy::SpdyFrame>
      stream2_rst(ConstructSpdyRstStream(2, spdy::INVALID_ASSOCIATED_STREAM));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
    CreateMockWrite(*stream2_rst, 4),
  };

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_syn(ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    9,
                                    "http://www.google.com/foo.dat"));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream1_body, 4),
    MockRead(true, ERR_IO_PENDING, 5),  // Force a pause
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());

  helper.RunPreTestSetup();
  helper.AddData(data.get());

  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof()) << "Read count: "
                                   << data->read_count()
                                   << " Read index: "
                                   << data->read_index();
  EXPECT_TRUE(data->at_write_eof()) << "Write count: "
                                    << data->write_count()
                                    << " Write index: "
                                    << data->write_index();

  // Verify the SYN_REPLY.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushNoURL) {
  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  scoped_ptr<spdy::SpdyFrame>
      stream2_rst(ConstructSpdyRstStream(2, spdy::PROTOCOL_ERROR));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
    CreateMockWrite(*stream2_rst, 4),
  };

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_syn(ConstructSpdyPush(NULL, 0, 2, 1));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream1_body, 4),
    MockRead(true, ERR_IO_PENDING, 5)  // Force a pause
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());

  helper.RunPreTestSetup();
  helper.AddData(data.get());

  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof()) << "Read count: "
                                   << data->read_count()
                                   << " Read index: "
                                   << data->read_index();
  EXPECT_TRUE(data->at_write_eof()) << "Write count: "
                                    << data->write_count()
                                    << " Write index: "
                                    << data->write_index();

  // Verify the SYN_REPLY.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());
}

// Verify that various SynReply headers parse correctly through the
// HTTP layer.
TEST_P(SpdyNetworkTransactionTest, SynReplyHeaders) {
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
      "version: HTTP/1.1\n"
    },
    // This is the minimalist set of headers.
    { 0,
      { NULL },
      "hello: bye\n"
      "status: 200\n"
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
                                       BoundNetLog(), GetParam());
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
TEST_P(SpdyNetworkTransactionTest, SynReplyHeadersVary) {
  static const SpdyHeaderInfo syn_reply_info = {
    spdy::SYN_REPLY,                              // Syn Reply
    1,                                            // Stream ID
    0,                                            // Associated Stream ID
    net::ConvertRequestPriorityToSpdyPriority(LOWEST),
                                                  // Priority
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
                                       BoundNetLog(), GetParam());
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
TEST_P(SpdyNetworkTransactionTest, InvalidSynReply) {
  const SpdyHeaderInfo kSynStartHeader = {
    spdy::SYN_REPLY,              // Kind = SynReply
    1,                            // Stream ID
    0,                            // Associated stream ID
    net::ConvertRequestPriorityToSpdyPriority(LOWEST),
                                  // Priority
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
                                       BoundNetLog(), GetParam());
    helper.RunToCompletion(data.get());
    TransactionHelperResult out = helper.output();
    EXPECT_EQ(ERR_INCOMPLETE_SPDY_HEADERS, out.rv);
  }
}

// Verify that we don't crash on some corrupt frames.
TEST_P(SpdyNetworkTransactionTest, CorruptFrameSessionError) {
  // This is the length field that's too short.
  scoped_ptr<spdy::SpdyFrame> syn_reply_wrong_length(
      ConstructSpdyGetSynReply(NULL, 0, 1));
  syn_reply_wrong_length->set_length(syn_reply_wrong_length->length() - 4);

  struct SynReplyTests {
    const spdy::SpdyFrame* syn_reply;
  } test_cases[] = {
    { syn_reply_wrong_length.get(), },
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
                                       BoundNetLog(), GetParam());
    helper.RunToCompletion(data.get());
    TransactionHelperResult out = helper.output();
    EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
  }
}

// Test that we shutdown correctly on write errors.
TEST_P(SpdyNetworkTransactionTest, WriteError) {
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
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_FAILED, out.rv);
  data->Reset();
}

// Test that partial writes work.
TEST_P(SpdyNetworkTransactionTest, PartialWrite) {
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
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// In this test, we enable compression, but get a uncompressed SynReply from
// the server.  Verify that teardown is all clean.
TEST_P(SpdyNetworkTransactionTest, DecompressFailureOnSynReply) {
  // For this test, we turn on the normal compression.
  EnableCompression(true);

  scoped_ptr<spdy::SpdyFrame> compressed(
      ConstructSpdyGet(NULL, 0, true, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> rst(
      ConstructSpdyRstStream(1, spdy::PROTOCOL_ERROR));
  MockWrite writes[] = {
    CreateMockWrite(*compressed),
    CreateMockWrite(*rst),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
  data->Reset();

  EnableCompression(false);
}

// Test that the NetLog contains good data for a simple GET request.
TEST_P(SpdyNetworkTransactionTest, NetLog) {
  static const char* const kExtraHeaders[] = {
    "user-agent",   "Chrome",
  };
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(kExtraHeaders, 1, false, 1,
                                                   LOWEST));
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
  NormalSpdyTransactionHelper helper(CreateGetRequestWithUserAgent(),
                                     log.bound(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // Check that the NetLog was filled reasonably.
  // This test is intentionally non-specific about the exact ordering of the
  // log; instead we just check to make sure that certain events exist, and that
  // they are in the right order.
  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);

  EXPECT_LT(0u, entries.size());
  int pos = 0;
  pos = net::ExpectLogContainsSomewhere(entries, 0,
      net::NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(entries, pos + 1,
      net::NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST,
      net::NetLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(entries, pos + 1,
      net::NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(entries, pos + 1,
      net::NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS,
      net::NetLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(entries, pos + 1,
      net::NetLog::TYPE_HTTP_TRANSACTION_READ_BODY,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(entries, pos + 1,
      net::NetLog::TYPE_HTTP_TRANSACTION_READ_BODY,
      net::NetLog::PHASE_END);

  // Check that we logged all the headers correctly
  pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_SPDY_SESSION_SYN_STREAM,
      net::NetLog::PHASE_NONE);
  CapturingNetLog::Entry entry = entries[pos];
  NetLogSpdySynParameter* request_params =
      static_cast<NetLogSpdySynParameter*>(entry.extra_parameters.get());
  spdy::SpdyHeaderBlock* headers =
      request_params->GetHeaders().get();

  spdy::SpdyHeaderBlock expected;
  expected["host"] = "www.google.com";
  expected["url"] = "/";
  expected["scheme"] = "http";
  expected["version"] = "HTTP/1.1";
  expected["method"] = "GET";
  expected["user-agent"] = "Chrome";
  EXPECT_EQ(expected.size(), headers->size());
  spdy::SpdyHeaderBlock::const_iterator end = expected.end();
  for (spdy::SpdyHeaderBlock::const_iterator it = expected.begin();
      it != end;
      ++it) {
    EXPECT_EQ(it->second, (*headers)[it->first]);
  }
}

// Since we buffer the IO from the stream to the renderer, this test verifies
// that when we read out the maximum amount of data (e.g. we received 50 bytes
// on the network, but issued a Read for only 5 of those bytes) that the data
// flow still works correctly.
TEST_P(SpdyNetworkTransactionTest, BufferFull) {
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
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
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
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSmallReadSize));
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

TEST_P(SpdyNetworkTransactionTest, ConnectFailureFallbackToHttp) {
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
                                       BoundNetLog(), GetParam());
    helper.RunPreTestSetup();
    helper.AddData(data.get());

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
    if (GetParam() == SPDYNOSSL || GetParam() == SPDYSSL) {
      ASSERT_TRUE(response == NULL);
      return;
    }
    if (GetParam() != SPDYNPN)
      NOTREACHED();
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
TEST_P(SpdyNetworkTransactionTest, Buffering) {
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
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
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
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSmallReadSize));
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
TEST_P(SpdyNetworkTransactionTest, BufferedAll) {
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
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
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
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSmallReadSize));
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
TEST_P(SpdyNetworkTransactionTest, BufferedClosed) {
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
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
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
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSmallReadSize));
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
TEST_P(SpdyNetworkTransactionTest, BufferedCancelled) {
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
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
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
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kReadSize));
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
TEST_P(SpdyNetworkTransactionTest, SettingsSaved) {
  static const SpdyHeaderInfo kSynReplyInfo = {
    spdy::SYN_REPLY,                              // Syn Reply
    1,                                            // Stream ID
    0,                                            // Associated Stream ID
    net::ConvertRequestPriorityToSpdyPriority(LOWEST),
                                                  // Priority
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
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();

  // Verify that no settings exist initially.
  HostPortPair host_port_pair("www.google.com", helper.port());
  SpdySessionPool* spdy_session_pool = helper.session()->spdy_session_pool();
  EXPECT_TRUE(spdy_session_pool->spdy_settings().Get(host_port_pair).empty());

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
  helper.AddData(data.get());
  helper.RunDefaultTest();
  helper.VerifyDataConsumed();
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  {
    // Verify we had two persisted settings.
    spdy::SpdySettings saved_settings =
        spdy_session_pool->spdy_settings().Get(host_port_pair);
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
TEST_P(SpdyNetworkTransactionTest, SettingsPlayback) {
  static const SpdyHeaderInfo kSynReplyInfo = {
    spdy::SYN_REPLY,                              // Syn Reply
    1,                                            // Stream ID
    0,                                            // Associated Stream ID
    net::ConvertRequestPriorityToSpdyPriority(LOWEST),
                                                  // Priority
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
                                     BoundNetLog(), GetParam());
  helper.RunPreTestSetup();

  // Verify that no settings exist initially.
  HostPortPair host_port_pair("www.google.com", helper.port());
  SpdySessionPool* spdy_session_pool = helper.session()->spdy_session_pool();
  EXPECT_TRUE(spdy_session_pool->spdy_settings().Get(host_port_pair).empty());

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

    spdy_session_pool->mutable_spdy_settings()->Set(host_port_pair, settings);
  }

  EXPECT_EQ(2u, spdy_session_pool->spdy_settings().Get(host_port_pair).size());

  // Construct the SETTINGS frame.
  const spdy::SpdySettings& settings =
      spdy_session_pool->spdy_settings().Get(host_port_pair);
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
  helper.AddData(data.get());
  helper.RunDefaultTest();
  helper.VerifyDataConsumed();
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  {
    // Verify we had two persisted settings.
    spdy::SpdySettings saved_settings =
        spdy_session_pool->spdy_settings().Get(host_port_pair);
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

TEST_P(SpdyNetworkTransactionTest, GoAwayWithActiveStream) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> go_away(ConstructSpdyGoAway());
  MockRead reads[] = {
    CreateMockRead(*go_away),
    MockRead(true, 0, 0),  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.AddData(data);
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_ABORTED, out.rv);
}

TEST_P(SpdyNetworkTransactionTest, CloseWithActiveStream) {
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
                                     log, GetParam());
  helper.RunPreTestSetup();
  helper.AddData(data.get());
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

// Test to make sure we can correctly connect through a proxy.
TEST_P(SpdyNetworkTransactionTest, ProxyConnect) {
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.session_deps().reset(new SpdySessionDependencies(
      ProxyService::CreateFixedFromPacResult("PROXY myproxy:70")));
  helper.SetSession(make_scoped_refptr(
      SpdySessionDependencies::SpdyCreateSession(helper.session_deps().get())));
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  const char kConnect443[] = {"CONNECT www.google.com:443 HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n\r\n"};
  const char kConnect80[] = {"CONNECT www.google.com:80 HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n\r\n"};
  const char kHTTP200[] = {"HTTP/1.1 200 OK\r\n\r\n"};
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));

  MockWrite writes_SPDYNPN[] = {
    MockWrite(false, kConnect443, arraysize(kConnect443) - 1, 0),
    CreateMockWrite(*req, 2),
  };
  MockRead reads_SPDYNPN[] = {
    MockRead(false, kHTTP200, arraysize(kHTTP200) - 1, 1),
    CreateMockRead(*resp, 3),
    CreateMockRead(*body.get(), 4),
    MockRead(true, 0, 0, 5),
  };

  MockWrite writes_SPDYSSL[] = {
    MockWrite(false, kConnect80, arraysize(kConnect80) - 1, 0),
    CreateMockWrite(*req, 2),
  };
  MockRead reads_SPDYSSL[] = {
    MockRead(false, kHTTP200, arraysize(kHTTP200) - 1, 1),
    CreateMockRead(*resp, 3),
    CreateMockRead(*body.get(), 4),
    MockRead(true, 0, 0, 5),
  };

  MockWrite writes_SPDYNOSSL[] = {
    CreateMockWrite(*req, 0),
  };

  MockRead reads_SPDYNOSSL[] = {
    CreateMockRead(*resp, 1),
    CreateMockRead(*body.get(), 2),
    MockRead(true, 0, 0, 3),
  };

  scoped_refptr<OrderedSocketData> data;
  switch(GetParam()) {
    case SPDYNOSSL:
      data = new OrderedSocketData(reads_SPDYNOSSL,
                                   arraysize(reads_SPDYNOSSL),
                                   writes_SPDYNOSSL,
                                   arraysize(writes_SPDYNOSSL));
      break;
    case SPDYSSL:
      data = new OrderedSocketData(reads_SPDYSSL, arraysize(reads_SPDYSSL),
                                   writes_SPDYSSL, arraysize(writes_SPDYSSL));
      break;
    case SPDYNPN:
      data = new OrderedSocketData(reads_SPDYNPN, arraysize(reads_SPDYNPN),
                                   writes_SPDYNPN, arraysize(writes_SPDYNPN));
      break;
    default:
      NOTREACHED();
  }

  helper.AddData(data.get());
  TestCompletionCallback callback;

  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(0, rv);

  // Verify the SYN_REPLY.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans, &response_data));
  EXPECT_EQ("hello!", response_data);
  helper.VerifyDataConsumed();
}

// Test to make sure we can correctly connect through a proxy to www.google.com,
// if there already exists a direct spdy connection to www.google.com. See
// http://crbug.com/49874
TEST_P(SpdyNetworkTransactionTest, DirectConnectProxyReconnect) {
  // When setting up the first transaction, we store the SpdySessionPool so that
  // we can use the same pool in the second transaction.
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());

  // Use a proxy service which returns a proxy fallback list from DIRECT to
  // myproxy:70. For this test there will be no fallback, so it is equivalent
  // to simply DIRECT. The reason for appending the second proxy is to verify
  // that the session pool key used does is just "DIRECT".
  helper.session_deps().reset(new SpdySessionDependencies(
      ProxyService::CreateFixedFromPacResult("DIRECT; PROXY myproxy:70")));
  helper.SetSession(make_scoped_refptr(
      SpdySessionDependencies::SpdyCreateSession(helper.session_deps().get())));

  SpdySessionPool* spdy_session_pool = helper.session()->spdy_session_pool();
  helper.RunPreTestSetup();

  // Construct and send a simple GET request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req, 1),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    CreateMockRead(*body, 3),
    MockRead(true, ERR_IO_PENDING, 4),  // Force a pause
    MockRead(true, 0, 5)  // EOF
  };
  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  helper.AddData(data.get());
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  TransactionHelperResult out;
  out.rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());

  EXPECT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.rv = ReadTransaction(trans, &out.response_data);
  EXPECT_EQ(OK, out.rv);
  out.status_line = response->headers->GetStatusLine();
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // Check that the SpdySession is still in the SpdySessionPool.
  HostPortPair host_port_pair("www.google.com", helper.port());
  HostPortProxyPair session_pool_key_direct(
      host_port_pair, ProxyServer::Direct());
  EXPECT_TRUE(spdy_session_pool->HasSession(session_pool_key_direct));
  HostPortProxyPair session_pool_key_proxy(
      host_port_pair,
      ProxyServer::FromURI("www.foo.com", ProxyServer::SCHEME_HTTP));
  EXPECT_FALSE(spdy_session_pool->HasSession(session_pool_key_proxy));

  // Set up data for the proxy connection.
  const char kConnect443[] = {"CONNECT www.google.com:443 HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n\r\n"};
  const char kConnect80[] = {"CONNECT www.google.com:80 HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n\r\n"};
  const char kHTTP200[] = {"HTTP/1.1 200 OK\r\n\r\n"};
  scoped_ptr<spdy::SpdyFrame> req2(ConstructSpdyGet(
      "http://www.google.com/foo.dat", false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body2(ConstructSpdyBodyFrame(1, true));

  MockWrite writes_SPDYNPN[] = {
    MockWrite(false, kConnect443, arraysize(kConnect443) - 1, 0),
    CreateMockWrite(*req2, 2),
  };
  MockRead reads_SPDYNPN[] = {
    MockRead(false, kHTTP200, arraysize(kHTTP200) - 1, 1),
    CreateMockRead(*resp2, 3),
    CreateMockRead(*body2, 4),
    MockRead(true, 0, 5)  // EOF
  };

  MockWrite writes_SPDYNOSSL[] = {
    CreateMockWrite(*req2, 0),
  };
  MockRead reads_SPDYNOSSL[] = {
    CreateMockRead(*resp2, 1),
    CreateMockRead(*body2, 2),
    MockRead(true, 0, 3)  // EOF
  };

  MockWrite writes_SPDYSSL[] = {
    MockWrite(false, kConnect80, arraysize(kConnect80) - 1, 0),
    CreateMockWrite(*req2, 2),
  };
  MockRead reads_SPDYSSL[] = {
    MockRead(false, kHTTP200, arraysize(kHTTP200) - 1, 1),
    CreateMockRead(*resp2, 3),
    CreateMockRead(*body2, 4),
    MockRead(true, 0, 0, 5),
  };

  scoped_refptr<OrderedSocketData> data_proxy;
  switch(GetParam()) {
    case SPDYNPN:
      data_proxy = new OrderedSocketData(reads_SPDYNPN,
                                         arraysize(reads_SPDYNPN),
                                         writes_SPDYNPN,
                                         arraysize(writes_SPDYNPN));
      break;
    case SPDYNOSSL:
      data_proxy = new OrderedSocketData(reads_SPDYNOSSL,
                                         arraysize(reads_SPDYNOSSL),
                                         writes_SPDYNOSSL,
                                         arraysize(writes_SPDYNOSSL));
      break;
    case SPDYSSL:
      data_proxy = new OrderedSocketData(reads_SPDYSSL,
                                         arraysize(reads_SPDYSSL),
                                         writes_SPDYSSL,
                                         arraysize(writes_SPDYSSL));
      break;
    default:
      NOTREACHED();
  }

  // Create another request to www.google.com, but this time through a proxy.
  HttpRequestInfo request_proxy;
  request_proxy.method = "GET";
  request_proxy.url = GURL("http://www.google.com/foo.dat");
  request_proxy.load_flags = 0;
  scoped_ptr<SpdySessionDependencies> ssd_proxy(new SpdySessionDependencies());
  // Ensure that this transaction uses the same SpdySessionPool.
  scoped_refptr<HttpNetworkSession> session_proxy(
      SpdySessionDependencies::SpdyCreateSession(ssd_proxy.get()));
  NormalSpdyTransactionHelper helper_proxy(request_proxy,
                                           BoundNetLog(), GetParam());
  HttpNetworkSessionPeer session_peer(session_proxy);
  session_peer.SetProxyService(
          ProxyService::CreateFixedFromPacResult("PROXY myproxy:70"));
  helper_proxy.session_deps().swap(ssd_proxy);
  helper_proxy.SetSession(session_proxy);
  helper_proxy.RunPreTestSetup();
  helper_proxy.AddData(data_proxy.get());

  HttpNetworkTransaction* trans_proxy = helper_proxy.trans();
  TestCompletionCallback callback_proxy;
  int rv = trans_proxy->Start(&request_proxy, &callback_proxy, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback_proxy.WaitForResult();
  EXPECT_EQ(0, rv);

  HttpResponseInfo response_proxy = *trans_proxy->GetResponseInfo();
  EXPECT_TRUE(response_proxy.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response_proxy.headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans_proxy, &response_data));
  EXPECT_EQ("hello!", response_data);

  data->CompleteRead();
  helper_proxy.VerifyDataConsumed();
}

// When we get a TCP-level RST, we need to retry a HttpNetworkTransaction
// on a new connection, if the connection was previously known to be good.
// This can happen when a server reboots without saying goodbye, or when
// we're behind a NAT that masked the RST.
TEST_P(SpdyNetworkTransactionTest, VerifyRetryOnConnectionReset) {
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, ERR_IO_PENDING),
    MockRead(true, ERR_CONNECTION_RESET),
  };

  MockRead reads2[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  // This test has a couple of variants.
  enum {
    // Induce the RST while waiting for our transaction to send.
    VARIANT_RST_DURING_SEND_COMPLETION,
    // Induce the RST while waiting for our transaction to read.
    // In this case, the send completed - everything copied into the SNDBUF.
    VARIANT_RST_DURING_READ_COMPLETION
  };

  for (int variant = VARIANT_RST_DURING_SEND_COMPLETION;
       variant <= VARIANT_RST_DURING_READ_COMPLETION;
       ++variant) {
    scoped_refptr<DelayedSocketData> data1(
        new DelayedSocketData(1, reads, arraysize(reads),
                              NULL, 0));

    scoped_refptr<DelayedSocketData> data2(
        new DelayedSocketData(1, reads2, arraysize(reads2),
                               NULL, 0));

    NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                       BoundNetLog(), GetParam());
    helper.AddData(data1.get());
    helper.AddData(data2.get());
    helper.RunPreTestSetup();

    for (int i = 0; i < 2; ++i) {
      scoped_ptr<HttpNetworkTransaction> trans(
          new HttpNetworkTransaction(helper.session()));

      TestCompletionCallback callback;
      int rv = trans->Start(&helper.request(), &callback, BoundNetLog());
      EXPECT_EQ(ERR_IO_PENDING, rv);
      // On the second transaction, we trigger the RST.
      if (i == 1) {
        if (variant == VARIANT_RST_DURING_READ_COMPLETION) {
          // Writes to the socket complete asynchronously on SPDY by running
          // through the message loop.  Complete the write here.
          MessageLoop::current()->RunAllPending();
        }

        // Now schedule the ERR_CONNECTION_RESET.
        EXPECT_EQ(3u, data1->read_index());
        data1->CompleteRead();
        EXPECT_EQ(4u, data1->read_index());
      }
      rv = callback.WaitForResult();
      EXPECT_EQ(OK, rv);

      const HttpResponseInfo* response = trans->GetResponseInfo();
      ASSERT_TRUE(response != NULL);
      EXPECT_TRUE(response->headers != NULL);
      EXPECT_TRUE(response->was_fetched_via_spdy);
      std::string response_data;
      rv = ReadTransaction(trans.get(), &response_data);
      EXPECT_EQ(OK, rv);
      EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
      EXPECT_EQ("hello!", response_data);
    }

    helper.VerifyDataConsumed();
  }
}

// Test that turning SPDY on and off works properly.
TEST_P(SpdyNetworkTransactionTest, SpdyOnOffToggle) {
  net::HttpStreamFactory::set_spdy_enabled(true);
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite spdy_writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1,
                            spdy_reads, arraysize(spdy_reads),
                            spdy_writes, arraysize(spdy_writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  net::HttpStreamFactory::set_spdy_enabled(false);
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(false, OK),
  };
  scoped_refptr<DelayedSocketData> data2(
      new DelayedSocketData(1, http_reads, arraysize(http_reads),
                            NULL, 0));
  NormalSpdyTransactionHelper helper2(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper2.SetSpdyDisabled();
  helper2.RunToCompletion(data2.get());
  TransactionHelperResult out2 = helper2.output();
  EXPECT_EQ(OK, out2.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out2.status_line);
  EXPECT_EQ("hello from http", out2.response_data);

  net::HttpStreamFactory::set_spdy_enabled(true);
}

// Tests that Basic authentication works over SPDY
TEST_P(SpdyNetworkTransactionTest, SpdyBasicAuth) {
  net::HttpStreamFactory::set_spdy_enabled(true);

  // The first request will be a bare GET, the second request will be a
  // GET with an Authorization header.
  scoped_ptr<spdy::SpdyFrame> req_get(
      ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  const char* const kExtraAuthorizationHeaders[] = {
    "authorization",
    "Basic Zm9vOmJhcg==",
  };
  scoped_ptr<spdy::SpdyFrame> req_get_authorization(
      ConstructSpdyGet(
          kExtraAuthorizationHeaders,
          arraysize(kExtraAuthorizationHeaders) / 2,
          false, 3, LOWEST));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req_get, 1),
    CreateMockWrite(*req_get_authorization, 4),
  };

  // The first response is a 401 authentication challenge, and the second
  // response will be a 200 response since the second request includes a valid
  // Authorization header.
  const char* const kExtraAuthenticationHeaders[] = {
    "WWW-Authenticate",
    "Basic realm=\"MyRealm\""
  };
  scoped_ptr<spdy::SpdyFrame> resp_authentication(
      ConstructSpdySynReplyError(
          "401 Authentication Required",
          kExtraAuthenticationHeaders,
          arraysize(kExtraAuthenticationHeaders) / 2,
          1));
  scoped_ptr<spdy::SpdyFrame> body_authentication(
      ConstructSpdyBodyFrame(1, true));
  scoped_ptr<spdy::SpdyFrame> resp_data(ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<spdy::SpdyFrame> body_data(ConstructSpdyBodyFrame(3, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp_authentication, 2),
    CreateMockRead(*body_authentication, 3),
    CreateMockRead(*resp_data, 5),
    CreateMockRead(*body_data, 6),
    MockRead(true, 0, 7),
  };

  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(spdy_reads, arraysize(spdy_reads),
                            spdy_writes, arraysize(spdy_writes)));
  HttpRequestInfo request(CreateGetRequest());
  BoundNetLog net_log;
  NormalSpdyTransactionHelper helper(request, net_log, GetParam());

  helper.RunPreTestSetup();
  helper.AddData(data.get());
  HttpNetworkTransaction* trans = helper.trans();
  TestCompletionCallback callback_start;
  const int rv_start = trans->Start(&request, &callback_start, net_log);
  EXPECT_EQ(ERR_IO_PENDING, rv_start);
  const int rv_start_complete = callback_start.WaitForResult();
  EXPECT_EQ(OK, rv_start_complete);

  // Make sure the response has an auth challenge.
  const HttpResponseInfo* const response_start = trans->GetResponseInfo();
  ASSERT_TRUE(response_start != NULL);
  ASSERT_TRUE(response_start->headers != NULL);
  EXPECT_EQ(401, response_start->headers->response_code());
  EXPECT_TRUE(response_start->was_fetched_via_spdy);
  ASSERT_TRUE(response_start->auth_challenge.get() != NULL);
  EXPECT_FALSE(response_start->auth_challenge->is_proxy);
  EXPECT_EQ(L"basic", response_start->auth_challenge->scheme);
  EXPECT_EQ(L"MyRealm", response_start->auth_challenge->realm);

  // Restart with a username/password.
  const string16 kFoo(ASCIIToUTF16("foo"));
  const string16 kBar(ASCIIToUTF16("bar"));
  TestCompletionCallback callback_restart;
  const int rv_restart = trans->RestartWithAuth(kFoo, kBar, &callback_restart);
  EXPECT_EQ(ERR_IO_PENDING, rv_restart);
  const int rv_restart_complete = callback_restart.WaitForResult();
  EXPECT_EQ(OK, rv_restart_complete);
  // TODO(cbentzel): This is actually the same response object as before, but
  // data has changed.
  const HttpResponseInfo* const response_restart = trans->GetResponseInfo();
  ASSERT_TRUE(response_restart != NULL);
  ASSERT_TRUE(response_restart->headers != NULL);
  EXPECT_EQ(200, response_restart->headers->response_code());
  EXPECT_TRUE(response_restart->auth_challenge.get() == NULL);
}

TEST_P(SpdyNetworkTransactionTest, ServerPushWithHeaders) {
  static const unsigned char kPushBodyFrame[] = {
    0x00, 0x00, 0x00, 0x02,                                      // header, ID
    0x01, 0x00, 0x00, 0x06,                                      // FIN, length
    'p', 'u', 's', 'h', 'e', 'd'                                 // "pushed"
  };
  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  static const char* const kInitialHeaders[] = {
    "url",
    "http://www.google.com/foo.dat",
  };
  static const char* const kLateHeaders[] = {
    "hello",
    "bye",
    "status",
    "200",
    "version",
    "HTTP/1.1"
  };
  scoped_ptr<spdy::SpdyFrame>
    stream2_syn(ConstructSpdyControlFrame(kInitialHeaders,
                                          arraysize(kInitialHeaders) / 2,
                                          false,
                                          2,
                                          LOWEST,
                                          spdy::SYN_STREAM,
                                          spdy::CONTROL_FLAG_NONE,
                                          NULL,
                                          0,
                                          1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_headers(ConstructSpdyControlFrame(kLateHeaders,
                                                arraysize(kLateHeaders) / 2,
                                                false,
                                                2,
                                                LOWEST,
                                                spdy::HEADERS,
                                                spdy::CONTROL_FLAG_NONE,
                                                NULL,
                                                0,
                                                0));

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream2_headers, 4),
    CreateMockRead(*stream1_body, 5, false),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame),
             arraysize(kPushBodyFrame), 6),
    MockRead(true, ERR_IO_PENDING, 7),  // Force a pause
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  scoped_refptr<OrderedSocketData> data(new OrderedSocketData(
      reads,
      arraysize(reads),
      writes,
      arraysize(writes)));
  RunServerPushTest(data.get(),
                    &response,
                    &response2,
                    expected_push_result);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushClaimBeforeHeaders) {
  // We push a stream and attempt to claim it before the headers come down.
  static const unsigned char kPushBodyFrame[] = {
    0x00, 0x00, 0x00, 0x02,                                      // header, ID
    0x01, 0x00, 0x00, 0x06,                                      // FIN, length
    'p', 'u', 's', 'h', 'e', 'd'                                 // "pushed"
  };
  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 0, false),
  };

  static const char* const kInitialHeaders[] = {
    "url",
    "http://www.google.com/foo.dat",
  };
  static const char* const kLateHeaders[] = {
    "hello",
    "bye",
    "status",
    "200",
    "version",
    "HTTP/1.1"
  };
  scoped_ptr<spdy::SpdyFrame>
      stream2_syn(ConstructSpdyControlFrame(kInitialHeaders,
                                            arraysize(kInitialHeaders) / 2,
                                            false,
                                            2,
                                            LOWEST,
                                            spdy::SYN_STREAM,
                                            spdy::CONTROL_FLAG_NONE,
                                            NULL,
                                            0,
                                            1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_headers(ConstructSpdyControlFrame(kLateHeaders,
                                                arraysize(kLateHeaders) / 2,
                                                false,
                                                2,
                                                LOWEST,
                                                spdy::HEADERS,
                                                spdy::CONTROL_FLAG_NONE,
                                                NULL,
                                                0,
                                                0));

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 1),
    CreateMockRead(*stream2_syn, 2),
    CreateMockRead(*stream1_body, 3),
    CreateMockRead(*stream2_headers, 4),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame),
             arraysize(kPushBodyFrame), 5),
    MockRead(true, 0, 5),  // EOF
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  scoped_refptr<DeterministicSocketData> data(new DeterministicSocketData(
      reads,
      arraysize(reads),
      writes,
      arraysize(writes)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.SetDeterministic();
  helper.AddDeterministicData(static_cast<DeterministicSocketData*>(data));
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  // Run until we've received the primary SYN_STREAM, the pushed SYN_STREAM,
  // and the body of the primary stream, but before we've received the HEADERS
  // for the pushed stream.
  data->SetStop(3);

  // Start the transaction.
  TestCompletionCallback callback;
  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  data->Run();
  rv = callback.WaitForResult();
  EXPECT_EQ(0, rv);

  // Request the pushed path.  At this point, we've received the push, but the
  // headers are not yet complete.
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(helper.session()));
  rv = trans2->Start(&CreateGetPushRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  data->RunFor(3);
  MessageLoop::current()->RunAllPending();

  // Read the server push body.
  std::string result2;
  ReadResult(trans2.get(), data.get(), &result2);
  // Read the response body.
  std::string result;
  ReadResult(trans, data, &result);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());

  // Verify that the received push data is same as the expected push data.
  EXPECT_EQ(result2.compare(expected_push_result), 0)
      << "Received data: "
      << result2
      << "||||| Expected data: "
      << expected_push_result;

  // Verify the SYN_REPLY.
  // Copy the response info, because trans goes away.
  response = *trans->GetResponseInfo();
  response2 = *trans2->GetResponseInfo();

  VerifyStreamsClosed(helper);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushWithTwoHeaderFrames) {
  // We push a stream and attempt to claim it before the headers come down.
  static const unsigned char kPushBodyFrame[] = {
    0x00, 0x00, 0x00, 0x02,                                      // header, ID
    0x01, 0x00, 0x00, 0x06,                                      // FIN, length
    'p', 'u', 's', 'h', 'e', 'd'                                 // "pushed"
  };
  scoped_ptr<spdy::SpdyFrame>
      stream1_syn(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<spdy::SpdyFrame>
      stream1_body(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 0, false),
  };

  static const char* const kInitialHeaders[] = {
    "url",
    "http://www.google.com/foo.dat",
  };
  static const char* const kMiddleHeaders[] = {
    "hello",
    "bye",
  };
  static const char* const kLateHeaders[] = {
    "status",
    "200",
    "version",
    "HTTP/1.1"
  };
  scoped_ptr<spdy::SpdyFrame>
      stream2_syn(ConstructSpdyControlFrame(kInitialHeaders,
                                            arraysize(kInitialHeaders) / 2,
                                            false,
                                            2,
                                            LOWEST,
                                            spdy::SYN_STREAM,
                                            spdy::CONTROL_FLAG_NONE,
                                            NULL,
                                            0,
                                            1));
  scoped_ptr<spdy::SpdyFrame>
      stream2_headers1(ConstructSpdyControlFrame(kMiddleHeaders,
                                                 arraysize(kMiddleHeaders) / 2,
                                                 false,
                                                 2,
                                                 LOWEST,
                                                 spdy::HEADERS,
                                                 spdy::CONTROL_FLAG_NONE,
                                                 NULL,
                                                 0,
                                                 0));
  scoped_ptr<spdy::SpdyFrame>
      stream2_headers2(ConstructSpdyControlFrame(kLateHeaders,
                                                 arraysize(kLateHeaders) / 2,
                                                 false,
                                                 2,
                                                 LOWEST,
                                                 spdy::HEADERS,
                                                 spdy::CONTROL_FLAG_NONE,
                                                 NULL,
                                                 0,
                                                 0));

  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 1),
    CreateMockRead(*stream2_syn, 2),
    CreateMockRead(*stream1_body, 3),
    CreateMockRead(*stream2_headers1, 4),
    CreateMockRead(*stream2_headers2, 5),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame),
             arraysize(kPushBodyFrame), 6),
    MockRead(true, 0, 6),  // EOF
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  scoped_refptr<DeterministicSocketData> data(new DeterministicSocketData(
      reads,
      arraysize(reads),
      writes,
      arraysize(writes)));

  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.SetDeterministic();
  helper.AddDeterministicData(static_cast<DeterministicSocketData*>(data));
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  // Run until we've received the primary SYN_STREAM, the pushed SYN_STREAM,
  // the first HEADERS frame, and the body of the primary stream, but before
  // we've received the final HEADERS for the pushed stream.
  data->SetStop(4);

  // Start the transaction.
  TestCompletionCallback callback;
  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  data->Run();
  rv = callback.WaitForResult();
  EXPECT_EQ(0, rv);

  // Request the pushed path.  At this point, we've received the push, but the
  // headers are not yet complete.
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(helper.session()));
  rv = trans2->Start(&CreateGetPushRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  data->RunFor(3);
  MessageLoop::current()->RunAllPending();

  // Read the server push body.
  std::string result2;
  ReadResult(trans2.get(), data, &result2);
  // Read the response body.
  std::string result;
  ReadResult(trans, data, &result);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());

  // Verify that the received push data is same as the expected push data.
  EXPECT_EQ(result2.compare(expected_push_result), 0)
      << "Received data: "
      << result2
      << "||||| Expected data: "
      << expected_push_result;

  // Verify the SYN_REPLY.
  // Copy the response info, because trans goes away.
  response = *trans->GetResponseInfo();
  response2 = *trans2->GetResponseInfo();

  VerifyStreamsClosed(helper);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());

  // Verify we got all the headers
  EXPECT_TRUE(response2.headers->HasHeaderValue(
      "url",
      "http://www.google.com/foo.dat"));
  EXPECT_TRUE(response2.headers->HasHeaderValue("hello", "bye"));
  EXPECT_TRUE(response2.headers->HasHeaderValue("status", "200"));
  EXPECT_TRUE(response2.headers->HasHeaderValue("version", "HTTP/1.1"));
}

TEST_P(SpdyNetworkTransactionTest, SynReplyWithHeaders) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  static const char* const kInitialHeaders[] = {
    "status",
    "200 OK",
    "version",
    "HTTP/1.1"
  };
  static const char* const kLateHeaders[] = {
    "hello",
    "bye",
  };
  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyControlFrame(kInitialHeaders,
                                              arraysize(kInitialHeaders) / 2,
                                              false,
                                              1,
                                              LOWEST,
                                              spdy::SYN_REPLY,
                                              spdy::CONTROL_FLAG_NONE,
                                              NULL,
                                              0,
                                              0));
  scoped_ptr<spdy::SpdyFrame>
      stream1_headers(ConstructSpdyControlFrame(kLateHeaders,
                                                arraysize(kLateHeaders) / 2,
                                                false,
                                                1,
                                                LOWEST,
                                                spdy::HEADERS,
                                                spdy::CONTROL_FLAG_NONE,
                                                NULL,
                                                0,
                                                0));
  scoped_ptr<spdy::SpdyFrame> stream1_body(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply),
    CreateMockRead(*stream1_headers),
    CreateMockRead(*stream1_body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, SynReplyWithLateHeaders) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  static const char* const kInitialHeaders[] = {
    "status",
    "200 OK",
    "version",
    "HTTP/1.1"
  };
  static const char* const kLateHeaders[] = {
    "hello",
    "bye",
  };
  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyControlFrame(kInitialHeaders,
                                              arraysize(kInitialHeaders) / 2,
                                              false,
                                              1,
                                              LOWEST,
                                              spdy::SYN_REPLY,
                                              spdy::CONTROL_FLAG_NONE,
                                              NULL,
                                              0,
                                              0));
  scoped_ptr<spdy::SpdyFrame>
      stream1_headers(ConstructSpdyControlFrame(kLateHeaders,
                                                arraysize(kLateHeaders) / 2,
                                                false,
                                                1,
                                                LOWEST,
                                                spdy::HEADERS,
                                                spdy::CONTROL_FLAG_NONE,
                                                NULL,
                                                0,
                                                0));
  scoped_ptr<spdy::SpdyFrame> stream1_body(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> stream1_body2(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply),
    CreateMockRead(*stream1_body),
    CreateMockRead(*stream1_headers),
    CreateMockRead(*stream1_body2),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, SynReplyWithDuplicateLateHeaders) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = { CreateMockWrite(*req) };

  static const char* const kInitialHeaders[] = {
    "status",
    "200 OK",
    "version",
    "HTTP/1.1"
  };
  static const char* const kLateHeaders[] = {
    "status",
    "500 Server Error",
  };
  scoped_ptr<spdy::SpdyFrame>
      stream1_reply(ConstructSpdyControlFrame(kInitialHeaders,
                                              arraysize(kInitialHeaders) / 2,
                                              false,
                                              1,
                                              LOWEST,
                                              spdy::SYN_REPLY,
                                              spdy::CONTROL_FLAG_NONE,
                                              NULL,
                                              0,
                                              0));
  scoped_ptr<spdy::SpdyFrame>
      stream1_headers(ConstructSpdyControlFrame(kLateHeaders,
                                                arraysize(kLateHeaders) / 2,
                                                false,
                                                1,
                                                LOWEST,
                                                spdy::HEADERS,
                                                spdy::CONTROL_FLAG_NONE,
                                                NULL,
                                                0,
                                                0));
  scoped_ptr<spdy::SpdyFrame> stream1_body(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<spdy::SpdyFrame> stream1_body2(ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply),
    CreateMockRead(*stream1_body),
    CreateMockRead(*stream1_headers),
    CreateMockRead(*stream1_body2),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  NormalSpdyTransactionHelper helper(CreateGetRequest(),
                                     BoundNetLog(), GetParam());
  helper.RunToCompletion(data.get());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
}

TEST_P(SpdyNetworkTransactionTest, ServerPushCrossOriginCorrectness) {
  // In this test we want to verify that we can't accidentally push content
  // which can't be pushed by this content server.
  // This test assumes that:
  //   - if we're requesting http://www.foo.com/barbaz
  //   - the browser has made a connection to "www.foo.com".

  // A list of the URL to fetch, followed by the URL being pushed.
  static const char* const kTestCases[] = {
    "http://www.google.com/foo.html",
    "http://www.google.com:81/foo.js",     // Bad port

    "http://www.google.com/foo.html",
    "https://www.google.com/foo.js",       // Bad protocol

    "http://www.google.com/foo.html",
    "ftp://www.google.com/foo.js",         // Invalid Protocol

    "http://www.google.com/foo.html",
    "http://blat.www.google.com/foo.js",   // Cross subdomain

    "http://www.google.com/foo.html",
    "http://www.foo.com/foo.js",           // Cross domain
  };


  static const unsigned char kPushBodyFrame[] = {
    0x00, 0x00, 0x00, 0x02,                                      // header, ID
    0x01, 0x00, 0x00, 0x06,                                      // FIN, length
    'p', 'u', 's', 'h', 'e', 'd'                                 // "pushed"
  };

  for (size_t index = 0; index < arraysize(kTestCases); index += 2) {
    const char* url_to_fetch = kTestCases[index];
    const char* url_to_push = kTestCases[index + 1];

    scoped_ptr<spdy::SpdyFrame>
        stream1_syn(ConstructSpdyGet(url_to_fetch, false, 1, LOWEST));
    scoped_ptr<spdy::SpdyFrame>
        stream1_body(ConstructSpdyBodyFrame(1, true));
    scoped_ptr<spdy::SpdyFrame> push_rst(
        ConstructSpdyRstStream(2, spdy::REFUSED_STREAM));
    MockWrite writes[] = {
      CreateMockWrite(*stream1_syn, 1),
      CreateMockWrite(*push_rst, 4),
    };

    scoped_ptr<spdy::SpdyFrame>
        stream1_reply(ConstructSpdyGetSynReply(NULL, 0, 1));
    scoped_ptr<spdy::SpdyFrame>
        stream2_syn(ConstructSpdyPush(NULL,
                                      0,
                                      2,
                                      1,
                                      url_to_push));
    scoped_ptr<spdy::SpdyFrame> rst(
        ConstructSpdyRstStream(2, spdy::CANCEL));

    MockRead reads[] = {
      CreateMockRead(*stream1_reply, 2),
      CreateMockRead(*stream2_syn, 3),
      CreateMockRead(*stream1_body, 5, false),
      MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame),
               arraysize(kPushBodyFrame), 6),
      MockRead(true, ERR_IO_PENDING, 7),  // Force a pause
    };

    HttpResponseInfo response;
    scoped_refptr<OrderedSocketData> data(new OrderedSocketData(
        reads,
        arraysize(reads),
        writes,
        arraysize(writes)));

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL(url_to_fetch);
    request.load_flags = 0;
    NormalSpdyTransactionHelper helper(request,
                                       BoundNetLog(), GetParam());
    helper.RunPreTestSetup();
    helper.AddData(data);

    HttpNetworkTransaction* trans = helper.trans();

    // Start the transaction with basic parameters.
    TestCompletionCallback callback;

    int rv = trans->Start(&request, &callback, BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    rv = callback.WaitForResult();

    // Read the response body.
    std::string result;
    ReadResult(trans, data, &result);

    // Verify that we consumed all test data.
    EXPECT_TRUE(data->at_read_eof());
    EXPECT_TRUE(data->at_write_eof());

    // Verify the SYN_REPLY.
    // Copy the response info, because trans goes away.
    response = *trans->GetResponseInfo();

    VerifyStreamsClosed(helper);

    // Verify the SYN_REPLY.
    EXPECT_TRUE(response.headers != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());
  }
}

}  // namespace net
