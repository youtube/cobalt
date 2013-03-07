// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dial_http_server.h"

#include "base/message_loop.h"
#include "net/base/load_flags.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Test fixture
class DialHttpServerTest : public testing::Test {
 protected:
  scoped_refptr<DialHttpServer> server_;
  IPEndPoint addr_;
  scoped_refptr<HttpNetworkSession> session_;
  scoped_ptr<HttpNetworkTransaction> client_;

  virtual void SetUp() OVERRIDE {
    server_ = new DialHttpServer();
    server_->Start();
    EXPECT_EQ(OK, server_->GetLocalAddress(&addr_));
    EXPECT_NE(0, addr_.port());

    HttpNetworkSession::Params params;
    params.proxy_service = ProxyService::CreateDirect();
    params.ssl_config_service = new SSLConfigServiceDefaults();
    params.http_server_properties = new HttpServerPropertiesImpl();
    params.host_resolver = new MockHostResolver();
    session_ = new HttpNetworkSession(params);
    client_.reset(new HttpNetworkTransaction(session_));
  }

  virtual void TearDown() OVERRIDE {
    server_->Stop();
  }

  const HttpResponseInfo* GetResponse(const HttpRequestInfo& req) {
    TestCompletionCallback callback;
    int rv = client_->Start(&req, callback.callback(), BoundNetLog());

    if (rv == ERR_IO_PENDING) {
      callback.WaitForResult();
    }
    return client_->GetResponseInfo();
  }
};

TEST_F(DialHttpServerTest, SendManifest) {
  HttpRequestInfo req;
  req.url = GURL("http://" + addr_.ToString() + "/dd.xml");
  req.method = "GET";
  req.load_flags = LOAD_BYPASS_PROXY | LOAD_DISABLE_CACHE;

  const HttpResponseInfo* resp = GetResponse(req);
  std::string store;
  EXPECT_EQ(HTTP_OK, resp->headers->response_code());

  EXPECT_TRUE(resp->headers->GetMimeType(&store));
  EXPECT_EQ("text/xml", store);
  EXPECT_TRUE(resp->headers->GetCharset(&store));
  EXPECT_EQ("utf-8", store);

  EXPECT_TRUE(resp->headers->HasHeaderValue("APPLICATION-URL",
                                            server_->application_url()));

  int64 content_length = resp->headers->GetContentLength();
  ASSERT_NE(0, content_length); // if failed, no point continuing.

  scoped_refptr<IOBuffer> buffer(new IOBuffer(content_length));
  TestCompletionCallback callback;
  int rv = client_->Read(buffer, content_length, callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(content_length, rv);
}

TEST_F(DialHttpServerTest, AllOtherRequests) {
  struct TestStruct {
    const char* method;
    const char* path;
    const int response_code_received;
  } tests[] = {
    { "GET", "/", HTTP_NOT_FOUND },
    { "GET", "/etc", HTTP_NOT_FOUND },
    { "POST", "/dd.xml", HTTP_NOT_FOUND },
    { "POST", "/apps/YouTube", HTTP_NOT_FOUND },
  };

  for (int i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    HttpRequestInfo req;
    req.method = tests[i].method;
    req.url = GURL("http://" + addr_.ToString() + tests[i].path);
    req.load_flags = LOAD_BYPASS_PROXY | LOAD_DISABLE_CACHE;

    const HttpResponseInfo* resp = GetResponse(req);
    EXPECT_EQ(tests[i].response_code_received, resp->headers->response_code());
  }
}

// Ensure local address is not IFADDR_ANY, we need some IP addresses here!
TEST_F(DialHttpServerTest, GetLocalAddress) {
  EXPECT_STRNE("0.0.0.0", addr_.ToStringWithoutPort().c_str());
}

} // namespace net

