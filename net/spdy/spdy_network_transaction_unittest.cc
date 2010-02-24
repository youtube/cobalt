// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_network_transaction.h"

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/load_log_unittest.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_data.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_unittest.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_protocol.h"
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
        spdy_session_pool(new SpdySessionPool) {
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
        spdy_session_pool(new SpdySessionPool) {}

  scoped_refptr<MockHostResolverBase> host_resolver;
  scoped_refptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  MockClientSocketFactory socket_factory;
  scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory;
  scoped_refptr<SpdySessionPool> spdy_session_pool;
};

HttpNetworkSession* CreateSession(SessionDependencies* session_deps) {
  return new HttpNetworkSession(NULL,
                                session_deps->host_resolver,
                                session_deps->proxy_service,
                                &session_deps->socket_factory,
                                session_deps->ssl_config_service,
                                session_deps->spdy_session_pool,
                                session_deps->http_auth_handler_factory.get());
}

// Chop a frame into an array of MockWrites.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopFrame(const char* data, int length, int num_chunks) {
  MockWrite* chunks = new MockWrite[num_chunks];
  int chunk_size = length / num_chunks;
  for (int index = 0; index < num_chunks; index++) {
    const char* ptr = data + (index * chunk_size);
    if (index == num_chunks - 1)
      chunk_size += length % chunk_size;  // The last chunk takes the remainder.
    chunks[index] = MockWrite(true, ptr, chunk_size);
  }
  return chunks;
}

// ----------------------------------------------------------------------------

static const unsigned char kGetSyn[] = {
  0x80, 0x01, 0x00, 0x01,                                        // header
  0x01, 0x00, 0x00, 0x49,                                        // FIN, len
  0x00, 0x00, 0x00, 0x01,                                        // stream id
  0x00, 0x00, 0x00, 0x00,                                        // associated
  0xc0, 0x00, 0x00, 0x03,                                        // 4 headers
  0x00, 0x06, 'm', 'e', 't', 'h', 'o', 'd',
  0x00, 0x03, 'G', 'E', 'T',
  0x00, 0x03, 'u', 'r', 'l',
  0x00, 0x16, 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
              '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o',
              'm', '/',
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
};

static const unsigned char kGetSynCompressed[] = {
  0x80, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x47,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0xc0, 0x00, 0x78, 0xbb, 0xdf, 0xa2, 0x51, 0xb2,
  0x62, 0x60, 0x66, 0x60, 0xcb, 0x05, 0xe6, 0xc3,
  0xfc, 0x14, 0x06, 0x66, 0x77, 0xd7, 0x10, 0x06,
  0x66, 0x90, 0xa0, 0x58, 0x46, 0x49, 0x49, 0x81,
  0x95, 0xbe, 0x3e, 0x30, 0xe2, 0xf5, 0xd2, 0xf3,
  0xf3, 0xd3, 0x73, 0x52, 0xf5, 0x92, 0xf3, 0x73,
  0xf5, 0x19, 0xd8, 0xa1, 0x1a, 0x19, 0x38, 0x60,
  0xe6, 0x01, 0x00, 0x00, 0x00, 0xff, 0xff
};

static const unsigned char kGetSynReply[] = {
  0x80, 0x01, 0x00, 0x02,                                        // header
  0x00, 0x00, 0x00, 0x45,
  0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x04,                                        // 4 headers
  0x00, 0x05, 'h', 'e', 'l', 'l', 'o',                           // "hello"
  0x00, 0x03, 'b', 'y', 'e',                                     // "bye"
  0x00, 0x06, 's', 't', 'a', 't', 'u', 's',                      // "status"
  0x00, 0x03, '2', '0', '0',                                     // "200"
  0x00, 0x03, 'u', 'r', 'l',                                     // "url"
  0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',  // "/index...
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',                 // "version"
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',            // "HTTP/1.1"
};

static const unsigned char kGetBodyFrame[] = {
  0x00, 0x00, 0x00, 0x01,                                        // header
  0x01, 0x00, 0x00, 0x06,                                        // FIN, length
  'h', 'e', 'l', 'l', 'o', '!',                                  // "hello"
};

