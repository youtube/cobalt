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
};

TEST_F(HttpNetworkLayerTest, CreateAndDestroy) {
  MockClientSocketFactory mock_socket_factory;
  MockHostResolver host_resolver;
  CertVerifier cert_verifier;
  scoped_refptr<HttpNetworkSession> network_session(
      new HttpNetworkSession(
          &host_resolver,
          &cert_verifier,
          NULL /* dnsrr_resolver */,
          NULL /* dns_cert_checker */,
          NULL /* ssl_host_info_factory */,
          ProxyService::CreateDirect(),
          &mock_socket_factory,
          new SSLConfigServiceDefaults,
          new SpdySessionPool(NULL),
          NULL,
          NULL,
          NULL));

  HttpNetworkLayer factory(network_session);

  scoped_ptr<HttpTransaction> trans;
  int rv = factory.CreateTransaction(&trans);
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(trans.get() != NULL);
}

TEST_F(HttpNetworkLayerTest, Suspend) {
  MockClientSocketFactory mock_socket_factory;
  MockHostResolver host_resolver;
  CertVerifier cert_verifier;
  scoped_refptr<HttpNetworkSession> network_session(
      new HttpNetworkSession(
          &host_resolver,
          &cert_verifier,
          NULL /* dnsrr_resolver */,
          NULL /* dns_cert_checker */,
          NULL /* ssl_host_info_factory */,
          ProxyService::CreateDirect(),
          &mock_socket_factory,
          new SSLConfigServiceDefaults,
          new SpdySessionPool(NULL),
          NULL,
          NULL,
          NULL));
  HttpNetworkLayer factory(network_session);

  scoped_ptr<HttpTransaction> trans;
  int rv = factory.CreateTransaction(&trans);
  EXPECT_EQ(OK, rv);

  trans.reset();

  factory.Suspend(true);

  rv = factory.CreateTransaction(&trans);
  EXPECT_EQ(ERR_NETWORK_IO_SUSPENDED, rv);

  ASSERT_TRUE(trans == NULL);

  factory.Suspend(false);

  rv = factory.CreateTransaction(&trans);
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkLayerTest, GET) {
  MockClientSocketFactory mock_socket_factory;
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
  mock_socket_factory.AddSocketDataProvider(&data);

  MockHostResolver host_resolver;
  CertVerifier cert_verifier;
  scoped_refptr<HttpNetworkSession> network_session(
      new HttpNetworkSession(
          &host_resolver,
          &cert_verifier,
          NULL /* dnsrr_resolver */,
          NULL /* dns_cert_checker */,
          NULL /* ssl_host_info_factory */,
          ProxyService::CreateDirect(),
          &mock_socket_factory,
          new SSLConfigServiceDefaults,
          new SpdySessionPool(NULL),
          NULL,
          NULL,
          NULL));
  HttpNetworkLayer factory(network_session);

  TestCompletionCallback callback;

  scoped_ptr<HttpTransaction> trans;
  int rv = factory.CreateTransaction(&trans);
  EXPECT_EQ(OK, rv);

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                       "Foo/1.0");
  request_info.load_flags = LOAD_NORMAL;

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
