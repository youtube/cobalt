// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "dial_http_server.h"
#include "dial_service_handler.h"

#include "base/strings/string_split.h"
#include "base/test/scoped_task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dial/dial_service.h"
#include "net/dial/dial_test_helpers.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/client_socket_factory.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;

constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("dial_http_server_test",
                                        "dial_http_server_test");

// Data our mock service handler will send to the dial HTTP server.
struct ResponseData {
  ResponseData() : response_code_(0), succeeded_(false) {}
  std::string response_body_;
  std::string mime_type_;
  std::vector<std::pair<std::string, std::string>> headers_;
  int response_code_;
  bool succeeded_;
};

// Test fixture
class DialHttpServerTest : public testing::Test {
 public:
  std::unique_ptr<DialService> dial_service_;
  IPEndPoint addr_;
  std::unique_ptr<HttpNetworkSession> session_;
  std::unique_ptr<HttpNetworkTransaction> client_;
  scoped_refptr<MockServiceHandler> handler_;
  std::unique_ptr<ResponseData> test_response_;
  // The task environment mainly gives us an IO message loop that's needed for
  // TCP connection.
  base::test::ScopedTaskEnvironment scoped_task_env_;
  HttpServerProperties* http_server_properties_;

  // The following instances are usually stored in URLRequestContextStorage
  // but we don't need URLRequestContext and therefore can not have the
  // URLRequestContextStorage.
  SSLConfigService* ssl_config_service_;
  ProxyResolutionService* proxy_resolution_service_;
  TransportSecurityState* transport_security_state_;
  MultiLogCTVerifier* cert_transparency_verifier_;
  CTPolicyEnforcer* ct_policy_enforcer_;
  CertVerifier* cert_verifier_;
  HostResolver* host_resolver_;

  DialHttpServerTest()
      : scoped_task_env_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {
    handler_ = new MockServiceHandler("Foo");
  }

  void InitHttpClientLibrary() {
    HttpNetworkSession::Params params;
    HttpNetworkSession::Context context;
    context.proxy_resolution_service = proxy_resolution_service_ =
        ProxyResolutionService::CreateDirect().release();
    context.http_server_properties = http_server_properties_ =
        new HttpServerPropertiesImpl();
    context.ssl_config_service = ssl_config_service_ =
        new SSLConfigServiceDefaults();
    context.transport_security_state = transport_security_state_ =
        new TransportSecurityState();
    context.cert_transparency_verifier = cert_transparency_verifier_ =
        new MultiLogCTVerifier();
    context.ct_policy_enforcer = ct_policy_enforcer_ =
        new DefaultCTPolicyEnforcer();
    context.host_resolver = host_resolver_ = new MockHostResolver();
    context.cert_verifier = cert_verifier_ =
        CertVerifier::CreateDefault().release();
    session_.reset(new HttpNetworkSession(params, context));
    client_.reset(new HttpNetworkTransaction(net::RequestPriority::MEDIUM,
                                             session_.get()));
  }

  virtual void SetUp() override {
    dial_service_.reset(new DialService());
    dial_service_->Register(handler_);
    EXPECT_EQ(OK, dial_service_->http_server()->GetLocalAddress(&addr_));
    InitHttpClientLibrary();
  }

  virtual void TearDown() override {
    dial_service_->Deregister(handler_);
    dial_service_.reset(NULL);

    client_.reset();
    // We need to cleanup session_ for its destructor's dependency on
    // ssl_config_service_;
    session_.reset();
    delete http_server_properties_;
    delete ssl_config_service_;
    delete proxy_resolution_service_;
    delete transport_security_state_;
    delete cert_transparency_verifier_;
    delete ct_policy_enforcer_;
    delete cert_verifier_;
    delete host_resolver_;
  }

  const HttpResponseInfo* GetResponse(const HttpRequestInfo& req) {
    TestCompletionCallback callback;
    int rv = client_->Start(&req, callback.callback(), NetLogWithSource());

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
    req.traffic_annotation =
        MutableNetworkTrafficAnnotationTag(kNetworkTrafficAnnotation);
    return req;
  }

  void Capture(const std::string& path,
               const HttpServerRequestInfo& request,
               const DialServiceHandler::CompletionCB& on_completion) {
    if (!test_response_->succeeded_) {
      on_completion.Run(std::unique_ptr<HttpServerResponseInfo>());
      return;
    }
    // This function simulates a DialServiceHandler response.
    std::unique_ptr<HttpServerResponseInfo> response(new HttpServerResponseInfo(
        net::HttpStatusCode(test_response_->response_code_)));
    response->SetBody(test_response_->response_body_,
                      test_response_->mime_type_);
    for (auto i : test_response_->headers_) {
      response->AddHeader(i.first, i.second);
    }

    on_completion.Run(std::move(response));
  }

  void DoResponseCheck(const HttpResponseInfo* resp, bool has_contents) {
    EXPECT_EQ(test_response_->response_code_, resp->headers->response_code());

    for (auto it = test_response_->headers_.begin();
         it != test_response_->headers_.end(); ++it) {
      EXPECT_TRUE(resp->headers->HasHeaderValue(it->first, it->second));
    }

    int64 content_length = resp->headers->GetContentLength();
    if (!has_contents) {
      EXPECT_EQ(0, content_length);
      return;
    }

    ASSERT_NE(0, content_length);  // if failed, no point continuing.

    scoped_refptr<IOBuffer> buffer(
        new IOBuffer(static_cast<size_t>(content_length)));
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

  EXPECT_TRUE(resp->headers->HasHeaderValue(
      "APPLICATION-URL", "http://" + addr_.ToString() + "/apps/"));

  int64 content_length = resp->headers->GetContentLength();
  ASSERT_NE(0, content_length);  // if failed, no point continuing.

  scoped_refptr<IOBuffer> buffer(
      new IOBuffer(static_cast<size_t>(content_length)));
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

  EXPECT_TRUE(resp->headers->HasHeaderValue(
      "Location", "http://" + addr_.ToString() + "/apps/YouTube"));

  int64 content_length = resp->headers->GetContentLength();
  EXPECT_EQ(0, content_length);
}

TEST_F(DialHttpServerTest, AllOtherRequests) {
  struct TestStruct {
    const char* method;
    const char* path;
    const int response_code_received;
  } tests[] = {
      {"GET", "/", HTTP_NOT_FOUND},
      {"GET", "/etc", HTTP_NOT_FOUND},
      {"POST", "/dd.xml", HTTP_NOT_FOUND},
      {"POST", "/apps/YouTube", HTTP_NOT_FOUND},
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
  test_response_->headers_.push_back(
      std::make_pair<std::string, std::string>("X-Test-Header", "Baz"));

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

}  // namespace net
