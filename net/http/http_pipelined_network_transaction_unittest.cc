// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
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
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrEq;

namespace net {

namespace {

class SimpleProxyConfigService : public ProxyConfigService {
 public:
  virtual void AddObserver(Observer* observer) override {
    observer_ = observer;
  }

  virtual void RemoveObserver(Observer* observer) override {
    if (observer_ == observer) {
      observer_ = NULL;
    }
  }

  virtual ConfigAvailability GetLatestProxyConfig(
      ProxyConfig* config) override {
    *config = config_;
    return CONFIG_VALID;
  }

  void IncrementConfigId() {
    config_.set_id(config_.id() + 1);
    observer_->OnProxyConfigChanged(config_, ProxyConfigService::CONFIG_VALID);
  }

 private:
  ProxyConfig config_;
  Observer* observer_;
};

class HttpPipelinedNetworkTransactionTest : public testing::Test {
 public:
  HttpPipelinedNetworkTransactionTest()
      : histograms_("a"),
        pool_(1, 1, &histograms_, &factory_) {
  }

  void Initialize(bool force_http_pipelining) {
    // Normally, this code could just go in SetUp(). For a few of these tests,
    // we change the default number of sockets per group. That needs to be done
    // before we construct the HttpNetworkSession.
    proxy_config_service_ = new SimpleProxyConfigService();
    proxy_service_.reset(new ProxyService(proxy_config_service_, NULL, NULL));
    ssl_config_ = new SSLConfigServiceDefaults;
    auth_handler_factory_.reset(new HttpAuthHandlerMock::Factory());

    HttpNetworkSession::Params session_params;
    session_params.client_socket_factory = &factory_;
    session_params.proxy_service = proxy_service_.get();
    session_params.host_resolver = &mock_resolver_;
    session_params.ssl_config_service = ssl_config_.get();
    session_params.http_auth_handler_factory = auth_handler_factory_.get();
    session_params.http_server_properties = &http_server_properties_;
    session_params.force_http_pipelining = force_http_pipelining;
    session_params.http_pipelining_enabled = true;
    session_ = new HttpNetworkSession(session_params);
  }

  void AddExpectedConnection(MockRead* reads, size_t reads_count,
                             MockWrite* writes, size_t writes_count) {
    DeterministicSocketData* data = new DeterministicSocketData(
        reads, reads_count, writes, writes_count);
    data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
    if (reads_count || writes_count) {
      data->StopAfter(reads_count + writes_count);
    }
    factory_.AddSocketDataProvider(data);
    data_vector_.push_back(data);
  }

  enum RequestInfoOptions {
    REQUEST_DEFAULT,
    REQUEST_MAIN_RESOURCE,
  };

  HttpRequestInfo* GetRequestInfo(
      const char* filename, RequestInfoOptions options = REQUEST_DEFAULT) {
    std::string url = StringPrintf("http://localhost/%s", filename);
    HttpRequestInfo* request_info = new HttpRequestInfo;
    request_info->url = GURL(url);
    request_info->method = "GET";
    if (options == REQUEST_MAIN_RESOURCE) {
      request_info->load_flags = LOAD_MAIN_FRAME;
    }
    request_info_vector_.push_back(request_info);
    return request_info;
  }

  void ExpectResponse(const std::string& expected,
                      HttpNetworkTransaction& transaction,
                      IoMode io_mode) {
    scoped_refptr<IOBuffer> buffer(new IOBuffer(expected.size()));
    if (io_mode == ASYNC) {
      EXPECT_EQ(ERR_IO_PENDING, transaction.Read(buffer.get(), expected.size(),
                                                 callback_.callback()));
      data_vector_[0]->RunFor(1);
      EXPECT_EQ(static_cast<int>(expected.length()), callback_.WaitForResult());
    } else {
      EXPECT_EQ(static_cast<int>(expected.size()),
                transaction.Read(buffer.get(), expected.size(),
                                 callback_.callback()));
    }
    std::string actual(buffer->data(), expected.size());
    EXPECT_THAT(actual, StrEq(expected));
    EXPECT_EQ(OK, transaction.Read(buffer.get(), expected.size(),
                                   callback_.callback()));
  }

