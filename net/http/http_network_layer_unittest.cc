// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include "net/base/mock_cert_verifier.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_log.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
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
  HttpNetworkLayerTest() : ssl_config_service_(new SSLConfigServiceDefaults) {}

  virtual void SetUp() {
    ConfigureTestDependencies(ProxyService::CreateDirect());
  }

  void ConfigureTestDependencies(ProxyService* proxy_service) {
    cert_verifier_.reset(new MockCertVerifier);
    proxy_service_.reset(proxy_service);
    HttpNetworkSession::Params session_params;
    session_params.client_socket_factory = &mock_socket_factory_;
    session_params.host_resolver = &host_resolver_;
    session_params.cert_verifier = cert_verifier_.get();
    session_params.proxy_service = proxy_service_.get();
    session_params.ssl_config_service = ssl_config_service_;
    session_params.http_server_properties = &http_server_properties_;
    network_session_ = new HttpNetworkSession(session_params);
    factory_.reset(new HttpNetworkLayer(network_session_));
  }

  MockClientSocketFactory mock_socket_factory_;
  MockHostResolver host_resolver_;
  scoped_ptr<CertVerifier> cert_verifier_;
  scoped_ptr<ProxyService> proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;
  scoped_refptr<HttpNetworkSession> network_session_;
  scoped_ptr<HttpNetworkLayer> factory_;
  HttpServerPropertiesImpl http_server_properties_;
};

TEST_F(HttpNetworkLayerTest, CreateAndDestroy) {
  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(&trans, NULL);
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(trans.get() != NULL);
}

TEST_F(HttpNetworkLayerTest, Suspend) {
  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(&trans, NULL);
  EXPECT_EQ(OK, rv);

  trans.reset();

  factory_->OnSuspend();

  rv = factory_->CreateTransaction(&trans, NULL);
  EXPECT_EQ(ERR_NETWORK_IO_SUSPENDED, rv);

  ASSERT_TRUE(trans == NULL);

  factory_->OnResume();

  rv = factory_->CreateTransaction(&trans, NULL);
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkLayerTest, GET) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
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
  int rv = factory_->CreateTransaction(&trans, NULL);
  EXPECT_EQ(OK, rv);

  rv = trans->Start(&request_info, callback.callback(), BoundNetLog());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(OK, rv);

  std::string contents;
  rv = ReadTransaction(trans.get(), &contents);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", contents);
}

TEST_F(HttpNetworkLayerTest, ServerFallback) {
  // Verify that a Connection: Proxy-Bypass header induces proxy fallback to
  // a second proxy, if configured.

  // To configure this test, we need to wire up a custom proxy service to use
  // a pair of proxies. We'll induce fallback via the first and return
  // the expected data via the second.
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY bad:8080; PROXY good:8080"));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Connection: proxy-bypass\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite data_writes[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  StaticSocketDataProvider data1(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  mock_socket_factory_.AddSocketDataProvider(&data1);

  // Second data provider returns the expected content.
  MockRead data_reads2[] = {
    MockRead("HTTP/1.0 200 OK\r\n"
             "Server: not-proxy\r\n\r\n"),
    MockRead("content"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite data_writes2[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  StaticSocketDataProvider data2(data_reads2, arraysize(data_reads2),
                                data_writes2, arraysize(data_writes2));
  mock_socket_factory_.AddSocketDataProvider(&data2);

  TestCompletionCallback callback;

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.load_flags = LOAD_NORMAL;

  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(&trans, NULL);
  EXPECT_EQ(OK, rv);

  rv = trans->Start(&request_info, callback.callback(), BoundNetLog());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(OK, rv);

  std::string contents;
  rv = ReadTransaction(trans.get(), &contents);
  EXPECT_EQ(OK, rv);

  // We should obtain content from the second socket provider write
  // corresponding to the fallback proxy.
  EXPECT_EQ("content", contents);
  // We also have a server header here that isn't set by the proxy.
  EXPECT_TRUE(trans->GetResponseInfo()->headers->HasHeaderValue(
      "server", "not-proxy"));
  // We should also observe the bad proxy in the retry list.
  ASSERT_TRUE(1u == proxy_service_->proxy_retry_info().size());
  EXPECT_EQ("bad:8080", (*proxy_service_->proxy_retry_info().begin()).first);
}

TEST_F(HttpNetworkLayerTest, ServerFallbackDoesntLoop) {
  // Verify that a Connection: Proxy-Bypass header will display the original
  // proxy's error page content if a fallback option is not configured.
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult(
      "PROXY bad:8080; PROXY alsobad:8080"));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Connection: proxy-bypass\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite data_writes[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  StaticSocketDataProvider data1(data_reads, arraysize(data_reads),
                                 data_writes, arraysize(data_writes));
  StaticSocketDataProvider data2(data_reads, arraysize(data_reads),
                                 data_writes, arraysize(data_writes));
  mock_socket_factory_.AddSocketDataProvider(&data1);
  mock_socket_factory_.AddSocketDataProvider(&data2);

  TestCompletionCallback callback;

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.load_flags = LOAD_NORMAL;

  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(&trans, NULL);
  EXPECT_EQ(OK, rv);

  rv = trans->Start(&request_info, callback.callback(), BoundNetLog());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(OK, rv);

  std::string contents;
  rv = ReadTransaction(trans.get(), &contents);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("Bypass message", contents);

  // Despite not falling back to anything, we should still observe the proxies
  // in the bad proxies list.
  const ProxyRetryInfoMap& retry_info = proxy_service_->proxy_retry_info();
  ASSERT_EQ(2u, retry_info.size());
  ASSERT_TRUE(retry_info.find("bad:8080") != retry_info.end());
  ASSERT_TRUE(retry_info.find("alsobad:8080") != retry_info.end());
}

TEST_F(HttpNetworkLayerTest, ProxyBypassIgnoredOnDirectConnection) {
  // Verify that a Connection: proxy-bypass header is ignored when returned
  // from a directly connected origin server.
  ConfigureTestDependencies(ProxyService::CreateFixedFromPacResult("DIRECT"));

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Connection: proxy-bypass\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };
  StaticSocketDataProvider data1(data_reads, arraysize(data_reads),
                                 data_writes, arraysize(data_writes));
  mock_socket_factory_.AddSocketDataProvider(&data1);
  TestCompletionCallback callback;

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.load_flags = LOAD_NORMAL;

  scoped_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(&trans, NULL);
  EXPECT_EQ(OK, rv);

  rv = trans->Start(&request_info, callback.callback(), BoundNetLog());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(OK, rv);

  // We should have read the original page data.
  std::string contents;
  rv = ReadTransaction(trans.get(), &contents);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("Bypass message", contents);

  // We should have no entries in our bad proxy list.
  ASSERT_EQ(0u, proxy_service_->proxy_retry_info().size());
}

}  // namespace

}  // namespace net
