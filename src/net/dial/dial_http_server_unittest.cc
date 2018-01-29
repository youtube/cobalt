// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dial_http_server.h"
#include "dial_service_handler.h"

#include "base/string_split.h"
#include "net/base/load_flags.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/dial/dial_test_helpers.h"
#include "net/dial/dial_service.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/client_socket_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;

// Data our mock service handler will send to the dial HTTP server.
struct ResponseData {
  ResponseData() : response_code_(0), succeeded_(false) {}
  std::string response_body_;
  std::string mime_type_;
  std::vector<std::string> headers_;
  int response_code_;
  bool succeeded_;
};

// Test fixture
class DialHttpServerTest : public testing::Test {
 public:
  scoped_ptr<DialService> dial_service_;
  IPEndPoint addr_;
  scoped_refptr<HttpNetworkSession> session_;
  scoped_ptr<HttpNetworkTransaction> client_;
  scoped_refptr<MockServiceHandler> handler_;
  scoped_ptr<ResponseData> test_response_;

  DialHttpServerTest() { handler_ = new MockServiceHandler("Foo"); }

  void InitHttpClientLibrary() {
    HttpNetworkSession::Params params;
    params.proxy_service = ProxyService::CreateDirect();
    params.ssl_config_service = new SSLConfigServiceDefaults();
    params.http_server_properties = new HttpServerPropertiesImpl();
    params.host_resolver = new MockHostResolver();
    session_ = new HttpNetworkSession(params);
    client_.reset(new HttpNetworkTransaction(session_));
  }

  virtual void SetUp() override {
    dial_service_.reset(new DialService());
    dial_service_->Register(handler_);
    EXPECT_EQ(OK, dial_service_->http_server()->GetLocalAddress(&addr_));
    EXPECT_NE(0, addr_.port());
    InitHttpClientLibrary();
  }

  virtual void TearDown() override {
    dial_service_->Deregister(handler_);
    dial_service_.reset(NULL);

    const HttpNetworkSession::Params& params = session_->params();
    delete params.proxy_service;
    delete params.http_server_properties;
    delete params.host_resolver;
  }

  const HttpResponseInfo* GetResponse(const HttpRequestInfo& req) {
    TestCompletionCallback callback;
    int rv = client_->Start(&req, callback.callback(), BoundNetLog());

    if (rv == ERR_IO_PENDING) {
      // FIXME: This call can be flaky: It may wait forever.
      // NOTE: I think flakiness here has been fixed in Starboard, by explicitly
      // waking up the message pump when calling QuitWhenIdle.
      callback.WaitForResult();
    }
    return client_->GetResponseInfo();
  }

  HttpRequestInfo CreateRequest(const std::string& method,
                                const std::string& path) {
    HttpRequestInfo req;
    req.url = GURL("http://" + addr_.ToString() + path);
    req.method = method;
    req.load_flags = LOAD_BYPASS_PROXY | LOAD_DISABLE_CACHE;
    return req;
  }

  void Capture(const std::string& path,
               const HttpServerRequestInfo& request,
               const DialServiceHandler::CompletionCB& on_completion) {
    if (!test_response_->succeeded_) {
      on_completion.Run(scoped_ptr<HttpServerResponseInfo>());
      return;
    }
    // This function simulates a DialServiceHandler response.
    scoped_ptr<HttpServerResponseInfo> response(new HttpServerResponseInfo);
    response->body = test_response_->response_body_;
    response->mime_type = test_response_->mime_type_;
    response->response_code = test_response_->response_code_;
    response->headers = test_response_->headers_;

    on_completion.Run(response.Pass());
  }

  void DoResponseCheck(const HttpResponseInfo* resp, bool has_contents) {
    EXPECT_EQ(test_response_->response_code_, resp->headers->response_code());

    for (std::vector<std::string>::const_iterator it =
             test_response_->headers_.begin();
         it != test_response_->headers_.end(); ++it) {
      std::vector<std::string> result;
      base::SplitString(*it, ':', &result);
      ASSERT_EQ(2, result.size());
      EXPECT_TRUE(resp->headers->HasHeaderValue(result[0], result[1]));
    }

    int64 content_length = resp->headers->GetContentLength();
    if (!has_contents) {
      EXPECT_EQ(0, content_length);
      return;
    }

    ASSERT_NE(0, content_length); // if failed, no point continuing.

    scoped_refptr<IOBuffer> buffer(new IOBuffer(content_length));
    TestCompletionCallback callback;
    int rv = client_->Read(buffer, content_length, callback.callback());
    if (rv == net::ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }

    EXPECT_EQ(test_response_->response_body_.length(), rv);
    EXPECT_EQ(0, strncmp(buffer->data(), test_response_->response_body_.data(),
                         test_response_->response_body_.length()));
  }
};