  void CompleteTwoRequests(int data_index, int stop_at_step) {
    scoped_ptr<HttpNetworkTransaction> one_transaction(
        new HttpNetworkTransaction(session_.get()));
    TestCompletionCallback one_callback;
    EXPECT_EQ(ERR_IO_PENDING,
              one_transaction->Start(GetRequestInfo("one.html"),
                                     one_callback.callback(), BoundNetLog()));
    EXPECT_EQ(OK, one_callback.WaitForResult());

    HttpNetworkTransaction two_transaction(session_.get());
    TestCompletionCallback two_callback;
    EXPECT_EQ(ERR_IO_PENDING,
              two_transaction.Start(GetRequestInfo("two.html"),
                                    two_callback.callback(), BoundNetLog()));

    TestCompletionCallback one_read_callback;
    scoped_refptr<IOBuffer> buffer(new IOBuffer(8));
    EXPECT_EQ(ERR_IO_PENDING,
              one_transaction->Read(buffer.get(), 8,
                                    one_read_callback.callback()));

    data_vector_[data_index]->SetStop(stop_at_step);
    data_vector_[data_index]->Run();
    EXPECT_EQ(8, one_read_callback.WaitForResult());
    data_vector_[data_index]->SetStop(10);
    std::string actual(buffer->data(), 8);
    EXPECT_THAT(actual, StrEq("one.html"));
    EXPECT_EQ(OK, one_transaction->Read(buffer.get(), 8,
                                        one_read_callback.callback()));

    EXPECT_EQ(OK, two_callback.WaitForResult());
    ExpectResponse("two.html", two_transaction, SYNCHRONOUS);
  }

  void CompleteFourRequests(RequestInfoOptions options) {
    scoped_ptr<HttpNetworkTransaction> one_transaction(
        new HttpNetworkTransaction(session_.get()));
    TestCompletionCallback one_callback;
    EXPECT_EQ(ERR_IO_PENDING,
              one_transaction->Start(GetRequestInfo("one.html", options),
                                     one_callback.callback(), BoundNetLog()));
    EXPECT_EQ(OK, one_callback.WaitForResult());

    HttpNetworkTransaction two_transaction(session_.get());
    TestCompletionCallback two_callback;
    EXPECT_EQ(ERR_IO_PENDING,
              two_transaction.Start(GetRequestInfo("two.html", options),
                                    two_callback.callback(), BoundNetLog()));

    HttpNetworkTransaction three_transaction(session_.get());
    TestCompletionCallback three_callback;
    EXPECT_EQ(ERR_IO_PENDING,
              three_transaction.Start(GetRequestInfo("three.html", options),
                                      three_callback.callback(),
                                      BoundNetLog()));

    HttpNetworkTransaction four_transaction(session_.get());
    TestCompletionCallback four_callback;
    EXPECT_EQ(ERR_IO_PENDING,
              four_transaction.Start(GetRequestInfo("four.html", options),
                                     four_callback.callback(), BoundNetLog()));

    ExpectResponse("one.html", *one_transaction.get(), SYNCHRONOUS);
    EXPECT_EQ(OK, two_callback.WaitForResult());
    ExpectResponse("two.html", two_transaction, SYNCHRONOUS);
    EXPECT_EQ(OK, three_callback.WaitForResult());
    ExpectResponse("three.html", three_transaction, SYNCHRONOUS);

    one_transaction.reset();
    EXPECT_EQ(OK, four_callback.WaitForResult());
    ExpectResponse("four.html", four_transaction, SYNCHRONOUS);
  }

  DeterministicMockClientSocketFactory factory_;
  ClientSocketPoolHistograms histograms_;
  MockTransportClientSocketPool pool_;
  ScopedVector<DeterministicSocketData> data_vector_;
  TestCompletionCallback callback_;
  ScopedVector<HttpRequestInfo> request_info_vector_;

