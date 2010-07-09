// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_network_transaction.h"

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "net/base/completion_callback.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_log_unittest.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_data.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_unittest.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_test_util.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

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
        http_auth_handler_factory(HttpAuthHandlerFactory::CreateDefault()),
        spdy_session_pool(new SpdySessionPool()) {
    // Note: The CancelledTransaction test does cleanup by running all tasks
    // in the message loop (RunAllPending).  Unfortunately, that doesn't clean
    // up tasks on the host resolver thread; and TCPConnectJob is currently
    // not cancellable.  Using synchronous lookups allows the test to shutdown
    // cleanly.  Until we have cancellable TCPConnectJobs, use synchronous
    // lookups.
    host_resolver->set_synchronous_mode(true);
  }

  // Custom proxy service dependency.
  explicit SessionDependencies(ProxyService* proxy_service)
      : host_resolver(new MockHostResolver),
        proxy_service(proxy_service),
        ssl_config_service(new SSLConfigServiceDefaults),
        http_auth_handler_factory(HttpAuthHandlerFactory::CreateDefault()),
        spdy_session_pool(new SpdySessionPool()) {}

  scoped_refptr<MockHostResolverBase> host_resolver;
  scoped_refptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  MockClientSocketFactory socket_factory;
  scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory;
  scoped_refptr<SpdySessionPool> spdy_session_pool;
};

HttpNetworkSession* CreateSession(SessionDependencies* session_deps) {
  return new HttpNetworkSession(session_deps->host_resolver,
                                session_deps->proxy_service,
                                &session_deps->socket_factory,
                                session_deps->ssl_config_service,
                                session_deps->spdy_session_pool,
                                session_deps->http_auth_handler_factory.get(),
                                NULL,
                                NULL);
}

}  // namespace

class SpdyNetworkTransactionTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    // By default, all tests turn off compression.
    EnableCompression(false);
    google_get_request_initialized_ = false;
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

  TransactionHelperResult TransactionHelper(const HttpRequestInfo& request,
                                            DelayedSocketData* data,
                                            const BoundNetLog& log) {
    SessionDependencies session_deps;
    HttpNetworkSession* session = CreateSession(&session_deps);
    return TransactionHelperWithSession(request, data, log, &session_deps,
                                        session);
  }

  TransactionHelperResult TransactionHelperWithSession(
      const HttpRequestInfo& request, DelayedSocketData* data,
      const BoundNetLog& log, SessionDependencies* session_deps,
      HttpNetworkSession* session) {
    CHECK(session);
    CHECK(session_deps);

    TransactionHelperResult out;

    // We disable SSL for this test.
    SpdySession::SetSSLMode(false);

    scoped_ptr<SpdyNetworkTransaction> trans(
        new SpdyNetworkTransaction(session));

    session_deps->socket_factory.AddSocketDataProvider(data);

    TestCompletionCallback callback;

    out.rv = trans->Start(&request, &callback, log);
    EXPECT_LT(out.rv, 0);  // We expect an IO Pending or some sort of error.
    if (out.rv != ERR_IO_PENDING)
      return out;

    out.rv = callback.WaitForResult();
    if (out.rv != OK) {
      session->spdy_session_pool()->ClearSessions();
      return out;
    }

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response->headers != NULL);
    EXPECT_TRUE(response->was_fetched_via_spdy);
    out.status_line = response->headers->GetStatusLine();
    out.response_info = *response;  // Make a copy so we can verify.

    out.rv = ReadTransaction(trans.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);

    // Verify that we consumed all test data.
    EXPECT_TRUE(data->at_read_eof());
    EXPECT_TRUE(data->at_write_eof());

    return out;
  }

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

// Verify SpdyNetworkTransaction constructor.
TEST_F(SpdyNetworkTransactionTest, Constructor) {
  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session =
      CreateSession(&session_deps);
  scoped_ptr<HttpTransaction> trans(new SpdyNetworkTransaction(session));
}

TEST_F(SpdyNetworkTransactionTest, Get) {
  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(CreateGetRequest(),
                                                  data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
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
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
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
  TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                  BoundNetLog());
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
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  TransactionHelperResult out = TransactionHelper(request, data, BoundNetLog());
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
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
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
  TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
}