static const unsigned char kPostSyn[] = {
  0x80, 0x01, 0x00, 0x01,                                      // header
  0x00, 0x00, 0x00, 0x4a,                                      // flags, len
  0x00, 0x00, 0x00, 0x01,                                      // stream id
  0x00, 0x00, 0x00, 0x00,                                      // associated
  0xc0, 0x00, 0x00, 0x03,                                      // 4 headers
  0x00, 0x06, 'm', 'e', 't', 'h', 'o', 'd',
  0x00, 0x04, 'P', 'O', 'S', 'T',
  0x00, 0x03, 'u', 'r', 'l',
  0x00, 0x16, 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
              '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o',
              'm', '/',
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
};

static const unsigned char kPostUploadFrame[] = {
  0x00, 0x00, 0x00, 0x01,                                        // header
  0x01, 0x00, 0x00, 0x0c,                                        // FIN flag
  'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '\0'
};

// The response
static const unsigned char kPostSynReply[] = {
  0x80, 0x01, 0x00, 0x02,                                        // header
  0x00, 0x00, 0x00, 0x45,
  0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x04,                                        // 4 headers
  0x00, 0x05, 'h', 'e', 'l', 'l', 'o',                           // "hello"
  0x00, 0x03, 'b', 'y', 'e',                                     // "bye"
  0x00, 0x06, 's', 't', 'a', 't', 'u', 's',                      // "status"
  0x00, 0x03, '2', '0', '0',                                     // "200"
  0x00, 0x03, 'u', 'r', 'l',                                     // "url"
  // "/index.php"
  0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',                 // "version"
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',            // "HTTP/1.1"
};

static const unsigned char kPostBodyFrame[] = {
  0x00, 0x00, 0x00, 0x01,                                        // header
  0x01, 0x00, 0x00, 0x06,                                        // FIN, length
  'h', 'e', 'l', 'l', 'o', '!',                                  // "hello"
};

}  // namespace

// A DataProvider where the client must write a request before the reads (e.g.
// the response) will complete.
class DelayedSocketData : public StaticSocketDataProvider,
                          public base::RefCounted<DelayedSocketData> {
 public:
  // |write_delay| the number of MockWrites to complete before allowing
  //               a MockRead to complete.
  // |reads| the list of MockRead completions.
  // |writes| the list of MockWrite completions.
  // Note: All MockReads and MockWrites must be async.
  // Note: The MockRead and MockWrite lists musts end with a EOF
  //       e.g. a MockRead(true, 0, 0);
  DelayedSocketData(int write_delay,
                    MockRead* reads, size_t reads_count,
                    MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      write_delay_(write_delay),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
    DCHECK_GE(write_delay_, 0);
  }

  // |connect| the result for the connect phase.
  // |reads| the list of MockRead completions.
  // |write_delay| the number of MockWrites to complete before allowing
  //               a MockRead to complete.
  // |writes| the list of MockWrite completions.
  // Note: All MockReads and MockWrites must be async.
  // Note: The MockRead and MockWrite lists musts end with a EOF
  //       e.g. a MockRead(true, 0, 0);
  DelayedSocketData(const MockConnect& connect, int write_delay,
                    MockRead* reads, size_t reads_count,
                    MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      write_delay_(write_delay),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
    DCHECK_GE(write_delay_, 0);
    set_connect_data(connect);
  }

  virtual MockRead GetNextRead() {
    if (write_delay_)
      return MockRead(true, ERR_IO_PENDING);
    return StaticSocketDataProvider::GetNextRead();
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    MockWriteResult rv = StaticSocketDataProvider::OnWrite(data);
    // Now that our write has completed, we can allow reads to continue.
    if (!--write_delay_)
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
        factory_.NewRunnableMethod(&DelayedSocketData::CompleteRead), 100);
    return rv;
  }

  virtual void Reset() {
    set_socket(NULL);
    factory_.RevokeAll();
    StaticSocketDataProvider::Reset();
  }

  void CompleteRead() {
    if (socket())
      socket()->OnReadComplete(GetNextRead());
  }

 private:
  int write_delay_;
  ScopedRunnableMethodFactory<DelayedSocketData> factory_;
};

class SpdyNetworkTransactionTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    // By default, all tests turn off compression.
    EnableCompression(false);
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
                                            LoadLog* log) {
    TransactionHelperResult out;

    // We disable SSL for this test.
    SpdySession::SetSSLMode(false);

    SessionDependencies session_deps;
    scoped_ptr<SpdyNetworkTransaction> trans(
        new SpdyNetworkTransaction(CreateSession(&session_deps)));

    session_deps.socket_factory.AddSocketDataProvider(data);

    TestCompletionCallback callback;

    int rv = trans->Start(&request, &callback, log);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    out.rv = callback.WaitForResult();
    if (out.rv != OK)
      return out;

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response->headers != NULL);
    EXPECT_TRUE(response->was_fetched_via_spdy);
    out.status_line = response->headers->GetStatusLine();
    out.response_info = *response;  // Make a copy so we can verify.

    rv = ReadTransaction(trans.get(), &out.response_data);
    EXPECT_EQ(OK, rv);

    // Verify that we consumed all test data.
    EXPECT_TRUE(data->at_read_eof());
    EXPECT_TRUE(data->at_write_eof());

    return out;
  }

  void ConnectStatusHelperWithExpectedStatus(const MockRead& status,
                                             int expected_status);

  void ConnectStatusHelper(const MockRead& status);
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
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(request, data.get(), NULL);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a simple POST works.
TEST_F(SpdyNetworkTransactionTest, Post) {
  static const char upload[] = { "hello world" };

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data = new UploadData();
  request.upload_data->AppendBytes(upload, sizeof(upload));

  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kPostSyn),
              arraysize(kPostSyn)),
    MockWrite(true, reinterpret_cast<const char*>(kPostUploadFrame),
              arraysize(kPostUploadFrame)),
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kPostSynReply),
             arraysize(kPostSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kPostBodyFrame),
             arraysize(kPostBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(request, data.get(), NULL);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a simple POST works.
TEST_F(SpdyNetworkTransactionTest, EmptyPost) {
static const unsigned char kEmptyPostSyn[] = {
  0x80, 0x01, 0x00, 0x01,                                      // header
  0x01, 0x00, 0x00, 0x4a,                                      // flags, len
  0x00, 0x00, 0x00, 0x01,                                      // stream id
  0x00, 0x00, 0x00, 0x00,                                      // associated
  0xc0, 0x00, 0x00, 0x03,                                      // 4 headers
  0x00, 0x06, 'm', 'e', 't', 'h', 'o', 'd',
  0x00, 0x04, 'P', 'O', 'S', 'T',
  0x00, 0x03, 'u', 'r', 'l',
  0x00, 0x16, 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
              '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o',
              'm', '/',
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
};

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  // Create an empty UploadData.
  request.upload_data = new UploadData();

  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kEmptyPostSyn),
              arraysize(kEmptyPostSyn)),
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kPostSynReply),
             arraysize(kPostSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kPostBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
    new DelayedSocketData(1, reads, arraysize(reads),
                          writes, arraysize(writes)));

  TransactionHelperResult out = TransactionHelper(request, data, NULL);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that the transaction doesn't crash when we don't have a reply.
TEST_F(SpdyNetworkTransactionTest, ResponseWithoutSynReply) {
  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kPostBodyFrame),
             arraysize(kPostBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads), NULL, 0));
  TransactionHelperResult out = TransactionHelper(request, data.get(), NULL);
  EXPECT_EQ(ERR_SYN_REPLY_NOT_RECEIVED, out.rv);
}

TEST_F(SpdyNetworkTransactionTest, CancelledTransaction) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
    MockRead(true, 0, 0)  // EOF
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    // This following read isn't used by the test, except during the
    // RunAllPending() call at the end since the SpdySession survives the
    // SpdyNetworkTransaction and still tries to continue Read()'ing.  Any
    // MockRead will do here.
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  StaticSocketDataProvider data(reads, arraysize(reads),
                                writes, arraysize(writes));
  session_deps.socket_factory.AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  trans.reset();  // Cancel the transaction.

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();
}