  SimpleProxyConfigService* proxy_config_service_;
  scoped_ptr<ProxyService> proxy_service_;
  MockHostResolver mock_resolver_;
  scoped_refptr<SSLConfigService> ssl_config_;
  scoped_ptr<HttpAuthHandlerMock::Factory> auth_handler_factory_;
  HttpServerPropertiesImpl http_server_properties_;
  scoped_refptr<HttpNetworkSession> session_;
};

TEST_F(HttpPipelinedNetworkTransactionTest, OneRequest) {
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /test.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 9\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "test.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  HttpNetworkTransaction transaction(session_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            transaction.Start(GetRequestInfo("test.html"), callback_.callback(),
                              BoundNetLog()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  ExpectResponse("test.html", transaction, SYNCHRONOUS);
}

TEST_F(HttpPipelinedNetworkTransactionTest, ReusePipeline) {
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 3, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(ASYNC, 4, "one.html"),
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 7, "two.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  CompleteTwoRequests(0, 5);
}

TEST_F(HttpPipelinedNetworkTransactionTest, ReusesOnSpaceAvailable) {
  int old_max_sockets = ClientSocketPoolManager::max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL);
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 4, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 7, "GET /three.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 12, "GET /four.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "one.html"),
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 8, "two.html"),
    MockRead(SYNCHRONOUS, 9, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 10, "Content-Length: 10\r\n\r\n"),
    MockRead(SYNCHRONOUS, 11, "three.html"),
    MockRead(SYNCHRONOUS, 13, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 14, "Content-Length: 9\r\n\r\n"),
    MockRead(SYNCHRONOUS, 15, "four.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  CompleteFourRequests(REQUEST_DEFAULT);

  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, old_max_sockets);
}

TEST_F(HttpPipelinedNetworkTransactionTest, WontPipelineMainResource) {
  int old_max_sockets = ClientSocketPoolManager::max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL);
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 4, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 8, "GET /three.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 12, "GET /four.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "one.html"),
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 7, "two.html"),
    MockRead(SYNCHRONOUS, 9, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 10, "Content-Length: 10\r\n\r\n"),
    MockRead(SYNCHRONOUS, 11, "three.html"),
    MockRead(SYNCHRONOUS, 13, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 14, "Content-Length: 9\r\n\r\n"),
    MockRead(SYNCHRONOUS, 15, "four.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  CompleteFourRequests(REQUEST_MAIN_RESOURCE);

  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, old_max_sockets);
}

TEST_F(HttpPipelinedNetworkTransactionTest, UnknownSizeEvictsToNewPipeline) {
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n\r\n"),
    MockRead(ASYNC, 2, "one.html"),
    MockRead(SYNCHRONOUS, OK, 3),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  MockWrite writes2[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads2[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "two.html"),
  };
  AddExpectedConnection(reads2, arraysize(reads2), writes2, arraysize(writes2));

  CompleteTwoRequests(0, 3);
}

TEST_F(HttpPipelinedNetworkTransactionTest, ConnectionCloseEvictToNewPipeline) {
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 3, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(ASYNC, 4, "one.html"),
    MockRead(SYNCHRONOUS, ERR_SOCKET_NOT_CONNECTED, 5),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  MockWrite writes2[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads2[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "two.html"),
  };
  AddExpectedConnection(reads2, arraysize(reads2), writes2, arraysize(writes2));

  CompleteTwoRequests(0, 5);
}

TEST_F(HttpPipelinedNetworkTransactionTest, ErrorEvictsToNewPipeline) {
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 3, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_FAILED, 2),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  MockWrite writes2[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads2[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "two.html"),
  };
  AddExpectedConnection(reads2, arraysize(reads2), writes2, arraysize(writes2));

  HttpNetworkTransaction one_transaction(session_.get());
  TestCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction.Start(GetRequestInfo("one.html"),
                                  one_callback.callback(), BoundNetLog()));
  EXPECT_EQ(OK, one_callback.WaitForResult());

  HttpNetworkTransaction two_transaction(session_.get());
  TestCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction.Start(GetRequestInfo("two.html"),
                                  two_callback.callback(), BoundNetLog()));

  scoped_refptr<IOBuffer> buffer(new IOBuffer(1));
  EXPECT_EQ(ERR_FAILED,
            one_transaction.Read(buffer.get(), 1, callback_.callback()));
  EXPECT_EQ(OK, two_callback.WaitForResult());
  ExpectResponse("two.html", two_transaction, SYNCHRONOUS);
}

