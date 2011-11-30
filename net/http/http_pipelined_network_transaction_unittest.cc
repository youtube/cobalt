// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "net/base/address_list.h"
#include "net/base/host_cache.h"
#include "net/base/io_buffer.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/request_priority.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_auth_handler_mock.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrEq;

namespace net {

namespace {

class HttpPipelinedNetworkTransactionTest : public testing::Test {
 public:
  HttpPipelinedNetworkTransactionTest()
      : histograms_("a"),
        pool_(1, 1, &histograms_, &factory_) {
  }

  virtual void SetUp() OVERRIDE {
    default_pipelining_enabled_ = HttpStreamFactory::http_pipelining_enabled();
    HttpStreamFactory::set_http_pipelining_enabled(true);
  }

  virtual void TearDown() OVERRIDE {
    MessageLoop::current()->RunAllPending();
    HttpStreamFactory::set_http_pipelining_enabled(default_pipelining_enabled_);
  }

  void Initialize() {
    proxy_service_.reset(ProxyService::CreateDirect());
    ssl_config_ = new SSLConfigServiceDefaults;
    auth_handler_factory_.reset(new HttpAuthHandlerMock::Factory());

    HttpNetworkSession::Params session_params;
    session_params.client_socket_factory = &factory_;
    session_params.proxy_service = proxy_service_.get();
    session_params.host_resolver = &mock_resolver_;
    session_params.ssl_config_service = ssl_config_.get();
    session_params.http_auth_handler_factory = auth_handler_factory_.get();
    session_params.http_server_properties = &http_server_properties_;
    session_ = new HttpNetworkSession(session_params);
  }

  void AddExpectedConnection(MockRead* reads, size_t reads_count,
                             MockWrite* writes, size_t writes_count) {
    DeterministicSocketData* data = new DeterministicSocketData(
        reads, reads_count, writes, writes_count);
    data->set_connect_data(MockConnect(false, 0));
    if (reads_count || writes_count) {
      data->StopAfter(reads_count + writes_count);
    }
    factory_.AddSocketDataProvider(data);
    data_vector_.push_back(data);
  }

  const HttpRequestInfo* GetRequestInfo(const char* filename) {
    std::string url = StringPrintf("http://localhost/%s", filename);
    HttpRequestInfo* request_info = new HttpRequestInfo;
    request_info->url = GURL(url);
    request_info->method = "GET";
    request_info_vector_.push_back(request_info);
    return request_info;
  }

  void ExpectResponse(const std::string& expected,
                      HttpNetworkTransaction& transaction) {
    scoped_refptr<IOBuffer> buffer(new IOBuffer(expected.size()));
    EXPECT_EQ(static_cast<int>(expected.size()),
              transaction.Read(buffer.get(), expected.size(), &callback_));
    std::string actual(buffer->data(), expected.size());
    EXPECT_THAT(actual, StrEq(expected));
    EXPECT_EQ(OK, transaction.Read(buffer.get(), expected.size(), &callback_));
  }

  void CompleteTwoRequests() {
    scoped_ptr<HttpNetworkTransaction> one_transaction(
        new HttpNetworkTransaction(session_.get()));
    TestOldCompletionCallback one_callback;
    EXPECT_EQ(ERR_IO_PENDING,
              one_transaction->Start(GetRequestInfo("one.html"), &one_callback,
                                     BoundNetLog()));
    EXPECT_EQ(OK, one_callback.WaitForResult());

    HttpNetworkTransaction two_transaction(session_.get());
    TestOldCompletionCallback two_callback;
    EXPECT_EQ(ERR_IO_PENDING,
              two_transaction.Start(GetRequestInfo("two.html"), &two_callback,
                                    BoundNetLog()));

    TestOldCompletionCallback one_read_callback;
    scoped_refptr<IOBuffer> buffer(new IOBuffer(8));
    EXPECT_EQ(ERR_IO_PENDING,
              one_transaction->Read(buffer.get(), 8, &one_read_callback));

    data_vector_[0]->RunFor(2);
    EXPECT_EQ(8, one_read_callback.WaitForResult());
    data_vector_[0]->SetStop(10);
    std::string actual(buffer->data(), 8);
    EXPECT_THAT(actual, StrEq("one.html"));
    EXPECT_EQ(OK, one_transaction->Read(buffer.get(), 8, &one_read_callback));

    EXPECT_EQ(OK, two_callback.WaitForResult());
    ExpectResponse("two.html", two_transaction);
  }

  DeterministicMockClientSocketFactory factory_;
  ClientSocketPoolHistograms histograms_;
  MockTransportClientSocketPool pool_;
  std::vector<scoped_refptr<DeterministicSocketData> > data_vector_;
  TestOldCompletionCallback callback_;
  ScopedVector<HttpRequestInfo> request_info_vector_;
  bool default_pipelining_enabled_;