TEST_F(DialHttpServerTest, SendManifest) {
  HttpRequestInfo req = CreateRequest("GET", "/dd.xml");
  const HttpResponseInfo* resp = GetResponse(req);
  ASSERT_TRUE(resp != NULL);

  std::string store;
  EXPECT_EQ(HTTP_OK, resp->headers->response_code());

  EXPECT_TRUE(resp->headers->GetMimeType(&store));
  EXPECT_EQ("text/xml", store);
  EXPECT_TRUE(resp->headers->GetCharset(&store));
  EXPECT_EQ("utf-8", store);

  EXPECT_TRUE(resp->headers->HasHeaderValue("APPLICATION-URL",
      "http://" + addr_.ToString() + "/apps/"));

  int64 content_length = resp->headers->GetContentLength();
  ASSERT_NE(0, content_length); // if failed, no point continuing.

  scoped_refptr<IOBuffer> buffer(new IOBuffer(content_length));
  TestCompletionCallback callback;
  int rv = client_->Read(buffer, content_length, callback.callback());
  if (rv == net::ERR_IO_PENDING) {
    rv = callback.WaitForResult();
  }
  EXPECT_EQ(content_length, rv);
}

TEST_F(DialHttpServerTest, CurrentRunningAppRedirect) {
  HttpRequestInfo req = CreateRequest("GET", "/apps/");
  const HttpResponseInfo* resp = GetResponse(req);
  ASSERT_TRUE(resp != NULL);

  std::string store;
  EXPECT_EQ(HTTP_FOUND, resp->headers->response_code());

  EXPECT_FALSE(resp->headers->GetMimeType(&store));
  EXPECT_FALSE(resp->headers->GetCharset(&store));

  EXPECT_TRUE(resp->headers->HasHeaderValue("Location",
      "http://" + addr_.ToString() + "/apps/YouTube"));

  int64 content_length = resp->headers->GetContentLength();
  EXPECT_EQ(0, content_length);
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
    HttpRequestInfo req = CreateRequest(tests[i].method, tests[i].path);
    const HttpResponseInfo* resp = GetResponse(req);
    ASSERT_TRUE(resp != NULL);

    EXPECT_EQ(tests[i].response_code_received, resp->headers->response_code());
  }
}

TEST_F(DialHttpServerTest, CallbackNormalTest) {
  test_response_.reset(new ResponseData());
  test_response_->succeeded_ = true;
  test_response_->response_body_ = "App Test";
  test_response_->mime_type_ = "text/plain; charset=\"utf-8\"";
  test_response_->response_code_ = HTTP_OK;
  test_response_->headers_.push_back("X-Test-Header: Baz");

  const HttpRequestInfo& req = CreateRequest("GET", "/apps/Foo/bar");
  EXPECT_CALL(*handler_, HandleRequest(Eq("/bar"), _, _))
      .WillOnce(DoAll(Invoke(this, &DialHttpServerTest::Capture), Return()));

  const HttpResponseInfo* resp = GetResponse(req);
  ASSERT_TRUE(resp != NULL);
  DoResponseCheck(resp, true);

  std::string store;
  EXPECT_TRUE(resp->headers->GetMimeType(&store));
  EXPECT_EQ("text/plain", store);
  EXPECT_TRUE(resp->headers->GetCharset(&store));
  EXPECT_EQ("utf-8", store);
}

TEST_F(DialHttpServerTest, CallbackExceptionInServiceHandler) {
  // succeeded_ = false means the HTTP server should send us a 404.
  test_response_.reset(new ResponseData());
  test_response_->succeeded_ = false;
  test_response_->response_body_ = "App Test";
  test_response_->mime_type_ = "text/plain; charset=\"utf-8\"";
  test_response_->response_code_ = HTTP_OK;

  const HttpRequestInfo& req = CreateRequest("GET", "/apps/Foo/?throw=1");
  EXPECT_CALL(*handler_, HandleRequest(Eq("/?throw=1"), _, _))
      .WillOnce(DoAll(Invoke(this, &DialHttpServerTest::Capture), Return()));

  const HttpResponseInfo* resp = GetResponse(req);
  ASSERT_TRUE(resp != NULL);
  EXPECT_EQ(HTTP_NOT_FOUND, resp->headers->response_code());
}

TEST_F(DialHttpServerTest, CallbackHandleRequestReturnsFalse) {
  test_response_.reset(new ResponseData());
  test_response_->response_code_ = HTTP_NOT_FOUND;
  const HttpRequestInfo& req = CreateRequest("GET", "/apps/Foo/false/app");
  EXPECT_CALL(*handler_, HandleRequest(Eq("/false/app"), _, _))
      .WillOnce(DoAll(Invoke(this, &DialHttpServerTest::Capture), Return()));

  const HttpResponseInfo* resp = GetResponse(req);
  ASSERT_TRUE(resp != NULL);
  EXPECT_EQ(HTTP_NOT_FOUND, resp->headers->response_code());
}

// Ensure src address is not IFADDR_ANY or lo. We need some IP addresses here!
TEST_F(DialHttpServerTest, GetLocalAddress) {
  EXPECT_STRNE("0.0.0.0", addr_.ToStringWithoutPort().c_str());
  EXPECT_STRNE("127.0.0.1", addr_.ToStringWithoutPort().c_str());
}

} // namespace net