TEST_F(HttpPipelinedNetworkTransactionTest, SendErrorEvictsToNewPipeline) {
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(ASYNC, ERR_FAILED, 0),
  };
  AddExpectedConnection(NULL, 0, writes, arraysize(writes));

  MockWrite writes2[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads2[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "two.html"),
  };
  AddExpectedConnection(reads2, arraysize(reads2), writes2, arraysize(writes2));

  HttpNetworkTransaction one_transaction(session_.get());
  TestCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction.Start(GetRequestInfo("one.html"),
                                  one_callback.callback(), BoundNetLog()));

  HttpNetworkTransaction two_transaction(session_.get());
  TestCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction.Start(GetRequestInfo("two.html"),
                                  two_callback.callback(), BoundNetLog()));

  data_vector_[0]->RunFor(1);
  EXPECT_EQ(ERR_FAILED, one_callback.WaitForResult());

  EXPECT_EQ(OK, two_callback.WaitForResult());
  ExpectResponse("two.html", two_transaction, SYNCHRONOUS);
}

TEST_F(HttpPipelinedNetworkTransactionTest, RedirectDrained) {
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /redirect.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 3, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 302 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(ASYNC, 4, "redirect"),
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 7, "two.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpNetworkTransaction> one_transaction(
      new HttpNetworkTransaction(session_.get()));
  TestCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction->Start(GetRequestInfo("redirect.html"),
                                   one_callback.callback(), BoundNetLog()));
  EXPECT_EQ(OK, one_callback.WaitForResult());

  HttpNetworkTransaction two_transaction(session_.get());
  TestCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction.Start(GetRequestInfo("two.html"),
                                  two_callback.callback(), BoundNetLog()));

  one_transaction.reset();
  data_vector_[0]->RunFor(2);
  data_vector_[0]->SetStop(10);

  EXPECT_EQ(OK, two_callback.WaitForResult());
  ExpectResponse("two.html", two_transaction, SYNCHRONOUS);
}

TEST_F(HttpPipelinedNetworkTransactionTest, BasicHttpAuthentication) {
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 5, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: auth_token\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 401 Authentication Required\r\n"),
    MockRead(SYNCHRONOUS, 2,
             "WWW-Authenticate: Basic realm=\"Secure Area\"\r\n"),
    MockRead(SYNCHRONOUS, 3, "Content-Length: 20\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "needs authentication"),
    MockRead(SYNCHRONOUS, 6, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 7, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 8, "one.html"),
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
  EXPECT_EQ(ERR_IO_PENDING,
            transaction.Start(GetRequestInfo("one.html"), callback_.callback(),
                              BoundNetLog()));
  EXPECT_EQ(OK, callback_.WaitForResult());

  AuthCredentials credentials(ASCIIToUTF16("user"), ASCIIToUTF16("pass"));
  EXPECT_EQ(OK, transaction.RestartWithAuth(credentials, callback_.callback()));

  ExpectResponse("one.html", transaction, SYNCHRONOUS);
}

TEST_F(HttpPipelinedNetworkTransactionTest, OldVersionDisablesPipelining) {
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /pipelined.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.0 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 14\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "pipelined.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  MockWrite writes2[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads2[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(ASYNC, 3, "one.html"),
    MockRead(SYNCHRONOUS, OK, 4),
  };
  AddExpectedConnection(reads2, arraysize(reads2), writes2, arraysize(writes2));

  MockWrite writes3[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads3[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "two.html"),
    MockRead(SYNCHRONOUS, OK, 4),
  };
  AddExpectedConnection(reads3, arraysize(reads3), writes3, arraysize(writes3));

  HttpNetworkTransaction one_transaction(session_.get());
  TestCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction.Start(GetRequestInfo("pipelined.html"),
                                  one_callback.callback(), BoundNetLog()));
  EXPECT_EQ(OK, one_callback.WaitForResult());
  ExpectResponse("pipelined.html", one_transaction, SYNCHRONOUS);

  CompleteTwoRequests(1, 4);
}