// Test that the transaction doesn't crash when we don't have a reply.
TEST_F(SpdyNetworkTransactionTest, ResponseWithoutSynReply) {
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads[] = {
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads), NULL, 0));
  TransactionHelperResult out = TransactionHelper(CreateGetRequest(),
                                                  data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(ERR_SYN_REPLY_NOT_RECEIVED, out.rv);
}

// Test that the transaction doesn't crash when we get two replies on the same
// stream ID. See http://crbug.com/45639.
TEST_F(SpdyNetworkTransactionTest, ResponseWithTwoSynReplies) {
  SessionDependencies session_deps;
  HttpNetworkSession* session = CreateSession(&session_deps);

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  session_deps.socket_factory.AddSocketDataProvider(data.get());

  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(session));

  TestCompletionCallback callback;
  int rv = trans->Start(&request, &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, rv);
}

TEST_F(SpdyNetworkTransactionTest, CancelledTransaction) {
  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    // This following read isn't used by the test, except during the
    // RunAllPending() call at the end since the SpdySession survives the
    // SpdyNetworkTransaction and still tries to continue Read()'ing.  Any
    // MockRead will do here.
    MockRead(true, 0, 0)  // EOF
  };

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  StaticSocketDataProvider data(reads, arraysize(reads),
                                writes, arraysize(writes));
  session_deps.socket_factory.AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  trans.reset();  // Cancel the transaction.

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();
}

class DeleteSessionCallback : public CallbackRunner< Tuple1<int> > {
 public:
  explicit DeleteSessionCallback(SpdyNetworkTransaction* trans1) :
      trans(trans1) {}

  // We kill the transaction, which deletes the session and stream. However, the
  // memory is still accessible, so we also have to zero out the memory of the
  // stream. This is not a well defined operation, and can cause failures.
  virtual void RunWithParams(const Tuple1<int>& params) {
    delete trans;
  }

 private:
  const SpdyNetworkTransaction* trans;
};

// Verify that the client can correctly deal with the user callback deleting the
// transaction. Failures will usually be valgrind errors. See
// http://crbug.com/46925
TEST_F(SpdyNetworkTransactionTest, DeleteSessionOnReadCallback) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp.get(), 2),
    MockRead(true, ERR_IO_PENDING, 3),  // Force a pause
    CreateMockRead(*body.get(), 4),
    MockRead(true, 0, 0, 5),  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  SpdyNetworkTransaction * trans =
      new SpdyNetworkTransaction(CreateSession(&session_deps));
  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  session_deps.socket_factory.AddSocketDataProvider(data);

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&request, &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();

  // Setup a user callback which will delete the session, and clear out the
  // memory holding the stream object. Note that the callback deletes trans.
  DeleteSessionCallback callback2(trans);
  const int kSize = 3000;
  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kSize);
  rv = trans->Read(buf, kSize, &callback2);
  ASSERT_EQ(ERR_IO_PENDING, rv);
  data->CompleteRead();

  // Finish running rest of tasks.
  MessageLoop::current()->RunAllPending();
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
    scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
    MockWrite writes[] = { CreateMockWrite(*req) };

    scoped_ptr<spdy::SpdyFrame> resp(
        ConstructSpdyGetSynReply(test_cases[i].extra_headers,
                                 test_cases[i].num_headers));
    scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
    MockRead reads[] = {
      CreateMockRead(*resp),
      CreateMockRead(*body),
      MockRead(true, 0, 0)  // EOF
    };

    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(CreateGetRequest(),
                                                    data.get(),
                                                    BoundNetLog());
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
                       test_cases[i].num_headers[0]));

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

    scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
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
    TransactionHelperResult out = TransactionHelper(request,
                                                    data.get(),
                                                    BoundNetLog());
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
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
    MockWrite writes[] = {
      CreateMockWrite(*req),
      MockWrite(true, 0, 0)  // EOF
    };

    scoped_ptr<spdy::SpdyFrame> resp(
        ConstructSpdyPacket(kSynStartHeader,
                            NULL, 0,
                            test_cases[i].headers,
                            test_cases[i].num_headers));
    scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
    MockRead reads[] = {
      CreateMockRead(*resp),
      CreateMockRead(*body),
      MockRead(true, 0, 0)  // EOF
    };

    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(CreateGetRequest(),
                                                    data.get(),
                                                    BoundNetLog());
    EXPECT_EQ(ERR_INVALID_RESPONSE, out.rv);
  }
}

