// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_verifier.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_log.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_unittest.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_pool.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

class HttpNetworkLayerTest : public PlatformTest {
 protected:
  HttpNetworkLayerTest()
      : proxy_service_(ProxyService::CreateDirect()),
        ssl_config_service_(new SSLConfigServiceDefaults) {
    HttpNetworkSession::Params session_params;
    session_params.client_socket_factory = &mock_socket_factory_;
    session_params.host_resolver = &host_resolver_;
    session_params.cert_verifier = &cert_verifier_;
    session_params.proxy_service = proxy_service_;
    session_params.ssl_config_service = ssl_config_service_;
    network_session_ = new HttpNetworkSession(session_params);
    factory_.reset(new HttpNetworkLayer(network_session_));
  }

  MockClientSocketFactory mock_socket_factory_;
  MockHostResolver host_resolver_;
  CertVerifier cert_verifier_;
  const scoped_refptr<ProxyService> proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;
  scoped_refptr<HttpNetworkSession> network_session_;
  scoped_ptr<HttpNetworkLayer> factory_;
};

TEST_F(HttpNetworkLayerTest, CreateAndDestroy) {
  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(&trans);
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(trans.get() != NULL);
}

TEST_F(HttpNetworkLayerTest, Suspend) {
  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(&trans);
  EXPECT_EQ(OK, rv);

  trans.reset();

  factory_->Suspend(true);

  rv = factory_->CreateTransaction(&trans);
  EXPECT_EQ(ERR_NETWORK_IO_SUSPENDED, rv);

  ASSERT_TRUE(trans == NULL);

  factory_->Suspend(false);

  rv = factory_->CreateTransaction(&trans);
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkLayerTest, GET) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(false, OK),
  };
  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
                   "Host: www.google.com\r\n"
                   "Connection: keep-alive\r\n"
                   "User-Agent: Foo/1.0\r\n\r\n"),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                     data_writes, arraysize(data_writes));
  mock_socket_factory_.AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                       "Foo/1.0");
  request_info.load_flags = LOAD_NORMAL;

  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(&trans);
  EXPECT_EQ(OK, rv);

  rv = trans->Start(&request_info, &callback, BoundNetLog());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(OK, rv);

  std::string contents;
  rv = ReadTransaction(trans.get(), &contents);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", contents);
}

}  // namespace

}  // namespace net