TEST_F(HttpPipelinedNetworkTransactionTest, PipelinesImmediatelyIfKnownGood) {
  // The first request gets us an HTTP/1.1. The next 3 test pipelining. When the
  // 3rd request completes, we know pipelining is safe. After the first 4
  // complete, the 5th and 6th should then be immediately sent pipelined on a
  // new HttpPipelinedConnection.
  int old_max_sockets = ClientSocketPoolManager::max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL);
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 4, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 7, "GET /three.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 12, "GET /four.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 16, "GET /second-pipeline-one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 17, "GET /second-pipeline-two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "one.html"),
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 8, "two.html"),
    MockRead(SYNCHRONOUS, 9, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 10, "Content-Length: 10\r\n\r\n"),
    MockRead(SYNCHRONOUS, 11, "three.html"),
    MockRead(SYNCHRONOUS, 13, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 14, "Content-Length: 9\r\n\r\n"),
    MockRead(SYNCHRONOUS, 15, "four.html"),
    MockRead(ASYNC, 18, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 19, "Content-Length: 24\r\n\r\n"),
    MockRead(SYNCHRONOUS, 20, "second-pipeline-one.html"),
    MockRead(SYNCHRONOUS, 21, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 22, "Content-Length: 24\r\n\r\n"),
    MockRead(SYNCHRONOUS, 23, "second-pipeline-two.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  CompleteFourRequests(REQUEST_DEFAULT);

  HttpNetworkTransaction second_one_transaction(session_.get());
  TestCompletionCallback second_one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            second_one_transaction.Start(
                GetRequestInfo("second-pipeline-one.html"),
                second_one_callback.callback(), BoundNetLog()));
  MessageLoop::current()->RunUntilIdle();

  HttpNetworkTransaction second_two_transaction(session_.get());
  TestCompletionCallback second_two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            second_two_transaction.Start(
                GetRequestInfo("second-pipeline-two.html"),
                second_two_callback.callback(), BoundNetLog()));

  data_vector_[0]->RunFor(3);
  EXPECT_EQ(OK, second_one_callback.WaitForResult());
  data_vector_[0]->StopAfter(100);
  ExpectResponse("second-pipeline-one.html", second_one_transaction,
                 SYNCHRONOUS);
  EXPECT_EQ(OK, second_two_callback.WaitForResult());
  ExpectResponse("second-pipeline-two.html", second_two_transaction,
                 SYNCHRONOUS);

  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, old_max_sockets);
}

class DataRunnerObserver : public MessageLoop::TaskObserver {
 public:
  DataRunnerObserver(DeterministicSocketData* data, int run_before_task)
      : data_(data),
        run_before_task_(run_before_task),
        current_task_(0) { }

  virtual void WillProcessTask(base::TimeTicks) override {
    ++current_task_;
    if (current_task_ == run_before_task_) {
      data_->Run();
      MessageLoop::current()->RemoveTaskObserver(this);
    }
  }

  virtual void DidProcessTask(base::TimeTicks) override { }

 private:
  DeterministicSocketData* data_;
  int run_before_task_;
  int current_task_;
};