// Verify that various SynReply headers parse correctly through the
// HTTP layer.
TEST_F(SpdyNetworkTransactionTest, SynReplyHeaders) {
  // This uses a multi-valued cookie header.
  static const unsigned char syn_reply1[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x4c,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 'c', 'o', 'o', 'k', 'i', 'e',
    0x00, 0x09, 'v', 'a', 'l', '1', '\0',
                'v', 'a', 'l', '2',
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  // This is the minimalist set of headers.
  static const unsigned char syn_reply2[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x39,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  // Headers with a comma separated list.
  static const unsigned char syn_reply3[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x4c,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 'c', 'o', 'o', 'k', 'i', 'e',
    0x00, 0x09, 'v', 'a', 'l', '1', ',', 'v', 'a', 'l', '2',
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  struct SynReplyTests {
    const unsigned char* syn_reply;
    int syn_reply_length;
    const char* expected_headers;
  } test_cases[] = {
    // Test the case of a multi-valued cookie.  When the value is delimited
    // with NUL characters, it needs to be unfolded into multiple headers.
    { syn_reply1, sizeof(syn_reply1),
      "cookie: val1\n"
      "cookie: val2\n"
      "status: 200\n"
      "url: /index.php\n"
      "version: HTTP/1.1\n"
    },
    // This is the simplest set of headers possible.
    { syn_reply2, sizeof(syn_reply2),
      "status: 200\n"
      "url: /index.php\n"
      "version: HTTP/1.1\n"
    },
    // Test that a comma delimited list is NOT interpreted as a multi-value
    // name/value pair.  The comma-separated list is just a single value.
    { syn_reply3, sizeof(syn_reply3),
      "cookie: val1,val2\n"
      "status: 200\n"
      "url: /index.php\n"
      "version: HTTP/1.1\n"
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    MockWrite writes[] = {
      MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
                arraysize(kGetSyn)),
    };

    MockRead reads[] = {
      MockRead(true, reinterpret_cast<const char*>(test_cases[i].syn_reply),
               test_cases[i].syn_reply_length),
      MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
               arraysize(kGetBodyFrame)),
      MockRead(true, 0, 0)  // EOF
    };

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;
    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(request, data.get(), NULL);
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

// Verify that we don't crash on invalid SynReply responses.
TEST_F(SpdyNetworkTransactionTest, InvalidSynReply) {
  static const unsigned char kSynReplyMissingStatus[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x3f,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 'c', 'o', 'o', 'k', 'i', 'e',
    0x00, 0x09, 'v', 'a', 'l', '1', '\0',
                'v', 'a', 'l', '2',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  static const unsigned char kSynReplyMissingVersion[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x26,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
  };

  struct SynReplyTests {
    const unsigned char* syn_reply;
    int syn_reply_length;
  } test_cases[] = {
    { kSynReplyMissingStatus, arraysize(kSynReplyMissingStatus) },
    { kSynReplyMissingVersion, arraysize(kSynReplyMissingVersion) }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    MockWrite writes[] = {
      MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
                arraysize(kGetSyn)),
      MockWrite(true, 0, 0)  // EOF
    };

    MockRead reads[] = {
      MockRead(true, reinterpret_cast<const char*>(test_cases[i].syn_reply),
               test_cases[i].syn_reply_length),
      MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
               arraysize(kGetBodyFrame)),
      MockRead(true, 0, 0)  // EOF
    };

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;
    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(request, data.get(), NULL);
    EXPECT_EQ(ERR_INVALID_RESPONSE, out.rv);
  }
}

// Verify that we don't crash on some corrupt frames.
TEST_F(SpdyNetworkTransactionTest, CorruptFrameSessionError) {
  static const unsigned char kSynReplyMassiveLength[] = {
    0x80, 0x01, 0x00, 0x02,
    0x0f, 0x11, 0x11, 0x26,   // This is the length field with a big number
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
  };

  struct SynReplyTests {
    const unsigned char* syn_reply;
    int syn_reply_length;
  } test_cases[] = {
    { kSynReplyMassiveLength, arraysize(kSynReplyMassiveLength) }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    MockWrite writes[] = {
      MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
                arraysize(kGetSyn)),
      MockWrite(true, 0, 0)  // EOF
    };

    MockRead reads[] = {
      MockRead(true, reinterpret_cast<const char*>(test_cases[i].syn_reply),
               test_cases[i].syn_reply_length),
      MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
               arraysize(kGetBodyFrame)),
      MockRead(true, 0, 0)  // EOF
    };

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;
    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(request, data.get(), NULL);
    EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
  }
}

TEST_F(SpdyNetworkTransactionTest, ServerPush) {
  // Reply with the X-Associated-Content header.
  static const unsigned char syn_reply[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x71,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x14, 'X', '-', 'A', 's', 's', 'o', 'c', 'i', 'a', 't',
                'e', 'd', '-', 'C', 'o', 'n', 't', 'e', 'n', 't',
    0x00, 0x20, '1', '?', '?', 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w',
                'w', '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o', 'm',
                '/', 'f', 'o', 'o', '.', 'd', 'a', 't',
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  // Syn for the X-Associated-Content (foo.dat)
  static const unsigned char syn_push[] = {
    0x80, 0x01, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x4b,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00,  // TODO(mbelshe): use new server push protocol.
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x04, 'p', 'a', 't', 'h',
    0x00, 0x08, '/', 'f', 'o', 'o', '.', 'd', 'a', 't',
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x08, '/', 'f', 'o', 'o', '.', 'd', 'a', 't',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  // Body for stream 2
  static const unsigned char body_frame_2[] = {
    0x00, 0x00, 0x00, 0x02,
    0x01, 0x00, 0x00, 0x07,
    'g', 'o', 'o', 'd', 'b', 'y', 'e',
  };

  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(syn_reply),
             arraysize(syn_reply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, ERR_IO_PENDING),  // Force a pause
    MockRead(true, reinterpret_cast<const char*>(syn_push),
             arraysize(syn_push)),
    MockRead(true, reinterpret_cast<const char*>(body_frame_2),
             arraysize(body_frame_2)),
    MockRead(true, ERR_IO_PENDING),  // Force a pause
    MockRead(true, 0, 0)  // EOF
  };

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  enum TestTypes {
    // Simulate that the server sends the first request, notifying the client
    // that it *will* push the second stream.  But the client issues the
    // request for the second stream before the push data arrives.
    PUSH_AFTER_REQUEST,
    // Simulate that the server is sending the pushed stream data before the
    // client requests it.  The SpdySession will buffer the response and then
    // deliver the data when the client does make the request.
    PUSH_BEFORE_REQUEST,
    DONE
  };

  for (int test_type = PUSH_AFTER_REQUEST; test_type != DONE; ++test_type) {
    // Setup a mock session.
    SessionDependencies session_deps;
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    session_deps.socket_factory.AddSocketDataProvider(data.get());

    // Issue the first request
    {
      SpdyNetworkTransaction trans(session.get());

      // Issue the first request.
      HttpRequestInfo request;
      request.method = "GET";
      request.url = GURL("http://www.google.com/");
      request.load_flags = 0;
      TestCompletionCallback callback;
      int rv = trans.Start(&request, &callback, NULL);
      EXPECT_EQ(ERR_IO_PENDING, rv);

      rv = callback.WaitForResult();
      EXPECT_EQ(rv, OK);

      // Verify the SYN_REPLY.
      const HttpResponseInfo* response = trans.GetResponseInfo();
      EXPECT_TRUE(response->headers != NULL);
      EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

      if (test_type == PUSH_BEFORE_REQUEST)
        data->CompleteRead();

      // Verify the body.
      std::string response_data;
      rv = ReadTransaction(&trans, &response_data);
      EXPECT_EQ(OK, rv);
      EXPECT_EQ("hello!", response_data);
    }

    // Issue a second request for the X-Associated-Content.
    {
      SpdyNetworkTransaction trans(session.get());

      HttpRequestInfo request;
      request.method = "GET";
      request.url = GURL("http://www.google.com/foo.dat");
      request.load_flags = 0;
      TestCompletionCallback callback;
      int rv = trans.Start(&request, &callback, NULL);
      EXPECT_EQ(ERR_IO_PENDING, rv);

      // In the case where we are Complete the next read now.
      if (test_type == PUSH_AFTER_REQUEST)
        data->CompleteRead();

      rv = callback.WaitForResult();
      EXPECT_EQ(rv, OK);

      // Verify the SYN_REPLY.
      const HttpResponseInfo* response = trans.GetResponseInfo();
      EXPECT_TRUE(response->headers != NULL);
      EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

      // Verify the body.
      std::string response_data;
      rv = ReadTransaction(&trans, &response_data);
      EXPECT_EQ(OK, rv);
      EXPECT_EQ("goodbye", response_data);
    }

    // Complete the next read now and teardown.
    data->CompleteRead();

    // Verify that we consumed all test data.
    EXPECT_TRUE(data->at_read_eof());
    EXPECT_TRUE(data->at_write_eof());
  }
}

// Test that we shutdown correctly on write errors.
TEST_F(SpdyNetworkTransactionTest, WriteError) {
  MockWrite writes[] = {
    // We'll write 10 bytes successfully
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn), 10),
    // Followed by ERROR!
    MockWrite(true, ERR_FAILED),
    MockWrite(true, 0, 0)  // EOF
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(request, data.get(), NULL);
  EXPECT_EQ(ERR_FAILED, out.rv);
  data->Reset();
}

// Test that partial writes work.
TEST_F(SpdyNetworkTransactionTest, PartialWrite) {
  // Chop the SYN_STREAM frame into 5 chunks.
  const int kChunks = 5;
  scoped_array<MockWrite> writes(ChopFrame(
      reinterpret_cast<const char*>(kGetSyn), arraysize(kGetSyn), kChunks));

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(kChunks, reads, arraysize(reads),
                            writes.get(), kChunks));
  TransactionHelperResult out = TransactionHelper(request, data.get(), NULL);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

TEST_F(SpdyNetworkTransactionTest, DISABLED_ConnectFailure) {
  MockConnect connects[]  = {
    MockConnect(true, ERR_NAME_NOT_RESOLVED),
    MockConnect(false, ERR_NAME_NOT_RESOLVED),
    MockConnect(true, ERR_INTERNET_DISCONNECTED),
    MockConnect(false, ERR_INTERNET_DISCONNECTED)
  };

  for (size_t index = 0; index < arraysize(connects); ++index) {
    MockWrite writes[] = {
      MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
                arraysize(kGetSyn)),
      MockWrite(true, 0, 0)  // EOF
    };

    MockRead reads[] = {
      MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
               arraysize(kGetSynReply)),
      MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
               arraysize(kGetBodyFrame)),
      MockRead(true, 0, 0)  // EOF
    };

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;
    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(connects[index], 1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(request, data.get(), NULL);
    EXPECT_EQ(connects[index].result, out.rv);
  }
}