  scoped_ptr<ProxyService> proxy_service_;
  MockHostResolver mock_resolver_;
  scoped_refptr<SSLConfigService> ssl_config_;
  scoped_ptr<HttpAuthHandlerMock::Factory> auth_handler_factory_;
  HttpServerPropertiesImpl http_server_properties_;
  scoped_refptr<HttpNetworkSession> session_;
};

TEST_F(HttpPipelinedNetworkTransactionTest, OneRequest) {
  Initialize();

  MockWrite writes[] = {
    MockWrite(false, 0, "GET /test.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 9\r\n\r\n"),
    MockRead(false, 3, "test.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  HttpNetworkTransaction transaction(session_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            transaction.Start(GetRequestInfo("test.html"), &callback_,
                              BoundNetLog()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  ExpectResponse("test.html", transaction);
}

TEST_F(HttpPipelinedNetworkTransactionTest, ReusePipeline) {
  Initialize();

  MockWrite writes[] = {
    MockWrite(false, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(false, 3, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(true, 4, "one.html"),
    MockRead(false, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(false, 7, "two.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  CompleteTwoRequests();
}

TEST_F(HttpPipelinedNetworkTransactionTest, ReusesOnSpaceAvailable) {
  int old_max_sockets = ClientSocketPoolManager::max_sockets_per_group();
  ClientSocketPoolManager::set_max_sockets_per_group(1);
  Initialize();

  MockWrite writes[] = {
    MockWrite(false, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(false, 4, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(false, 7, "GET /three.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(false, 12, "GET /four.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(false, 3, "one.html"),
    MockRead(false, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(false, 8, "two.html"),
    MockRead(false, 9, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 10, "Content-Length: 10\r\n\r\n"),
    MockRead(false, 11, "three.html"),
    MockRead(false, 13, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 14, "Content-Length: 9\r\n\r\n"),
    MockRead(false, 15, "four.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpNetworkTransaction> one_transaction(
      new HttpNetworkTransaction(session_.get()));
  TestOldCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction->Start(GetRequestInfo("one.html"), &one_callback,
                                   BoundNetLog()));
  EXPECT_EQ(OK, one_callback.WaitForResult());

  HttpNetworkTransaction two_transaction(session_.get());
  TestOldCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction.Start(GetRequestInfo("two.html"), &two_callback,
                                  BoundNetLog()));

  HttpNetworkTransaction three_transaction(session_.get());
  TestOldCompletionCallback three_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            three_transaction.Start(GetRequestInfo("three.html"),
                                    &three_callback, BoundNetLog()));

  HttpNetworkTransaction four_transaction(session_.get());
  TestOldCompletionCallback four_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            four_transaction.Start(GetRequestInfo("four.html"), &four_callback,
                                   BoundNetLog()));

  ExpectResponse("one.html", *one_transaction.get());
  EXPECT_EQ(OK, two_callback.WaitForResult());
  ExpectResponse("two.html", two_transaction);
  EXPECT_EQ(OK, three_callback.WaitForResult());
  ExpectResponse("three.html", three_transaction);

  one_transaction.reset();
  EXPECT_EQ(OK, four_callback.WaitForResult());
  ExpectResponse("four.html", four_transaction);

  ClientSocketPoolManager::set_max_sockets_per_group(old_max_sockets);
}

TEST_F(HttpPipelinedNetworkTransactionTest, UnknownSizeEvictsToNewPipeline) {
  Initialize();

  MockWrite writes[] = {
    MockWrite(false, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(false, 2, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n\r\n"),
    MockRead(true, 3, "one.html"),
    MockRead(false, OK, 4),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  MockWrite writes2[] = {
    MockWrite(false, 0, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads2[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(false, 3, "two.html"),
  };
  AddExpectedConnection(reads2, arraysize(reads2), writes2, arraysize(writes2));

  CompleteTwoRequests();
}

TEST_F(HttpPipelinedNetworkTransactionTest, ConnectionCloseEvictToNewPipeline) {
  Initialize();

  MockWrite writes[] = {
    MockWrite(false, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(false, 3, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(true, 4, "one.html"),
    MockRead(false, ERR_SOCKET_NOT_CONNECTED, 5),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  MockWrite writes2[] = {
    MockWrite(false, 0, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads2[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(false, 3, "two.html"),
  };
  AddExpectedConnection(reads2, arraysize(reads2), writes2, arraysize(writes2));

  CompleteTwoRequests();
}

TEST_F(HttpPipelinedNetworkTransactionTest, ErrorEvictsToNewPipeline) {
  Initialize();

  MockWrite writes[] = {
    MockWrite(false, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(false, 3, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n\r\n"),
    MockRead(false, ERR_FAILED, 2),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  MockWrite writes2[] = {
    MockWrite(false, 0, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads2[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(false, 3, "two.html"),
  };
  AddExpectedConnection(reads2, arraysize(reads2), writes2, arraysize(writes2));

  HttpNetworkTransaction one_transaction(session_.get());
  TestOldCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction.Start(GetRequestInfo("one.html"), &one_callback,
                                  BoundNetLog()));
  EXPECT_EQ(OK, one_callback.WaitForResult());

  HttpNetworkTransaction two_transaction(session_.get());
  TestOldCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction.Start(GetRequestInfo("two.html"), &two_callback,
                                  BoundNetLog()));

  scoped_refptr<IOBuffer> buffer(new IOBuffer(1));
  EXPECT_EQ(ERR_FAILED, one_transaction.Read(buffer.get(), 1, &callback_));
  EXPECT_EQ(OK, two_callback.WaitForResult());
  ExpectResponse("two.html", two_transaction);
}

TEST_F(HttpPipelinedNetworkTransactionTest, SendErrorEvictsToNewPipeline) {
  Initialize();

  MockWrite writes[] = {
    MockWrite(true, ERR_FAILED, 0),
  };
  AddExpectedConnection(NULL, 0, writes, arraysize(writes));

  MockWrite writes2[] = {
    MockWrite(false, 0, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads2[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(false, 3, "two.html"),
  };
  AddExpectedConnection(reads2, arraysize(reads2), writes2, arraysize(writes2));

  HttpNetworkTransaction one_transaction(session_.get());
  TestOldCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction.Start(GetRequestInfo("one.html"), &one_callback,
                                  BoundNetLog()));

  HttpNetworkTransaction two_transaction(session_.get());
  TestOldCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction.Start(GetRequestInfo("two.html"), &two_callback,
                                  BoundNetLog()));

  data_vector_[0]->RunFor(1);
  EXPECT_EQ(ERR_FAILED, one_callback.WaitForResult());

  EXPECT_EQ(OK, two_callback.WaitForResult());
  ExpectResponse("two.html", two_transaction);
}

TEST_F(HttpPipelinedNetworkTransactionTest, RedirectDrained) {
  Initialize();

  MockWrite writes[] = {
    MockWrite(false, 0, "GET /redirect.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(false, 3, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 302 OK\r\n"),
    MockRead(false, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(true, 4, "redirect"),
    MockRead(false, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(false, 7, "two.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpNetworkTransaction> one_transaction(
      new HttpNetworkTransaction(session_.get()));
  TestOldCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction->Start(GetRequestInfo("redirect.html"),
                                   &one_callback, BoundNetLog()));
  EXPECT_EQ(OK, one_callback.WaitForResult());

  HttpNetworkTransaction two_transaction(session_.get());
  TestOldCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction.Start(GetRequestInfo("two.html"), &two_callback,
                                  BoundNetLog()));

  one_transaction.reset();
  data_vector_[0]->RunFor(2);
  data_vector_[0]->SetStop(10);

  EXPECT_EQ(OK, two_callback.WaitForResult());
  ExpectResponse("two.html", two_transaction);
}

TEST_F(HttpPipelinedNetworkTransactionTest, BasicHttpAuthentication) {
  Initialize();

  MockWrite writes[] = {
    MockWrite(false, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(false, 5, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: auth_token\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 401 Authentication Required\r\n"),
    MockRead(false, 2, "WWW-Authenticate: Basic realm=\"Secure Area\"\r\n"),
    MockRead(false, 3, "Content-Length: 20\r\n\r\n"),
    MockRead(false, 4, "needs authentication"),
    MockRead(false, 6, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 7, "Content-Length: 8\r\n\r\n"),
    MockRead(false, 8, "one.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  HttpAuthHandlerMock* mock_auth = new HttpAuthHandlerMock;
  std::string challenge_text = "Basic";
  HttpAuth::ChallengeTokenizer challenge(challenge_text.begin(),
                                         challenge_text.end());
  GURL origin("localhost");
  EXPECT_TRUE(mock_auth->InitFromChallenge(&challenge,
                                           HttpAuth::AUTH_SERVER,
                                           origin,
                                           BoundNetLog()));
  auth_handler_factory_->AddMockHandler(mock_auth, HttpAuth::AUTH_SERVER);

  HttpNetworkTransaction transaction(session_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      transaction.Start(GetRequestInfo("one.html"), &callback_, BoundNetLog()));
  EXPECT_EQ(OK, callback_.WaitForResult());

  AuthCredentials credentials(ASCIIToUTF16("user"), ASCIIToUTF16("pass"));
  EXPECT_EQ(OK, transaction.RestartWithAuth(credentials, &callback_));

  ExpectResponse("one.html", transaction);
}

}  // anonymous namespace

}  // namespace net