TEST_F(HttpPipelinedNetworkTransactionTest, OpenPipelinesWhileBinding) {
  // There was a racy crash in the pipelining code. This test recreates that
  // race. The steps are:
  // 1. The first request starts a pipeline and requests headers.
  // 2. HttpStreamFactoryImpl::Job tries to bind a pending request to a new
  // pipeline and queues a task to do so.
  // 3. Before that task runs, the first request receives its headers and
  // determines this host is probably capable of pipelining.
  // 4. All of the hosts' pipelines are notified they have capacity in a loop.
  // 5. On the first iteration, the first pipeline is opened up to accept new
  // requests and steals the request from step #2.
  // 6. The pipeline from #2 is deleted because it has no streams.
  // 7. On the second iteration, the host tries to notify the pipeline from step
  // #2 that it has capacity. This is a use-after-free.
  Initialize(false);

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(ASYNC, 3, "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "one.html"),
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, 7, "two.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  AddExpectedConnection(NULL, 0, NULL, 0);

  HttpNetworkTransaction one_transaction(session_.get());
  TestCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction.Start(GetRequestInfo("one.html"),
                                  one_callback.callback(), BoundNetLog()));

  data_vector_[0]->SetStop(2);
  data_vector_[0]->Run();

  HttpNetworkTransaction two_transaction(session_.get());
  TestCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction.Start(GetRequestInfo("two.html"),
                                  two_callback.callback(), BoundNetLog()));
  // Posted tasks should be:
  // 1. MockHostResolverBase::ResolveNow
  // 2. HttpStreamFactoryImpl::Job::OnStreamReadyCallback for job 1
  // 3. HttpStreamFactoryImpl::Job::OnStreamReadyCallback for job 2
  //
  // We need to make sure that the response that triggers OnPipelineFeedback(OK)
  // is called in between when task #3 is scheduled and when it runs. The
  // DataRunnerObserver does that.
  DataRunnerObserver observer(data_vector_[0], 3);
  MessageLoop::current()->AddTaskObserver(&observer);
  data_vector_[0]->SetStop(4);
  MessageLoop::current()->RunUntilIdle();
  data_vector_[0]->SetStop(10);

  EXPECT_EQ(OK, one_callback.WaitForResult());
  ExpectResponse("one.html", one_transaction, SYNCHRONOUS);
  EXPECT_EQ(OK, two_callback.WaitForResult());
  ExpectResponse("two.html", two_transaction, SYNCHRONOUS);
}

TEST_F(HttpPipelinedNetworkTransactionTest, ProxyChangesWhileConnecting) {
  Initialize(false);

  DeterministicSocketData data(NULL, 0, NULL, 0);
  data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_REFUSED));
  factory_.AddSocketDataProvider(&data);

  DeterministicSocketData data2(NULL, 0, NULL, 0);
  data2.set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  factory_.AddSocketDataProvider(&data2);

  HttpNetworkTransaction transaction(session_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            transaction.Start(GetRequestInfo("test.html"), callback_.callback(),
                              BoundNetLog()));

  proxy_config_service_->IncrementConfigId();

  EXPECT_EQ(ERR_FAILED, callback_.WaitForResult());
}

TEST_F(HttpPipelinedNetworkTransactionTest, ForcedPipelineSharesConnection) {
  Initialize(true);

  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"
              "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 8\r\n\r\n"),
    MockRead(ASYNC, 3, "one.html"),
    MockRead(ASYNC, 4, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 5, "Content-Length: 8\r\n\r\n"),
    MockRead(ASYNC, 6, "two.html"),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpNetworkTransaction> one_transaction(
      new HttpNetworkTransaction(session_.get()));
  TestCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction->Start(GetRequestInfo("one.html"),
                                   one_callback.callback(), BoundNetLog()));

  HttpNetworkTransaction two_transaction(session_.get());
  TestCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction.Start(GetRequestInfo("two.html"),
                                  two_callback.callback(), BoundNetLog()));

  data_vector_[0]->RunFor(3);  // Send + 2 lines of headers.
  EXPECT_EQ(OK, one_callback.WaitForResult());
  ExpectResponse("one.html", *one_transaction.get(), ASYNC);
  one_transaction.reset();

  data_vector_[0]->RunFor(2);  // 2 lines of headers.
  EXPECT_EQ(OK, two_callback.WaitForResult());
  ExpectResponse("two.html", two_transaction, ASYNC);
}

