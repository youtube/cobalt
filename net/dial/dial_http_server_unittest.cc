// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dial_http_server.h"
#include "dial_service_handler.h"

#include "base/message_loop.h"
#include "base/run_loop.h"
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

// Test fixture
class DialHttpServerTest : public testing::Test {
 public:
  IPEndPoint addr_;
  scoped_refptr<HttpNetworkSession> session_;
  scoped_ptr<HttpNetworkTransaction> client_;
  MockServiceHandler foo_handler;
  MessageLoop* gtest_message_loop_;

  std::string response_body_;
  std::string mime_type_;
  int response_code_;
  std::vector<std::string> headers_;
  bool succeeded_;

  void StartHttpServer() {
    gtest_message_loop_ = MessageLoop::current();

    ON_CALL(foo_handler, service_name()).WillByDefault(Return("Foo"));
    DialService::GetInstance()->Register(&foo_handler);
    net::WaitUntilIdle(DialService::GetInstance()->GetMessageLoop());

    ASSERT_EQ(OK, DialService::GetInstance()->GetLocalAddress(&addr_));
    ASSERT_NE(0, addr_.port());
  }

  void InitHttpClientLibrary() {
    HttpNetworkSession::Params params;
    params.proxy_service = ProxyService::CreateDirect();
    params.ssl_config_service = new SSLConfigServiceDefaults();
    params.http_server_properties = new HttpServerPropertiesImpl();
    params.host_resolver = new MockHostResolver();
    session_ = new HttpNetworkSession(params);
    client_.reset(new HttpNetworkTransaction(session_));
  }

  virtual void SetUp() OVERRIDE {
    StartHttpServer();
    InitHttpClientLibrary();
  }

  virtual void TearDown() OVERRIDE {
    // make sure all messages on DialService are executed.
    DialService::GetInstance()->Deregister(&foo_handler);
    net::WaitUntilIdle(DialService::GetInstance()->GetMessageLoop());
  }

  const HttpResponseInfo* GetResponse(const HttpRequestInfo& req) {
    TestCompletionCallback callback;
    int rv = client_->Start(&req, callback.callback(), BoundNetLog());

    if (rv == ERR_IO_PENDING) {
      // FIXME: This call can be flaky: It may wait forever.
      callback.WaitForResult();
    }
    base::RunLoop().RunUntilIdle();
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

  // This happens on the DialService::Thread, so redirect it back to the main
  // thread.
  void Capture(const std::string& path,
               const HttpServerRequestInfo& request,
               HttpServerResponseInfo* response,
               const base::Callback<void(bool)>& on_completion) {
    ASSERT_EQ(MessageLoop::current(),
              DialService::GetInstance()->GetMessageLoop());

    response->body = response_body_;
    response->mime_type = mime_type_;
    response->response_code = response_code_;
    response->headers = headers_;

    gtest_message_loop_->PostTask(FROM_HERE,
                                  base::Bind(on_completion, succeeded_));
    net::WaitUntilIdle(gtest_message_loop_);
  }

  void DoResponseCheck(const HttpResponseInfo* resp, bool has_contents) {
    EXPECT_EQ(response_code_, resp->headers->response_code());

    for (std::vector<std::string>::const_iterator it = headers_.begin();
         it != headers_.end();
         ++it) {
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
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(response_body_.length(), rv);
    EXPECT_EQ(0, strncmp(buffer->data(), response_body_.data(),
                         response_body_.length()));
  }
};

TEST_F(DialHttpServerTest, SendManifest) {
  HttpRequestInfo req = CreateRequest("GET", "/dd.xml");
  const HttpResponseInfo* resp = GetResponse(req);

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
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(content_length, rv);
}

TEST_F(DialHttpServerTest, CurrentRunningAppRedirect) {
  HttpRequestInfo req = CreateRequest("GET", "/apps/");
  const HttpResponseInfo* resp = GetResponse(req);

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

    EXPECT_EQ(tests[i].response_code_received, resp->headers->response_code());
  }
}

TEST_F(DialHttpServerTest, CallbackNormalTest) {
  this->succeeded_ = true;
  this->response_body_ = "App Test";
  this->mime_type_ = "text/plain; charset=\"utf-8\"";
  this->response_code_ = HTTP_OK;
  this->headers_.push_back("X-Test-Header: Baz");

  const HttpRequestInfo& req = CreateRequest("GET", "/apps/Foo/bar");
  EXPECT_CALL(foo_handler, handleRequest(Eq("/bar"), _, _, _)).WillOnce(
      DoAll(Invoke(this, &DialHttpServerTest::Capture), Return(true)));

  const HttpResponseInfo* resp = GetResponse(req);
  DoResponseCheck(resp, true);

  std::string store;
  EXPECT_TRUE(resp->headers->GetMimeType(&store));
  EXPECT_EQ("text/plain", store);
  EXPECT_TRUE(resp->headers->GetCharset(&store));
  EXPECT_EQ("utf-8", store);
}

TEST_F(DialHttpServerTest, CallbackExceptionInServiceHandler) {
  this->succeeded_ = false;
  this->response_body_ = "App Test";
  this->mime_type_ = "text/plain; charset=\"utf-8\"";
  this->response_code_ = HTTP_OK;

  const HttpRequestInfo& req = CreateRequest("GET", "/apps/Foo/?throw=1");
  EXPECT_CALL(foo_handler, handleRequest(Eq("/?throw=1"), _, _, _)).WillOnce(
      DoAll(Invoke(this, &DialHttpServerTest::Capture), Return(true)));

  const HttpResponseInfo* resp = GetResponse(req);
  EXPECT_EQ(HTTP_NOT_FOUND, resp->headers->response_code());
}

TEST_F(DialHttpServerTest, CallbackHandleRequestReturnsFalse) {
  const HttpRequestInfo& req = CreateRequest("GET", "/apps/Foo/false/app");
  EXPECT_CALL(foo_handler, handleRequest(Eq("/false/app"), _, _, _)).WillOnce(
      Return(false));

  const HttpResponseInfo* resp = GetResponse(req);
  EXPECT_EQ(HTTP_NOT_FOUND, resp->headers->response_code());
}

// Ensure src address is not IFADDR_ANY or lo. We need some IP addresses here!
TEST_F(DialHttpServerTest, GetLocalAddress) {
  EXPECT_STRNE("0.0.0.0", addr_.ToStringWithoutPort().c_str());
  EXPECT_STRNE("127.0.0.1", addr_.ToStringWithoutPort().c_str());
}

} // namespace net