// Verify that we don't crash on some corrupt frames.
// TODO(eroman): Renable this test, see http://crbug.com/48588
TEST_F(SpdyNetworkTransactionTest, DISABLED_CorruptFrameSessionError) {
  // This is the length field with a big number
  scoped_ptr<spdy::SpdyFrame> syn_reply_massive_length(
      ConstructSpdyGetSynReply(NULL, 0));
  syn_reply_massive_length->set_length(0x111126);

  struct SynReplyTests {
    const spdy::SpdyFrame* syn_reply;
  } test_cases[] = {
    { syn_reply_massive_length.get(), },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
    MockWrite writes[] = {
      CreateMockWrite(*req),
      MockWrite(true, 0, 0)  // EOF
    };

    scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
    MockRead reads[] = {
      CreateMockRead(*test_cases[i].syn_reply),
      CreateMockRead(*body),
      MockRead(true, 0, 0)  // EOF
    };

    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(CreateGetRequest(),
                                                    data.get(),
                                                    BoundNetLog());
    EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
  }
}

// Server push:
// ------------
// Client: Send the original SYN request.
// Server: Receive the SYN request.
// Server: Send a SYN reply, with X-Associated-Content and URL(s).
// Server: For each URL, send a SYN_STREAM with the URL and a stream ID,
//         followed by one or more Data frames (the last one with a FIN).
// Client: Requests the URL(s).
// Client: Receives the SYN_STREAMs, and the associated Data frames, and
//         associates the URLs with the incoming stream IDs.
//
// There are three possibilities when the client tries to send the second
// request (which doesn't make it to the wire):
//
// 1. The push data has arrived and is complete.
// 2. The push data has started arriving, but hasn't finished.
// 3. The push data has not yet arrived.

// Enum for ServerPush.
enum TestTypes {
  // Simulate that the server sends the first request, notifying the client
  // that it *will* push the second stream.  But the client issues the
  // request for the second stream before the push data arrives.
  PUSH_AFTER_REQUEST,
  // Simulate that the server is sending the pushed stream data before the
  // client requests it.  The SpdySession will buffer the response and then
  // deliver the data when the client does make the request.
  PUSH_BEFORE_REQUEST,
  // Simulate that the server is sending the pushed stream data before the
  // client requests it, but the stream has not yet finished when the request
  // occurs.  The SpdySession will buffer the response and then deliver the
  // data when the response is complete.
  PUSH_DURING_REQUEST,
  DONE
};