TEST_F(HttpPipelinedNetworkTransactionTest,
       ForcedPipelineConnectionErrorFailsBoth) {
  Initialize(true);

  DeterministicSocketData data(NULL, 0, NULL, 0);
  data.set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  factory_.AddSocketDataProvider(&data);

  scoped_ptr<HttpNetworkTransaction> one_transaction(
      new HttpNetworkTransaction(session_.get()));
  TestCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction->Start(GetRequestInfo("one.html"),
                                   one_callback.callback(), BoundNetLog()));

  HttpNetworkTransaction two_transaction(session_.get());
  TestCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction.Start(GetRequestInfo("two.html"),
                                  two_callback.callback(), BoundNetLog()));

  data.Run();
  EXPECT_EQ(ERR_FAILED, one_callback.WaitForResult());
  EXPECT_EQ(ERR_FAILED, two_callback.WaitForResult());
}

TEST_F(HttpPipelinedNetworkTransactionTest, ForcedPipelineEvictionIsFatal) {
  Initialize(true);

  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"
              "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, ERR_FAILED, 1),
  };
  AddExpectedConnection(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpNetworkTransaction> one_transaction(
      new HttpNetworkTransaction(session_.get()));
  TestCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction->Start(GetRequestInfo("one.html"),
                                   one_callback.callback(), BoundNetLog()));

  HttpNetworkTransaction two_transaction(session_.get());
  TestCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction.Start(GetRequestInfo("two.html"),
                                  two_callback.callback(), BoundNetLog()));

  data_vector_[0]->RunFor(2);
  EXPECT_EQ(ERR_FAILED, one_callback.WaitForResult());
  one_transaction.reset();
  EXPECT_EQ(ERR_PIPELINE_EVICTION, two_callback.WaitForResult());
}

TEST_F(HttpPipelinedNetworkTransactionTest, ForcedPipelineOrder) {
  Initialize(true);

  MockWrite writes[] = {
    MockWrite(ASYNC, 0,
              "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"
              "GET /two.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"
              "GET /three.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"
              "GET /four.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Connection: keep-alive\r\n\r\n"
              ),
  };
  MockRead reads[] = {
    MockRead(ASYNC, ERR_FAILED, 1),
  };
  DeterministicSocketData data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(MockConnect(ASYNC, OK));
  factory_.AddSocketDataProvider(&data);

  scoped_ptr<HttpNetworkTransaction> one_transaction(
      new HttpNetworkTransaction(session_.get()));
  TestCompletionCallback one_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            one_transaction->Start(GetRequestInfo("one.html"),
                                   one_callback.callback(), BoundNetLog()));

  scoped_ptr<HttpNetworkTransaction> two_transaction(
      new HttpNetworkTransaction(session_.get()));
  TestCompletionCallback two_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            two_transaction->Start(GetRequestInfo("two.html"),
                                   two_callback.callback(), BoundNetLog()));

  scoped_ptr<HttpNetworkTransaction> three_transaction(
      new HttpNetworkTransaction(session_.get()));
  TestCompletionCallback three_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            three_transaction->Start(GetRequestInfo("three.html"),
                                     three_callback.callback(), BoundNetLog()));

  scoped_ptr<HttpNetworkTransaction> four_transaction(
      new HttpNetworkTransaction(session_.get()));
  TestCompletionCallback four_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            four_transaction->Start(GetRequestInfo("four.html"),
                                    four_callback.callback(), BoundNetLog()));

  data.RunFor(3);
  EXPECT_EQ(ERR_FAILED, one_callback.WaitForResult());
  one_transaction.reset();
  EXPECT_EQ(ERR_PIPELINE_EVICTION, two_callback.WaitForResult());
  two_transaction.reset();
  EXPECT_EQ(ERR_PIPELINE_EVICTION, three_callback.WaitForResult());
  three_transaction.reset();
  EXPECT_EQ(ERR_PIPELINE_EVICTION, four_callback.WaitForResult());
}

}  // anonymous namespace

}  // namespace net