// In this test, we enable compression, but get a uncompressed SynReply from
// the server.  Verify that teardown is all clean.
TEST_F(SpdyNetworkTransactionTest, DecompressFailureOnSynReply) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSynCompressed),
              arraysize(kGetSynCompressed)),
    MockWrite(true, 0, 0)  // EOF
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  // For this test, we turn on the normal compression.
  EnableCompression(true);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(request, data.get(), NULL);
  EXPECT_EQ(ERR_SYN_REPLY_NOT_RECEIVED, out.rv);
  data->Reset();

  EnableCompression(false);
}

// Test that the LoadLog contains good data for a simple GET request.
TEST_F(SpdyNetworkTransactionTest, LoadLog) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<net::LoadLog> log(new net::LoadLog(net::LoadLog::kUnbounded));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                  log);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // Check that the LoadLog was filled reasonably.
  // This test is intentionally non-specific about the exact ordering of
  // the log; instead we just check to make sure that certain events exist.
  EXPECT_LT(0u, log->entries().size());
  int pos = 0;
  // We know the first event at position 0.
  EXPECT_TRUE(net::LogContainsBeginEvent(
      *log, 0, net::LoadLog::TYPE_SPDY_TRANSACTION_INIT_CONNECTION));
  // For the rest of the events, allow additional events in the middle,
  // but expect these to be logged in order.
  pos = net::ExpectLogContainsSomewhere(log, 0,
      net::LoadLog::TYPE_SPDY_TRANSACTION_INIT_CONNECTION,
      net::LoadLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(log, pos + 1,
      net::LoadLog::TYPE_SPDY_TRANSACTION_SEND_REQUEST,
      net::LoadLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(log, pos + 1,
      net::LoadLog::TYPE_SPDY_TRANSACTION_SEND_REQUEST,
      net::LoadLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(log, pos + 1,
      net::LoadLog::TYPE_SPDY_TRANSACTION_READ_HEADERS,
      net::LoadLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(log, pos + 1,
      net::LoadLog::TYPE_SPDY_TRANSACTION_READ_HEADERS,
      net::LoadLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(log, pos + 1,
      net::LoadLog::TYPE_SPDY_TRANSACTION_READ_BODY,
      net::LoadLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(log, pos + 1,
      net::LoadLog::TYPE_SPDY_TRANSACTION_READ_BODY,
      net::LoadLog::PHASE_END);
}

}  // namespace net