// Creates and processes a SpdyNetworkTransaction for server push, based on
//   |session|.
// |data| holds the expected writes, and the reads.
// |url| is the web page we want.  In pass 2, it contains the resource we expect
//   to be pushed.
// |expected_data| is the data we expect to get in response.
// |test_type| is one of PUSH_AFTER_REQUEST, PUSH_BEFORE_REQUEST, or
//   PUSH_DURING_REQUEST, indicating the type of test we're running.
// |pass| is 1 for the first request, and 2 for the request for the push data.
// |response| is the response information for the request.  It will be used to
//   verify the response time stamps.
static void MakeRequest(scoped_refptr<HttpNetworkSession> session,
                        scoped_refptr<OrderedSocketData> data,
                        const GURL& url,
                        const std::string& expected_data,
                        int test_type,
                        int pass,
                        HttpResponseInfo* response) {
  SpdyNetworkTransaction trans(session.get());

  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.load_flags = 0;
  TestCompletionCallback callback;

  // Allows the STOP_LOOP flag to work.
  data->SetCompletionCallback(&callback);
  // Sends a request.  In pass 1, this goes on the wire; in pass 2, it is
  // preempted by the push data.
  int rv = trans.Start(&request, &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // In the case where we are pushing beforehand, complete the next read now.
  if ((pass == 2) && (test_type == PUSH_AFTER_REQUEST)) {
    data->CompleteRead();
  }

  // Process messages until either a FIN or a STOP_LOOP is encountered.
  rv = callback.WaitForResult();
  if ((pass == 2) && (test_type == PUSH_DURING_REQUEST)) {
    // We should be in the middle of a request, so we're pending.
    EXPECT_EQ(ERR_IO_PENDING, rv);
  } else {
    EXPECT_EQ(OK, rv);
  }

  // Verify the SYN_REPLY.
  // Copy the response info, because trans goes away
  *response = *trans.GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  // In the case where we are Complete the next read now.
  if (((pass == 1) &&
      ((test_type == PUSH_BEFORE_REQUEST) ||
          (test_type == PUSH_DURING_REQUEST)))) {
    data->CompleteRead();
  }

  // Verify the body.
  std::string response_data;
  rv = ReadTransaction(&trans, &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ(expected_data, response_data);
  // Remove callback, so that if another STOP_LOOP occurs, there is no crash.
  data->SetCompletionCallback(NULL);
}

TEST_F(SpdyNetworkTransactionTest, ServerPush) {
  spdy::SpdyFramer framer;

  // Reply with the X-Associated-Content header.
  const char* const kSynReplyHeaders[] = {
      "x-associated-content", "1??http://www.google.com/foo.dat",
  };
  scoped_ptr<spdy::SpdyFrame> syn_reply(
      ConstructSpdyGetSynReply(kSynReplyHeaders, 1));

  const char* const kSynPushHeaders[] = {
      "path", "/foo.dat",
      "status", "200",
      "url", "/foo.dat",
      "version", "HTTP/1.1",
  };
  spdy::SpdyHeaderBlock headers;
  AppendHeadersToSpdyFrame(kSynPushHeaders, 4, &headers);
  scoped_ptr<spdy::SpdyFrame> syn_push(
      framer.CreateSynStream(2,  // Stream ID
                             0,  // Associated stream ID
                             SPDY_PRIORITY_LOWEST,
                             spdy::CONTROL_FLAG_NONE,
                             false,
                             &headers));

  // Body for stream 2
  const char syn_body_data1[] = "hello";
  const char syn_body_data2[] = "hello my darling hello my baby";
  const char* syn_body_data = NULL;

  scoped_ptr<spdy::SpdyFrame> push_body_frame(
    framer.CreateDataFrame(2, syn_body_data1, strlen(syn_body_data1),
                           spdy::DATA_FLAG_FIN));
  scoped_ptr<spdy::SpdyFrame> push_body_frame1(
    framer.CreateDataFrame(2, syn_body_data2, strlen(syn_body_data2),
                           spdy::DATA_FLAG_FIN));

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // This array is for request before and after push is received.  The push
  // body is only one 'packet', to allow the initial transaction to read all
  // the push data before .
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads1[] = {
    CreateMockRead(*syn_reply.get(), 2),                            // 0
    CreateMockRead(*body.get(), 1),                                 // 1
    MockRead(true, ERR_IO_PENDING, 4),  // Force a pause            // 2
    CreateMockRead(*syn_push.get(), 5),                             // 3
    CreateMockRead(*push_body_frame.get(), 6),                      // 4
    MockRead(true, ERR_IO_PENDING, 7),  // Force a pause            // 5
    MockRead(true, 0, 0, 8)  // EOF                                 // 6
  };

  // This array is for request while push is being received.  It extends
  // the push body so we can 'interrupt' it.
  scoped_array<MockRead> chopped_reads(
      ChopReadFrame(*push_body_frame1.get(), 4));
  chopped_reads[0].sequence_number = 5;
  // Force a pause by skipping a sequence number.
  chopped_reads[1].sequence_number = 7;
  chopped_reads[2].sequence_number = 8;
  chopped_reads[3].sequence_number = 9;
  MockRead reads2[] = {
    CreateMockRead(*syn_reply.get(), 2),                            // 0
    CreateMockRead(*body.get(), 3),                                 // 1
    CreateMockRead(*syn_push.get(), 4),                             // 2
    chopped_reads[0],                                               // 3
    chopped_reads[1],                                               // 4
    chopped_reads[2],                                               // 5
    chopped_reads[3],                                               // 6
    MockRead(true, ERR_IO_PENDING, MockRead::STOPLOOP | 10),        // 7
    // So we can do a final CompleteRead(), which cleans up memory.
    MockRead(true, NULL, 0, 11)                                     // 8
  };

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  base::Time zero_time = base::Time::FromInternalValue(0);
  for (int test_type = PUSH_AFTER_REQUEST; test_type < DONE; ++test_type) {
    DLOG(INFO) << "Test " << test_type;

    // Select the data to use.
    MockRead* reads = NULL;
    size_t num_reads = 0;
    size_t num_writes = arraysize(writes);
    int first_push_data_frame = 0;
    if (test_type == PUSH_DURING_REQUEST) {
      reads = reads2;
      num_reads = arraysize(reads2);
      syn_body_data = syn_body_data2;
      first_push_data_frame = 3;
    } else {
      reads = reads1;
      num_reads = arraysize(reads1);
      syn_body_data = syn_body_data1;
      first_push_data_frame = 4;
    }
    // Clear timestamp data
    for (size_t w = 0; w < num_writes; ++w) {
      writes[w].time_stamp = zero_time;
    }
    for (size_t r = 0; r < num_reads; ++r) {
      reads[r].time_stamp = zero_time;
    }

    // Setup a mock session.
    SessionDependencies session_deps;
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    scoped_refptr<OrderedSocketData> data(
        new OrderedSocketData(reads, num_reads, writes, num_writes));
    session_deps.socket_factory.AddSocketDataProvider(data.get());
    HttpResponseInfo response1;
    HttpResponseInfo response2;

    DLOG(INFO) << "Sending request 1";

    // Issue the first request.
    MakeRequest(session,
                data,
                GURL("http://www.google.com/"),
                "hello!",
                test_type,
                1,
                &response1);

    DLOG(INFO) << "Sending X-Associated-Content request";

    // This value should be set to something later than the one in
    // 'response1.request_time'.
    base::Time request1_time = writes[0].time_stamp;
    // We don't have a |writes| entry for the second request,
    // so put in Now() as the request time.  It's not as accurate,
    // but it will work.
    base::Time request2_time = base::Time::Now();

    // Issue a second request for the X-Associated-Content.
    MakeRequest(session,
                data,
                GURL("http://www.google.com/foo.dat"),
                syn_body_data,
                test_type,
                2,
                &response2);

    // Complete the next read now and teardown.
    data->CompleteRead();

    // Verify that we consumed all test data.
    EXPECT_TRUE(data->at_read_eof());
    EXPECT_TRUE(data->at_write_eof());

    // Check the timings

    // Verify that all the time stamps were set.
    EXPECT_GE(response1.request_time.ToInternalValue(),
        zero_time.ToInternalValue());
    EXPECT_GE(response2.request_time.ToInternalValue(),
        zero_time.ToInternalValue());
    EXPECT_GE(response1.response_time.ToInternalValue(),
        zero_time.ToInternalValue());
    EXPECT_GE(response2.response_time.ToInternalValue(),
        zero_time.ToInternalValue());

    // Verify that the values make sense.
    // First request.
    EXPECT_LE(response1.request_time.ToInternalValue(),
        request1_time.ToInternalValue());
    EXPECT_LE(response1.response_time.ToInternalValue(),
        reads[1].time_stamp.ToInternalValue());

    // Push request.
    EXPECT_GE(response2.request_time.ToInternalValue(),
        request2_time.ToInternalValue());
    // The response time should be between the server push SYN and DATA.
    EXPECT_GE(response2.response_time.ToInternalValue(),
      reads[first_push_data_frame - 1].time_stamp.ToInternalValue());
    EXPECT_LE(response2.response_time.ToInternalValue(),
      reads[first_push_data_frame].time_stamp.ToInternalValue());
  }
}

// Test that we shutdown correctly on write errors.
TEST_F(SpdyNetworkTransactionTest, WriteError) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  MockWrite writes[] = {
    // We'll write 10 bytes successfully
    MockWrite(true, req->data(), 10),
    // Followed by ERROR!
    MockWrite(true, ERR_FAILED),
    MockWrite(true, 0, 0)  // EOF
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp.get(), 2),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(CreateGetRequest(),
                                                  data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(ERR_FAILED, out.rv);
  data->Reset();
}

// Test that partial writes work.
TEST_F(SpdyNetworkTransactionTest, PartialWrite) {
  // Chop the SYN_STREAM frame into 5 chunks.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  const int kChunks = 5;
  scoped_array<MockWrite> writes(ChopWriteFrame(*req.get(), kChunks));

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(kChunks, reads, arraysize(reads),
                            writes.get(), kChunks));
  TransactionHelperResult out = TransactionHelper(CreateGetRequest(),
                                                  data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

TEST_F(SpdyNetworkTransactionTest, ConnectFailure) {
  MockConnect connects[]  = {
    MockConnect(true, ERR_NAME_NOT_RESOLVED),
    MockConnect(false, ERR_NAME_NOT_RESOLVED),
    MockConnect(true, ERR_INTERNET_DISCONNECTED),
    MockConnect(false, ERR_INTERNET_DISCONNECTED)
  };

  for (size_t index = 0; index < arraysize(connects); ++index) {
    scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
    MockWrite writes[] = {
      CreateMockWrite(*req),
      MockWrite(true, 0, 0)  // EOF
    };

    scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
    scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
    MockRead reads[] = {
      CreateMockRead(*resp),
      CreateMockRead(*body),
      MockRead(true, 0, 0)  // EOF
    };

    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(connects[index], 1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(CreateGetRequest(),
                                                    data.get(),
                                                    BoundNetLog());
    EXPECT_EQ(connects[index].result, out.rv);
  }
}

// In this test, we enable compression, but get a uncompressed SynReply from
// the server.  Verify that teardown is all clean.
TEST_F(SpdyNetworkTransactionTest, DecompressFailureOnSynReply) {
  // For this test, we turn on the normal compression.
  EnableCompression(true);

  scoped_ptr<spdy::SpdyFrame> compressed(ConstructSpdyGet(NULL, 0, true));
  MockWrite writes[] = {
    CreateMockWrite(*compressed),
    MockWrite(true, 0, 0)  // EOF
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(CreateGetRequest(),
                                                  data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(ERR_SYN_REPLY_NOT_RECEIVED, out.rv);
  data->Reset();

  EnableCompression(false);
}

// Test that the NetLog contains good data for a simple GET request.
TEST_F(SpdyNetworkTransactionTest, NetLog) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  net::CapturingBoundNetLog log(net::CapturingNetLog::kUnbounded);

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(CreateGetRequest(),
                                                  data.get(),
                                                  log.bound());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // Check that the NetLog was filled reasonably.
  // This test is intentionally non-specific about the exact ordering of
  // the log; instead we just check to make sure that certain events exist.
  EXPECT_LT(0u, log.entries().size());
  int pos = 0;
  // We know the first event at position 0.
  EXPECT_TRUE(net::LogContainsBeginEvent(
      log.entries(), 0, net::NetLog::TYPE_SPDY_TRANSACTION_INIT_CONNECTION));
  // For the rest of the events, allow additional events in the middle,
  // but expect these to be logged in order.
  pos = net::ExpectLogContainsSomewhere(log.entries(), 0,
      net::NetLog::TYPE_SPDY_TRANSACTION_INIT_CONNECTION,
      net::NetLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
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

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
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

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
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

  // For this test, we can't use the TransactionHelper, because we are
  // going to tightly control how the IOs fly.

  TransactionHelperResult out;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  session_deps.socket_factory.AddSocketDataProvider(data);

  TestCompletionCallback callback;

  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

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

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();


  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("goodbye world", out.response_data);
}

// Verify that basic buffering works; when multiple data frames arrive
// at the same time, ensure that we don't notify a read completion for
// each data frame individually.
TEST_F(SpdyNetworkTransactionTest, Buffering) {
  spdy::SpdyFramer framer;

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
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

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(true, ERR_IO_PENDING),  // Force a pause
    MockRead(true, combined_data_frames, combined_data_frames_len),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  // For this test, we can't use the TransactionHelper, because we are
  // going to tightly control how the IOs fly.

  TransactionHelperResult out;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  session_deps.socket_factory.AddSocketDataProvider(data);

  TestCompletionCallback callback;

  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

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

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("messagemessagemessagemessage", out.response_data);
}

// Verify the case where we buffer data but read it after it has been buffered.
TEST_F(SpdyNetworkTransactionTest, BufferedAll) {
  spdy::SpdyFramer framer;

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // 5 data frames in a single read.
  scoped_ptr<spdy::SpdyFrame> syn_reply(
      ConstructSpdyGetSynReply(NULL, 0));
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

  // For this test, we can't use the TransactionHelper, because we are
  // going to tightly control how the IOs fly.

  TransactionHelperResult out;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  session_deps.socket_factory.AddSocketDataProvider(data);

  TestCompletionCallback callback;

  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

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

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("messagemessagemessagemessage", out.response_data);
}

// Verify the case where we buffer data and close the connection.
TEST_F(SpdyNetworkTransactionTest, BufferedClosed) {
  spdy::SpdyFramer framer;

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
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
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(true, ERR_IO_PENDING),  // Force a wait
    MockRead(true, combined_data_frames, combined_data_frames_len),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  // For this test, we can't use the TransactionHelper, because we are
  // going to tightly control how the IOs fly.

  TransactionHelperResult out;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  session_deps.socket_factory.AddSocketDataProvider(data);

  TestCompletionCallback callback;

  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

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

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());
}

// Verify the case where we buffer data and cancel the transaction.
TEST_F(SpdyNetworkTransactionTest, BufferedCancelled) {
  spdy::SpdyFramer framer;

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // NOTE: We don't FIN the stream.
  scoped_ptr<spdy::SpdyFrame> data_frame(
      framer.CreateDataFrame(1, "message", 7, spdy::DATA_FLAG_NONE));

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(true, ERR_IO_PENDING),  // Force a wait
    CreateMockRead(*data_frame),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  // For this test, we can't use the TransactionHelper, because we are
  // going to tightly control how the IOs fly.
  TransactionHelperResult out;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  session_deps.socket_factory.AddSocketDataProvider(data);

  TestCompletionCallback callback;

  int rv = trans->Start(&CreateGetRequest(), &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

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
      trans.reset();
      break;
    }
    // We shouldn't get here in this test.
    FAIL() << "Unexpected read: " << rv;
  } while (rv > 0);

  // Flush the MessageLoop; this will cause the buffered IO task
  // to run for the final time.
  MessageLoop::current()->RunAllPending();
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

  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Verify that no settings exist initially.
  HostPortPair host_port_pair("www.google.com", 80);
  EXPECT_TRUE(session->spdy_settings().Get(host_port_pair).empty());

  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
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

  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads[] = {
    CreateMockRead(*reply),
    CreateMockRead(*body),
    CreateMockRead(*settings_frame),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelperWithSession(
      CreateGetRequest(), data.get(), BoundNetLog(),
      &session_deps, session.get());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  {
    // Verify we had two persisted settings.
    spdy::SpdySettings saved_settings =
        session->spdy_settings().Get(host_port_pair);
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

  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Verify that no settings exist initially.
  HostPortPair host_port_pair("www.google.com", 80);
  EXPECT_TRUE(session->spdy_settings().Get(host_port_pair).empty());

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

    session->mutable_spdy_settings()->Set(host_port_pair, settings);
  }

  EXPECT_EQ(2u, session->spdy_settings().Get(host_port_pair).size());

  // Construct the SETTINGS frame.
  const spdy::SpdySettings& settings =
      session->spdy_settings().Get(host_port_pair);
  scoped_ptr<spdy::SpdyFrame> settings_frame(ConstructSpdySettings(settings));

  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));

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

  scoped_ptr<spdy::SpdyFrame> body(ConstructSpdyBodyFrame());
  MockRead reads[] = {
    CreateMockRead(*reply),
    CreateMockRead(*body),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelperWithSession(
      CreateGetRequest(), data.get(), BoundNetLog(),
      &session_deps, session.get());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  {
    // Verify we had two persisted settings.
    spdy::SpdySettings saved_settings =
        session->spdy_settings().Get(host_port_pair);
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
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> go_away(ConstructSpdyGoAway());
  MockRead reads[] = {
    CreateMockRead(*go_away),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(CreateGetRequest(),
                                                  data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, out.rv);
}

TEST_F(SpdyNetworkTransactionTest, CloseWithActiveStream) {
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(false, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  BoundNetLog log;
  SessionDependencies session_deps;
  HttpNetworkSession* session = CreateSession(&session_deps);
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(session));

  session_deps.socket_factory.AddSocketDataProvider(data);

  TestCompletionCallback callback;

  out.rv = trans->Start(&CreateGetRequest(), &callback, log);
  EXPECT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.rv = ReadTransaction(trans.get(), &out.response_data);
  EXPECT_EQ(ERR_CONNECTION_CLOSED, out.rv);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());
}

}  // namespace net
