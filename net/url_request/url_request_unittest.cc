// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <shlobj.h>
#include <windows.h>
#endif

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_store_test_helpers.h"
#include "net/base/load_flags.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/net_module.h"
#include "net/base/net_util.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/upload_data.h"
#include "net/disk_cache/disk_cache.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_service.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_file_dir_job.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_redirect_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Time;

namespace net {

namespace {

const string16 kChrome(ASCIIToUTF16("chrome"));
const string16 kSecret(ASCIIToUTF16("secret"));
const string16 kUser(ASCIIToUTF16("user"));

base::StringPiece TestNetResourceProvider(int key) {
  return "header";
}

// Do a case-insensitive search through |haystack| for |needle|.
bool ContainsString(const std::string& haystack, const char* needle) {
  std::string::const_iterator it =
      std::search(haystack.begin(),
                  haystack.end(),
                  needle,
                  needle + strlen(needle),
                  base::CaseInsensitiveCompare<char>());
  return it != haystack.end();
}

void FillBuffer(char* buffer, size_t len) {
  static bool called = false;
  if (!called) {
    called = true;
    int seed = static_cast<int>(Time::Now().ToInternalValue());
    srand(seed);
  }

  for (size_t i = 0; i < len; i++) {
    buffer[i] = static_cast<char>(rand());
    if (!buffer[i])
      buffer[i] = 'g';
  }
}

scoped_refptr<UploadData> CreateSimpleUploadData(const char* data) {
  scoped_refptr<UploadData> upload(new UploadData);
  upload->AppendBytes(data, strlen(data));
  return upload;
}

// Verify that the SSLInfo of a successful SSL connection has valid values.
void CheckSSLInfo(const SSLInfo& ssl_info) {
  // Allow ChromeFrame fake SSLInfo to get through.
  if (ssl_info.cert.get() &&
      ssl_info.cert.get()->issuer().GetDisplayName() == "Chrome Internal") {
    // -1 means unknown.
    EXPECT_EQ(ssl_info.security_bits, -1);
    return;
  }

  // -1 means unknown.  0 means no encryption.
  EXPECT_GT(ssl_info.security_bits, 0);

  // The cipher suite TLS_NULL_WITH_NULL_NULL (0) must not be negotiated.
  int cipher_suite = SSLConnectionStatusToCipherSuite(
      ssl_info.connection_status);
  EXPECT_NE(0, cipher_suite);
}

}  // namespace

// A network delegate that blocks requests, optionally cancelling or redirecting
// them.
class BlockingNetworkDelegate : public TestNetworkDelegate {
 public:
  BlockingNetworkDelegate()
      : retval_(net::ERR_IO_PENDING),
        callback_retval_(net::OK),
        auth_retval_(NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING),
        auth_callback_retval_(
            NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {}

  void set_retval(int retval) { retval_ = retval; }
  void set_callback_retval(int retval) { callback_retval_ = retval; }
  void set_redirect_url(const GURL& url) { redirect_url_ = url; }
  void set_auth_retval(NetworkDelegate::AuthRequiredResponse retval) {
    auth_retval_ = retval; }
  void set_auth_callback_retval(NetworkDelegate::AuthRequiredResponse retval) {
    auth_callback_retval_ = retval; }
  void set_auth_credentials(const AuthCredentials& auth_credentials) {
    auth_credentials_ = auth_credentials;
  }

 private:
  // TestNetworkDelegate implementation.
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const net::CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE {
    if (redirect_url_ == request->url()) {
      // We've already seen this request and redirected elsewhere.
      return net::OK;
    }

    TestNetworkDelegate::OnBeforeURLRequest(request, callback, new_url);

    if (!redirect_url_.is_empty())
      *new_url = redirect_url_;

    if (retval_ != net::ERR_IO_PENDING)
      return retval_;

    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&BlockingNetworkDelegate::DoCallback,
                   weak_factory_.GetWeakPtr(), callback));
    return net::ERR_IO_PENDING;
  }

  virtual NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      URLRequest* request,
      const AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      AuthCredentials* credentials) OVERRIDE {
    TestNetworkDelegate::OnAuthRequired(request, auth_info, callback,
                                        credentials);
    switch (auth_retval_) {
      case NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION:
        break;
      case NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH:
        *credentials = auth_credentials_;
      case NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH:
        break;
      case NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING:
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&BlockingNetworkDelegate::DoAuthCallback,
                       weak_factory_.GetWeakPtr(), callback, credentials));
        break;
    }
    return auth_retval_;
  }

  void DoCallback(const net::CompletionCallback& callback) {
    callback.Run(callback_retval_);
  }

  void DoAuthCallback(const AuthCallback& callback,
                      AuthCredentials* credentials) {
    if (auth_callback_retval_ ==
        NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH) {
      *credentials = auth_credentials_;
    }
    callback.Run(auth_callback_retval_);
  }


  int retval_;
  int callback_retval_;
  GURL redirect_url_;
  NetworkDelegate::AuthRequiredResponse auth_retval_;
  NetworkDelegate::AuthRequiredResponse auth_callback_retval_;
  AuthCredentials auth_credentials_;
  base::WeakPtrFactory<BlockingNetworkDelegate> weak_factory_;
};

// A simple Interceptor that returns a pre-built URLRequestJob one time.
class TestJobInterceptor : public URLRequestJobFactory::Interceptor {
 public:
  TestJobInterceptor()
      : main_intercept_job_(NULL) {
  }

  virtual URLRequestJob* MaybeIntercept(URLRequest* request) const OVERRIDE {
    URLRequestJob* job = main_intercept_job_;
    main_intercept_job_ = NULL;
    return job;
  }

  virtual URLRequestJob* MaybeInterceptRedirect(
      const GURL& location, URLRequest* request) const OVERRIDE {
    return NULL;
  }

  virtual URLRequestJob* MaybeInterceptResponse(
      URLRequest* request) const OVERRIDE {
    return NULL;
  }

  void set_main_intercept_job(URLRequestJob* job) {
    main_intercept_job_ = job;
  }

 private:
  mutable URLRequestJob* main_intercept_job_;
};

// Inherit PlatformTest since we require the autorelease pool on Mac OS X.f
class URLRequestTest : public PlatformTest {
 public:
  URLRequestTest() : default_context_(new TestURLRequestContext(true)) {
    default_context_->set_network_delegate(&default_network_delegate_);
    default_context_->Init();
  }

  static void SetUpTestCase() {
    URLRequest::AllowFileAccess();
  }

  // Adds the TestJobInterceptor to the default context.
  TestJobInterceptor* AddTestInterceptor() {
    TestJobInterceptor* interceptor = new TestJobInterceptor();
    default_context_->set_job_factory(&job_factory_);
    job_factory_.AddInterceptor(interceptor);
    return interceptor;
  }

 protected:
  TestNetworkDelegate default_network_delegate_;  // must outlive URLRequest
  scoped_refptr<TestURLRequestContext> default_context_;
  URLRequestJobFactory job_factory_;
};

class URLRequestTestHTTP : public URLRequestTest {
 public:
  URLRequestTestHTTP()
      : test_server_(TestServer::TYPE_HTTP,
                     FilePath(FILE_PATH_LITERAL(
                                  "net/data/url_request_unittest"))) {
  }

 protected:
  // Requests |redirect_url|, which must return a HTTP 3xx redirect.
  // |request_method| is the method to use for the initial request.
  // |redirect_method| is the method that is expected to be used for the second
  // request, after redirection.
  // If |include_data| is true, data is uploaded with the request.  The
  // response body is expected to match it exactly, if and only if
  // |request_method| == |redirect_method|.
  void HTTPRedirectMethodTest(const GURL& redirect_url,
                              const std::string& request_method,
                              const std::string& redirect_method,
                              bool include_data) {
    static const char kData[] = "hello world";
    TestDelegate d;
    TestURLRequest req(redirect_url, &d);
    req.set_context(default_context_);
    req.set_method(request_method);
    if (include_data) {
      req.set_upload(CreateSimpleUploadData(kData).get());
      HttpRequestHeaders headers;
      headers.SetHeader(HttpRequestHeaders::kContentLength,
                        base::UintToString(arraysize(kData) - 1));
      req.SetExtraRequestHeaders(headers);
    }
    req.Start();
    MessageLoop::current()->Run();
    EXPECT_EQ(redirect_method, req.method());
    EXPECT_EQ(URLRequestStatus::SUCCESS, req.status().status());
    EXPECT_EQ(OK, req.status().error());
    if (include_data) {
      if (request_method == redirect_method) {
        EXPECT_EQ(kData, d.data_received());
      } else {
        EXPECT_NE(kData, d.data_received());
      }
    }
    if (HasFailure())
      LOG(WARNING) << "Request method was: " << request_method;
  }

  void HTTPUploadDataOperationTest(const std::string& method) {
    const int kMsgSize = 20000;  // multiple of 10
    const int kIterations = 50;
    char *uploadBytes = new char[kMsgSize+1];
    char *ptr = uploadBytes;
    char marker = 'a';
    for (int idx = 0; idx < kMsgSize/10; idx++) {
      memcpy(ptr, "----------", 10);
      ptr += 10;
      if (idx % 100 == 0) {
        ptr--;
        *ptr++ = marker;
        if (++marker > 'z')
          marker = 'a';
      }
    }
    uploadBytes[kMsgSize] = '\0';

    for (int i = 0; i < kIterations; ++i) {
      TestDelegate d;
      URLRequest r(test_server_.GetURL("echo"), &d);
      r.set_context(default_context_);
      r.set_method(method.c_str());

      r.AppendBytesToUpload(uploadBytes, kMsgSize);

      r.Start();
      EXPECT_TRUE(r.is_pending());

      MessageLoop::current()->Run();

      ASSERT_EQ(1, d.response_started_count()) << "request failed: " <<
          (int) r.status().status() << ", os error: " << r.status().error();

      EXPECT_FALSE(d.received_data_before_response());
      EXPECT_EQ(uploadBytes, d.data_received());
      EXPECT_EQ(memcmp(uploadBytes, d.data_received().c_str(), kMsgSize), 0);
      EXPECT_EQ(d.data_received().compare(uploadBytes), 0);
    }
    delete[] uploadBytes;
  }

  void AddChunksToUpload(TestURLRequest* r) {
    r->AppendChunkToUpload("a", 1, false);
    r->AppendChunkToUpload("bcd", 3, false);
    r->AppendChunkToUpload("this is a longer chunk than before.", 35, false);
    r->AppendChunkToUpload("\r\n\r\n", 4, false);
    r->AppendChunkToUpload("0", 1, false);
    r->AppendChunkToUpload("2323", 4, true);
  }

  void VerifyReceivedDataMatchesChunks(TestURLRequest* r, TestDelegate* d) {
    // This should match the chunks sent by AddChunksToUpload().
    const char* expected_data =
        "abcdthis is a longer chunk than before.\r\n\r\n02323";

    ASSERT_EQ(1, d->response_started_count()) << "request failed: " <<
        (int) r->status().status() << ", os error: " << r->status().error();

    EXPECT_FALSE(d->received_data_before_response());

    ASSERT_EQ(strlen(expected_data), static_cast<size_t>(d->bytes_received()));
    EXPECT_EQ(0, memcmp(d->data_received().c_str(), expected_data,
                        strlen(expected_data)));
  }

  TestServer test_server_;
};

// In this unit test, we're using the HTTPTestServer as a proxy server and
// issuing a CONNECT request with the magic host name "www.redirect.com".
// The HTTPTestServer will return a 302 response, which we should not
// follow.
TEST_F(URLRequestTestHTTP, ProxyTunnelRedirectTest) {
  ASSERT_TRUE(test_server_.Start());

  TestNetworkDelegate network_delegate;  // must outlive URLRequest
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->SetProxyFromString(test_server_.host_port_pair().ToString());
  context->set_network_delegate(&network_delegate);
  context->Init();

  TestDelegate d;
  {
    URLRequest r(GURL("https://www.redirect.com/"), &d);
    r.set_context(context);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, r.status().error());
    EXPECT_EQ(1, d.response_started_count());
    // We should not have followed the redirect.
    EXPECT_EQ(0, d.received_redirect_count());
  }
}

// This is the same as the previous test, but checks that the network delegate
// registers the error.
// This test was disabled because it made chrome_frame_net_tests hang
// (see bug 102991).
TEST_F(URLRequestTestHTTP, DISABLED_NetworkDelegateTunnelConnectionFailed) {
  ASSERT_TRUE(test_server_.Start());

  TestNetworkDelegate network_delegate;  // must outlive URLRequest
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->SetProxyFromString(test_server_.host_port_pair().ToString());
  context->set_network_delegate(&network_delegate);
  context->Init();

  TestDelegate d;
  {
    URLRequest r(GURL("https://www.redirect.com/"), &d);
    r.set_context(context);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, r.status().error());
    EXPECT_EQ(1, d.response_started_count());
    // We should not have followed the redirect.
    EXPECT_EQ(0, d.received_redirect_count());

    EXPECT_EQ(1, network_delegate.error_count());
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, network_delegate.last_error());
  }
}

// Tests that the network delegate can block and cancel a request.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate;
  network_delegate.set_callback_retval(ERR_EMPTY_RESPONSE);

  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->SetProxyFromString(test_server_.host_port_pair().ToString());
  context->set_network_delegate(&network_delegate);
  context->Init();

  {
    TestURLRequest r(test_server_.GetURL(""), &d);
    r.set_context(context);

    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    EXPECT_EQ(ERR_EMPTY_RESPONSE, r.status().error());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can cancel a request synchronously.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestSynchronously) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate;
  network_delegate.set_retval(ERR_EMPTY_RESPONSE);

  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->SetProxyFromString(test_server_.host_port_pair().ToString());
  context->set_network_delegate(&network_delegate);
  context->Init();

  {
    TestURLRequest r(test_server_.GetURL(""), &d);
    r.set_context(context);

    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    EXPECT_EQ(ERR_EMPTY_RESPONSE, r.status().error());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can block and redirect a request to a new
// URL.
TEST_F(URLRequestTestHTTP, NetworkDelegateRedirectRequest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate;
  GURL redirect_url(test_server_.GetURL("simple.html"));
  network_delegate.set_redirect_url(redirect_url);

  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->SetProxyFromString(test_server_.host_port_pair().ToString());
  context->set_network_delegate(&network_delegate);
  context->Init();

  {
    GURL original_url(test_server_.GetURL("empty.html"));
    TestURLRequest r(original_url, &d);
    r.set_context(context);

    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(redirect_url, r.url());
    EXPECT_EQ(original_url, r.original_url());
    EXPECT_EQ(2U, r.url_chain().size());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that redirects caused by the network delegate preserve POST data.
TEST_F(URLRequestTestHTTP, NetworkDelegateRedirectRequestPost) {
  ASSERT_TRUE(test_server_.Start());

  const char kData[] = "hello world";

  TestDelegate d;
  BlockingNetworkDelegate network_delegate;
  GURL redirect_url(test_server_.GetURL("echo"));
  network_delegate.set_redirect_url(redirect_url);

  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->Init();

  {
    GURL original_url(test_server_.GetURL("empty.html"));
    TestURLRequest r(original_url, &d);
    r.set_context(context);
    r.set_method("POST");
    r.set_upload(CreateSimpleUploadData(kData).get());
    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kContentLength,
                      base::UintToString(arraysize(kData) - 1));
    r.SetExtraRequestHeaders(headers);
    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(redirect_url, r.url());
    EXPECT_EQ(original_url, r.original_url());
    EXPECT_EQ(2U, r.url_chain().size());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
    EXPECT_EQ("POST", r.method());
    EXPECT_EQ(kData, d.data_received());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can synchronously complete OnAuthRequired
// by taking no action. This indicates that the NetworkDelegate does not want to
// handle the challenge, and is passing the buck along to the
// URLRequest::Delegate.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredSyncNoAction) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate;
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION);

  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->Init();

  d.set_credentials(AuthCredentials(kUser, kSecret));

  {
    GURL url(test_server_.GetURL("auth-basic"));
    TestURLRequest r(url, &d);
    r.set_context(context);
    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_TRUE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can synchronously complete OnAuthRequired
// by setting credentials.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredSyncSetAuth) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate;
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);

  network_delegate.set_auth_credentials(AuthCredentials(kUser, kSecret));

  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->Init();

  {
    GURL url(test_server_.GetURL("auth-basic"));
    TestURLRequest r(url, &d);
    r.set_context(context);
    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can synchronously complete OnAuthRequired
// by cancelling authentication.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredSyncCancel) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate;
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH);

  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->Init();

  {
    GURL url(test_server_.GetURL("auth-basic"));
    TestURLRequest r(url, &d);
    r.set_context(context);
    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(OK, r.status().error());
    EXPECT_EQ(401, r.GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can asynchronously complete OnAuthRequired
// by taking no action. This indicates that the NetworkDelegate does not want
// to handle the challenge, and is passing the buck along to the
// URLRequest::Delegate.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredAsyncNoAction) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate;
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING);
  network_delegate.set_auth_callback_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION);

  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->Init();

  d.set_credentials(AuthCredentials(kUser, kSecret));

  {
    GURL url(test_server_.GetURL("auth-basic"));
    TestURLRequest r(url, &d);
    r.set_context(context);
    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());
    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_TRUE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can asynchronously complete OnAuthRequired
// by setting credentials.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredAsyncSetAuth) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate;
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING);
  network_delegate.set_auth_callback_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);

  AuthCredentials auth_credentials(kUser, kSecret);
  network_delegate.set_auth_credentials(auth_credentials);

  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->Init();

  {
    GURL url(test_server_.GetURL("auth-basic"));
    TestURLRequest r(url, &d);
    r.set_context(context);
    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(0, r.status().error());

    EXPECT_EQ(200, r.GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can asynchronously complete OnAuthRequired
// by cancelling authentication.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredAsyncCancel) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate;
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING);
  network_delegate.set_auth_callback_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH);

  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->Init();

  {
    GURL url(test_server_.GetURL("auth-basic"));
    TestURLRequest r(url, &d);
    r.set_context(context);
    r.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    EXPECT_EQ(OK, r.status().error());
    EXPECT_EQ(401, r.GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// In this unit test, we're using the HTTPTestServer as a proxy server and
// issuing a CONNECT request with the magic host name "www.server-auth.com".
// The HTTPTestServer will return a 401 response, which we should balk at.
TEST_F(URLRequestTestHTTP, UnexpectedServerAuthTest) {
  ASSERT_TRUE(test_server_.Start());

  TestNetworkDelegate network_delegate;  // must outlive URLRequest
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->SetProxyFromString(test_server_.host_port_pair().ToString());
  context->set_network_delegate(&network_delegate);
  context->Init();

  TestDelegate d;
  {
    URLRequest r(GURL("https://www.server-auth.com/"), &d);
    r.set_context(context);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, r.status().error());
  }
}

TEST_F(URLRequestTestHTTP, GetTest_NoCache) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    TestURLRequest r(test_server_.GetURL(""), &d);
    r.set_context(default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());

    // TODO(eroman): Add back the NetLog tests...
  }
}

TEST_F(URLRequestTestHTTP, GetTest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    TestURLRequest r(test_server_.GetURL(""), &d);
    r.set_context(default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());
  }
}

TEST_F(URLRequestTestHTTP, GetZippedTest) {
  ASSERT_TRUE(test_server_.Start());

  // Parameter that specifies the Content-Length field in the response:
  // C - Compressed length.
  // U - Uncompressed length.
  // L - Large length (larger than both C & U).
  // M - Medium length (between C & U).
  // S - Small length (smaller than both C & U).
  const char test_parameters[] = "CULMS";
  const int num_tests = arraysize(test_parameters)- 1;  // Skip NULL.
  // C & U should be OK.
  // L & M are larger than the data sent, and show an error.
  // S has too little data, but we seem to accept it.
  const bool test_expect_success[num_tests] =
      { true, false, false, false, true };

  for (int i = 0; i < num_tests ; i++) {
    TestDelegate d;
    {
      std::string test_file =
          base::StringPrintf("compressedfiles/BullRunSpeech.txt?%c",
                             test_parameters[i]);

      TestNetworkDelegate network_delegate;  // must outlive URLRequest
      scoped_refptr<TestURLRequestContext> context(
          new TestURLRequestContext(true));
      context->set_network_delegate(&network_delegate);
      context->Init();

      TestURLRequest r(test_server_.GetURL(test_file), &d);
      r.set_context(context);
      r.Start();
      EXPECT_TRUE(r.is_pending());

      MessageLoop::current()->Run();

      EXPECT_EQ(1, d.response_started_count());
      EXPECT_FALSE(d.received_data_before_response());
      VLOG(1) << " Received " << d.bytes_received() << " bytes"
              << " status = " << r.status().status()
              << " error = " << r.status().error();
      if (test_expect_success[i]) {
        EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status())
            << " Parameter = \"" << test_file << "\"";
      } else {
        EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
        EXPECT_EQ(ERR_CONTENT_LENGTH_MISMATCH, r.status().error())
            << " Parameter = \"" << test_file << "\"";
      }
    }
  }
}

// This test was disabled because it made chrome_frame_net_tests hang
// (see bug 102991).
TEST_F(URLRequestTestHTTP, DISABLED_HTTPSToHTTPRedirectNoRefererTest) {
  ASSERT_TRUE(test_server_.Start());

  TestServer https_test_server(
      TestServer::TYPE_HTTPS, FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(https_test_server.Start());

  // An https server is sent a request with an https referer,
  // and responds with a redirect to an http url. The http
  // server should not be sent the referer.
  GURL http_destination = test_server_.GetURL("");
  TestDelegate d;
  TestURLRequest req(https_test_server.GetURL(
      "server-redirect?" + http_destination.spec()), &d);
  req.set_context(default_context_);
  req.set_referrer("https://www.referrer.com/");
  req.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(1, d.received_redirect_count());
  EXPECT_EQ(http_destination, req.url());
  EXPECT_EQ(std::string(), req.referrer());
}

TEST_F(URLRequestTestHTTP, MultipleRedirectTest) {
  ASSERT_TRUE(test_server_.Start());

  GURL destination_url = test_server_.GetURL("");
  GURL middle_redirect_url = test_server_.GetURL(
      "server-redirect?" + destination_url.spec());
  GURL original_url = test_server_.GetURL(
      "server-redirect?" + middle_redirect_url.spec());
  TestDelegate d;
  TestURLRequest req(original_url, &d);
  req.set_context(default_context_);
  req.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(2, d.received_redirect_count());
  EXPECT_EQ(destination_url, req.url());
  EXPECT_EQ(original_url, req.original_url());
  ASSERT_EQ(3U, req.url_chain().size());
  EXPECT_EQ(original_url, req.url_chain()[0]);
  EXPECT_EQ(middle_redirect_url, req.url_chain()[1]);
  EXPECT_EQ(destination_url, req.url_chain()[2]);
}

class HTTPSRequestTest : public testing::Test {
 public:
  HTTPSRequestTest() : default_context_(new TestURLRequestContext(true)) {
    default_context_->set_network_delegate(&default_network_delegate_);
    default_context_->Init();
  }
  virtual ~HTTPSRequestTest() {}

 protected:
  TestNetworkDelegate default_network_delegate_;  // must outlive URLRequest
  scoped_refptr<TestURLRequestContext> default_context_;
};

// This test was disabled because it made chrome_frame_net_tests hang
// (see bug 102991).
TEST_F(HTTPSRequestTest, DISABLED_HTTPSGetTest) {
  TestServer test_server(TestServer::TYPE_HTTPS,
                         FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  TestDelegate d;
  {
    TestURLRequest r(test_server.GetURL(""), &d);
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    CheckSSLInfo(r.ssl_info());
    EXPECT_EQ(test_server.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server.host_port_pair().port(),
              r.GetSocketAddress().port());
  }
}

TEST_F(HTTPSRequestTest, HTTPSMismatchedTest) {
  TestServer::HTTPSOptions https_options(
      TestServer::HTTPSOptions::CERT_MISMATCHED_NAME);
  TestServer test_server(https_options,
                         FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  bool err_allowed = true;
  for (int i = 0; i < 2 ; i++, err_allowed = !err_allowed) {
    TestDelegate d;
    {
      d.set_allow_certificate_errors(err_allowed);
      TestURLRequest r(test_server.GetURL(""), &d);
      r.set_context(default_context_);

      r.Start();
      EXPECT_TRUE(r.is_pending());

      MessageLoop::current()->Run();

      EXPECT_EQ(1, d.response_started_count());
      EXPECT_FALSE(d.received_data_before_response());
      EXPECT_TRUE(d.have_certificate_errors());
      if (err_allowed) {
        EXPECT_NE(0, d.bytes_received());
        CheckSSLInfo(r.ssl_info());
      } else {
        EXPECT_EQ(0, d.bytes_received());
      }
    }
  }
}

TEST_F(HTTPSRequestTest, HTTPSExpiredTest) {
  TestServer::HTTPSOptions https_options(
      TestServer::HTTPSOptions::CERT_EXPIRED);
  TestServer test_server(https_options,
                         FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  // Iterate from false to true, just so that we do the opposite of the
  // previous test in order to increase test coverage.
  bool err_allowed = false;
  for (int i = 0; i < 2 ; i++, err_allowed = !err_allowed) {
    TestDelegate d;
    {
      d.set_allow_certificate_errors(err_allowed);
      TestURLRequest r(test_server.GetURL(""), &d);
      r.set_context(default_context_);

      r.Start();
      EXPECT_TRUE(r.is_pending());

      MessageLoop::current()->Run();

      EXPECT_EQ(1, d.response_started_count());
      EXPECT_FALSE(d.received_data_before_response());
      EXPECT_TRUE(d.have_certificate_errors());
      if (err_allowed) {
        EXPECT_NE(0, d.bytes_received());
        CheckSSLInfo(r.ssl_info());
      } else {
        EXPECT_EQ(0, d.bytes_received());
      }
    }
  }
}

// This tests that a load of www.google.com with a certificate error sets the
// is_hsts_host flag correctly. This flag will cause the interstitial to be
// fatal.
TEST_F(HTTPSRequestTest, HTTPSPreloadedHSTSTest) {
  TestServer::HTTPSOptions https_options(
      TestServer::HTTPSOptions::CERT_MISMATCHED_NAME);
  TestServer test_server(https_options,
                         FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  // We require that the URL be www.google.com in order to pick up the
  // preloaded HSTS entries in the TransportSecurityState. This means that we
  // have to use a MockHostResolver in order to direct www.google.com to the
  // testserver.

  MockHostResolver host_resolver;
  host_resolver.rules()->AddRule("www.google.com", "127.0.0.1");
  TestNetworkDelegate network_delegate;  // must outlive URLRequest
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->set_host_resolver(&host_resolver);
  TransportSecurityState transport_security_state("");
  context->set_transport_security_state(&transport_security_state);
  context->Init();

  TestDelegate d;
  TestURLRequest r(GURL(StringPrintf("https://www.google.com:%d",
                                     test_server.host_port_pair().port())),
                   &d);
  r.set_context(context);

  r.Start();
  EXPECT_TRUE(r.is_pending());

  MessageLoop::current()->Run();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_FALSE(d.received_data_before_response());
  EXPECT_TRUE(d.have_certificate_errors());
  EXPECT_TRUE(d.is_hsts_host());
}

namespace {

class SSLClientAuthTestDelegate : public TestDelegate {
 public:
  SSLClientAuthTestDelegate() : on_certificate_requested_count_(0) {
  }
  virtual void OnCertificateRequested(
      URLRequest* request,
      SSLCertRequestInfo* cert_request_info) {
    on_certificate_requested_count_++;
    MessageLoop::current()->Quit();
  }
  int on_certificate_requested_count() {
    return on_certificate_requested_count_;
  }
 private:
  int on_certificate_requested_count_;
};

}  // namespace

// TODO(davidben): Test the rest of the code. Specifically,
// - Filtering which certificates to select.
// - Sending a certificate back.
// - Getting a certificate request in an SSL renegotiation sending the
//   HTTP request.
TEST_F(HTTPSRequestTest, ClientAuthTest) {
  TestServer::HTTPSOptions https_options;
  https_options.request_client_certificate = true;
  TestServer test_server(https_options,
                         FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  SSLClientAuthTestDelegate d;
  {
    TestURLRequest r(test_server.GetURL(""), &d);
    r.set_context(default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.on_certificate_requested_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());

    // Send no certificate.
    // TODO(davidben): Get temporary client cert import (with keys) working on
    // all platforms so we can test sending a cert as well.
    r.ContinueWithCertificate(NULL);

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
  }
}

TEST_F(URLRequestTestHTTP, CancelTest) {
  TestDelegate d;
  {
    TestURLRequest r(GURL("http://www.google.com/"), &d);
    r.set_context(default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    r.Cancel();

    MessageLoop::current()->Run();

    // We expect to receive OnResponseStarted even though the request has been
    // cancelled.
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
  }
}

TEST_F(URLRequestTestHTTP, CancelTest2) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    TestURLRequest r(test_server_.GetURL(""), &d);
    r.set_context(default_context_);

    d.set_cancel_in_response_started(true);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
  }
}

TEST_F(URLRequestTestHTTP, CancelTest3) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    TestURLRequest r(test_server_.GetURL(""), &d);
    r.set_context(default_context_);

    d.set_cancel_in_received_data(true);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    // There is no guarantee about how much data was received
    // before the cancel was issued.  It could have been 0 bytes,
    // or it could have been all the bytes.
    // EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
  }
}

TEST_F(URLRequestTestHTTP, CancelTest4) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    TestURLRequest r(test_server_.GetURL(""), &d);
    r.set_context(default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    // The request will be implicitly canceled when it is destroyed. The
    // test delegate must not post a quit message when this happens because
    // this test doesn't actually have a message loop. The quit message would
    // get put on this thread's message queue and the next test would exit
    // early, causing problems.
    d.set_quit_on_complete(false);
  }
  // expect things to just cleanup properly.

  // we won't actually get a received reponse here because we've never run the
  // message loop
  EXPECT_FALSE(d.received_data_before_response());
  EXPECT_EQ(0, d.bytes_received());
}

TEST_F(URLRequestTestHTTP, CancelTest5) {
  ASSERT_TRUE(test_server_.Start());

  // populate cache
  {
    TestDelegate d;
    URLRequest r(test_server_.GetURL("cachetime"), &d);
    r.set_context(default_context_);
    r.Start();
    MessageLoop::current()->Run();
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
  }

  // cancel read from cache (see bug 990242)
  {
    TestDelegate d;
    URLRequest r(test_server_.GetURL("cachetime"), &d);
    r.set_context(default_context_);
    r.Start();
    r.Cancel();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
  }
}

TEST_F(URLRequestTestHTTP, PostTest) {
  ASSERT_TRUE(test_server_.Start());
  HTTPUploadDataOperationTest("POST");
}

TEST_F(URLRequestTestHTTP, PutTest) {
  ASSERT_TRUE(test_server_.Start());
  HTTPUploadDataOperationTest("PUT");
}

TEST_F(URLRequestTestHTTP, PostEmptyTest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    TestURLRequest r(test_server_.GetURL("echo"), &d);
    r.set_context(default_context_);
    r.set_method("POST");

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    ASSERT_EQ(1, d.response_started_count()) << "request failed: " <<
        (int) r.status().status() << ", error: " << r.status().error();

    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_TRUE(d.data_received().empty());
  }
}

TEST_F(URLRequestTestHTTP, PostFileTest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    TestURLRequest r(test_server_.GetURL("echo"), &d);
    r.set_context(default_context_);
    r.set_method("POST");

    FilePath dir;
    PathService::Get(base::DIR_EXE, &dir);
    file_util::SetCurrentDirectory(dir);

    FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.Append(FILE_PATH_LITERAL("net"));
    path = path.Append(FILE_PATH_LITERAL("data"));
    path = path.Append(FILE_PATH_LITERAL("url_request_unittest"));
    path = path.Append(FILE_PATH_LITERAL("with-headers.html"));
    r.AppendFileToUpload(path);

    // This file should just be ignored in the upload stream.
    r.AppendFileToUpload(FilePath(FILE_PATH_LITERAL(
        "c:\\path\\to\\non\\existant\\file.randomness.12345")));

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 longsize;
    ASSERT_EQ(true, file_util::GetFileSize(path, &longsize));
    int size = static_cast<int>(longsize);
    scoped_array<char> buf(new char[size]);

    int size_read = static_cast<int>(file_util::ReadFile(path,
        buf.get(), size));
    ASSERT_EQ(size, size_read);

    ASSERT_EQ(1, d.response_started_count()) << "request failed: " <<
        (int) r.status().status() << ", error: " << r.status().error();

    EXPECT_FALSE(d.received_data_before_response());

    ASSERT_EQ(size, d.bytes_received());
    EXPECT_EQ(0, memcmp(d.data_received().c_str(), buf.get(), size));
  }
}

TEST_F(URLRequestTestHTTP, TestPostChunkedDataBeforeStart) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    TestURLRequest r(test_server_.GetURL("echo"), &d);
    r.set_context(default_context_);
    r.EnableChunkedUpload();
    r.set_method("POST");
    AddChunksToUpload(&r);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    VerifyReceivedDataMatchesChunks(&r, &d);
  }
}

TEST_F(URLRequestTestHTTP, TestPostChunkedDataAfterStart) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    TestURLRequest r(test_server_.GetURL("echo"), &d);
    r.set_context(default_context_);
    r.EnableChunkedUpload();
    r.set_method("POST");
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->RunAllPending();
    AddChunksToUpload(&r);
    MessageLoop::current()->Run();

    VerifyReceivedDataMatchesChunks(&r, &d);
  }
}

TEST_F(URLRequestTest, AboutBlankTest) {
  TestDelegate d;
  {
    TestURLRequest r(GURL("about:blank"), &d);
    r.set_context(default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_TRUE(!r.is_pending());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 0);
    EXPECT_EQ("", r.GetSocketAddress().host());
    EXPECT_EQ(0, r.GetSocketAddress().port());
  }
}

TEST_F(URLRequestTest, DataURLImageTest) {
  TestDelegate d;
  {
    // Use our nice little Chrome logo.
    TestURLRequest r(GURL(
        "data:image/png;base64,"
        "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAADVklEQVQ4jX2TfUwUBBjG3"
        "w1y+HGcd9dxhXR8T4awOccJGgOSWclHImznLkTlSw0DDQXkrmgYgbUYnlQTqQxIEVxitD"
        "5UMCATRA1CEEg+Qjw3bWDxIauJv/5oumqs39/P827vnucRmYN0gyF01GI5MpCVdW0gO7t"
        "vNC+vqSEtbZefk5NuLv1jdJ46p/zw0HeH4+PHr3h7c1mjoV2t5rKzMx1+fg9bAgK6zHq9"
        "cU5z+LpA3xOtx34+vTeT21onRuzssC3zxbbSwC13d/pFuC7CkIMDxQpF7r/MWq12UctI1"
        "dWWm99ypqSYmRUBdKem8MkrO/kgaTt1O7YzlpzE5GIVd0WYUqt57yWf2McHTObYPbVD+Z"
        "wbtlLTVMZ3BW+TnLyXLaWtmEq6WJVbT3HBh3Svj2HQQcm43XwmtoYM6vVKleh0uoWvnzW"
        "3v3MpidruPTQPf0bia7sJOtBM0ufTWNvus/nkDFHF9ZS+uYVjRUasMeHUmyLYtcklTvzW"
        "GFZnNOXczThvpKIzjcahSqIzkvDLayDq6D3eOjtBbNUEIZYyqsvj4V4wY92eNJ4IoyhTb"
        "xXX1T5xsV9tm9r4TQwHLiZw/pdDZJea8TKmsmR/K0uLh/GwnCHghTja6lPhphezPfO5/5"
        "MrVvMzNaI3+ERHfrFzPKQukrQGI4d/3EFD/3E2mVNYvi4at7CXWREaxZGD+3hg28zD3gV"
        "Md6q5c8GdosynKmSeRuGzpjyl1/9UDGtPR5HeaKT8Wjo17WXk579BXVUhN64ehF9fhRtq"
        "/uxxZKzNiZFGD0wRC3NFROZ5mwIPL/96K/rKMMLrIzF9uhHr+/sYH7DAbwlgC4J+R2Z7F"
        "Ux1qLnV7MGF40smVSoJ/jvHRfYhQeUJd/SnYtGWhPHR0Sz+GE2F2yth0B36Vcz2KpnufB"
        "JbsysjjW4kblBUiIjiURUWqJY65zxbnTy57GQyH58zgy0QBtTQv5gH15XMdKkYu+TGaJM"
        "nlm2O34uI4b9tflqp1+QEFGzoW/ulmcofcpkZCYJhDfSpme7QcrHa+Xfji8paEQkTkSfm"
        "moRWRNZr/F1KfVMjW+IKEnv2FwZfKdzt0BQR6lClcZR0EfEXEfv/G6W9iLiIyCoReV5En"
        "hORIBHx+ufPj/gLB/zGI/G4Bk0AAAAASUVORK5CYII="),
        &d);
    r.set_context(default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_TRUE(!r.is_pending());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 911);
    EXPECT_EQ("", r.GetSocketAddress().host());
    EXPECT_EQ(0, r.GetSocketAddress().port());
  }
}

TEST_F(URLRequestTest, FileTest) {
  FilePath app_path;
  PathService::Get(base::FILE_EXE, &app_path);
  GURL app_url = FilePathToFileURL(app_path);

  TestDelegate d;
  {
    TestURLRequest r(app_url, &d);
    r.set_context(default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = -1;
    EXPECT_TRUE(file_util::GetFileSize(app_path, &file_size));

    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
    EXPECT_EQ("", r.GetSocketAddress().host());
    EXPECT_EQ(0, r.GetSocketAddress().port());
  }
}

TEST_F(URLRequestTest, FileTestFullSpecifiedRange) {
  const size_t buffer_size = 4000;
  scoped_array<char> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);

  FilePath temp_path;
  EXPECT_TRUE(file_util::CreateTemporaryFile(&temp_path));
  GURL temp_url = FilePathToFileURL(temp_path);
  EXPECT_TRUE(file_util::WriteFile(temp_path, buffer.get(), buffer_size));

  int64 file_size;
  EXPECT_TRUE(file_util::GetFileSize(temp_path, &file_size));

  const size_t first_byte_position = 500;
  const size_t last_byte_position = buffer_size - first_byte_position;
  const size_t content_length = last_byte_position - first_byte_position + 1;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + last_byte_position + 1);

  TestDelegate d;
  {
    TestURLRequest r(temp_url, &d);
    r.set_context(default_context_);

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kRange,
                      base::StringPrintf(
                           "bytes=%" PRIuS "-%" PRIuS,
                           first_byte_position, last_byte_position));
    r.SetExtraRequestHeaders(headers);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();
    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(static_cast<int>(content_length), d.bytes_received());
    // Don't use EXPECT_EQ, it will print out a lot of garbage if check failed.
    EXPECT_TRUE(partial_buffer_string == d.data_received());
  }

  EXPECT_TRUE(file_util::Delete(temp_path, false));
}

TEST_F(URLRequestTest, FileTestHalfSpecifiedRange) {
  const size_t buffer_size = 4000;
  scoped_array<char> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);

  FilePath temp_path;
  EXPECT_TRUE(file_util::CreateTemporaryFile(&temp_path));
  GURL temp_url = FilePathToFileURL(temp_path);
  EXPECT_TRUE(file_util::WriteFile(temp_path, buffer.get(), buffer_size));

  int64 file_size;
  EXPECT_TRUE(file_util::GetFileSize(temp_path, &file_size));

  const size_t first_byte_position = 500;
  const size_t last_byte_position = buffer_size - 1;
  const size_t content_length = last_byte_position - first_byte_position + 1;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + last_byte_position + 1);

  TestDelegate d;
  {
    TestURLRequest r(temp_url, &d);
    r.set_context(default_context_);

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kRange,
                      base::StringPrintf("bytes=%" PRIuS "-",
                                         first_byte_position));
    r.SetExtraRequestHeaders(headers);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();
    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(static_cast<int>(content_length), d.bytes_received());
    // Don't use EXPECT_EQ, it will print out a lot of garbage if check failed.
    EXPECT_TRUE(partial_buffer_string == d.data_received());
  }

  EXPECT_TRUE(file_util::Delete(temp_path, false));
}

TEST_F(URLRequestTest, FileTestMultipleRanges) {
  const size_t buffer_size = 400000;
  scoped_array<char> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);

  FilePath temp_path;
  EXPECT_TRUE(file_util::CreateTemporaryFile(&temp_path));
  GURL temp_url = FilePathToFileURL(temp_path);
  EXPECT_TRUE(file_util::WriteFile(temp_path, buffer.get(), buffer_size));

  int64 file_size;
  EXPECT_TRUE(file_util::GetFileSize(temp_path, &file_size));

  TestDelegate d;
  {
    TestURLRequest r(temp_url, &d);
    r.set_context(default_context_);

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kRange,
                      "bytes=0-0,10-200,200-300");
    r.SetExtraRequestHeaders(headers);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();
    EXPECT_TRUE(d.request_failed());
  }

  EXPECT_TRUE(file_util::Delete(temp_path, false));
}

TEST_F(URLRequestTest, InvalidUrlTest) {
  TestDelegate d;
  {
    TestURLRequest r(GURL("invalid url"), &d);
    r.set_context(default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();
    EXPECT_TRUE(d.request_failed());
  }
}

TEST_F(URLRequestTestHTTP, ResponseHeadersTest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("files/with-headers.html"), &d);
  req.set_context(default_context_);
  req.Start();
  MessageLoop::current()->Run();

  const HttpResponseHeaders* headers = req.response_headers();

  // Simple sanity check that response_info() accesses the same data.
  EXPECT_EQ(headers, req.response_info().headers.get());

  std::string header;
  EXPECT_TRUE(headers->GetNormalizedHeader("cache-control", &header));
  EXPECT_EQ("private", header);

  header.clear();
  EXPECT_TRUE(headers->GetNormalizedHeader("content-type", &header));
  EXPECT_EQ("text/html; charset=ISO-8859-1", header);

  // The response has two "X-Multiple-Entries" headers.
  // This verfies our output has them concatenated together.
  header.clear();
  EXPECT_TRUE(headers->GetNormalizedHeader("x-multiple-entries", &header));
  EXPECT_EQ("a, b", header);
}

#if defined(OS_WIN)
TEST_F(URLRequestTest, ResolveShortcutTest) {
  FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("net");
  app_path = app_path.AppendASCII("data");
  app_path = app_path.AppendASCII("url_request_unittest");
  app_path = app_path.AppendASCII("with-headers.html");

  std::wstring lnk_path = app_path.value() + L".lnk";

  HRESULT result;
  IShellLink *shell = NULL;
  IPersistFile *persist = NULL;

  CoInitialize(NULL);
  // Temporarily create a shortcut for test
  result = CoCreateInstance(CLSID_ShellLink, NULL,
                            CLSCTX_INPROC_SERVER, IID_IShellLink,
                            reinterpret_cast<LPVOID*>(&shell));
  ASSERT_TRUE(SUCCEEDED(result));
  result = shell->QueryInterface(IID_IPersistFile,
                                reinterpret_cast<LPVOID*>(&persist));
  ASSERT_TRUE(SUCCEEDED(result));
  result = shell->SetPath(app_path.value().c_str());
  EXPECT_TRUE(SUCCEEDED(result));
  result = shell->SetDescription(L"ResolveShortcutTest");
  EXPECT_TRUE(SUCCEEDED(result));
  result = persist->Save(lnk_path.c_str(), TRUE);
  EXPECT_TRUE(SUCCEEDED(result));
  if (persist)
    persist->Release();
  if (shell)
    shell->Release();

  TestDelegate d;
  {
    TestURLRequest r(FilePathToFileURL(FilePath(lnk_path)), &d);
    r.set_context(default_context_);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    WIN32_FILE_ATTRIBUTE_DATA data;
    GetFileAttributesEx(app_path.value().c_str(),
                        GetFileExInfoStandard, &data);
    HANDLE file = CreateFile(app_path.value().c_str(), GENERIC_READ,
                             FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT_NE(INVALID_HANDLE_VALUE, file);
    scoped_array<char> buffer(new char[data.nFileSizeLow]);
    DWORD read_size;
    BOOL result;
    result = ReadFile(file, buffer.get(), data.nFileSizeLow,
                      &read_size, NULL);
    std::string content(buffer.get(), read_size);
    CloseHandle(file);

    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(content, d.data_received());
  }

  // Clean the shortcut
  DeleteFile(lnk_path.c_str());
  CoUninitialize();
}
#endif  // defined(OS_WIN)

TEST_F(URLRequestTestHTTP, ContentTypeNormalizationTest) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL(
      "files/content-type-normalization.html"), &d);
  req.set_context(default_context_);
  req.Start();
  MessageLoop::current()->Run();

  std::string mime_type;
  req.GetMimeType(&mime_type);
  EXPECT_EQ("text/html", mime_type);

  std::string charset;
  req.GetCharset(&charset);
  EXPECT_EQ("utf-8", charset);
  req.Cancel();
}

TEST_F(URLRequestTest, FileDirCancelTest) {
  // Put in mock resource provider.
  NetModule::SetResourceProvider(TestNetResourceProvider);

  TestDelegate d;
  {
    FilePath file_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_path = file_path.Append(FILE_PATH_LITERAL("net"));
    file_path = file_path.Append(FILE_PATH_LITERAL("data"));

    TestURLRequest req(FilePathToFileURL(file_path), &d);
    req.set_context(default_context_);
    req.Start();
    EXPECT_TRUE(req.is_pending());

    d.set_cancel_in_received_data_pending(true);

    MessageLoop::current()->Run();
  }

  // Take out mock resource provider.
  NetModule::SetResourceProvider(NULL);
}

TEST_F(URLRequestTest, FileDirRedirectNoCrash) {
  // There is an implicit redirect when loading a file path that matches a
  // directory and does not end with a slash.  Ensure that following such
  // redirects does not crash.  See http://crbug.com/18686.

  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.Append(FILE_PATH_LITERAL("net"));
  path = path.Append(FILE_PATH_LITERAL("data"));
  path = path.Append(FILE_PATH_LITERAL("url_request_unittest"));

  TestDelegate d;
  TestURLRequest req(FilePathToFileURL(path), &d);
  req.set_context(default_context_);
  req.Start();
  MessageLoop::current()->Run();

  ASSERT_EQ(1, d.received_redirect_count());
  ASSERT_LT(0, d.bytes_received());
  ASSERT_FALSE(d.request_failed());
  ASSERT_TRUE(req.status().is_success());
}

#if defined(OS_WIN)
// Don't accept the url "file:///" on windows. See http://crbug.com/1474.
TEST_F(URLRequestTest, FileDirRedirectSingleSlash) {
  TestDelegate d;
  TestURLRequest req(GURL("file:///"), &d);
  req.set_context(default_context_);
  req.Start();
  MessageLoop::current()->Run();

  ASSERT_EQ(1, d.received_redirect_count());
  ASSERT_FALSE(req.status().is_success());
}
#endif

TEST_F(URLRequestTestHTTP, RestrictRedirects) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL(
      "files/redirect-to-file.html"), &d);
  req.set_context(default_context_);
  req.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(URLRequestStatus::FAILED, req.status().status());
  EXPECT_EQ(ERR_UNSAFE_REDIRECT, req.status().error());
}

TEST_F(URLRequestTestHTTP, RedirectToInvalidURL) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL(
      "files/redirect-to-invalid-url.html"), &d);
  req.set_context(default_context_);
  req.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(URLRequestStatus::FAILED, req.status().status());
  EXPECT_EQ(ERR_INVALID_URL, req.status().error());
}

TEST_F(URLRequestTestHTTP, NoUserPassInReferrer) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("echoheader?Referer"), &d);
  req.set_context(default_context_);
  req.set_referrer("http://user:pass@foo.com/");
  req.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(std::string("http://foo.com/"), d.data_received());
}

TEST_F(URLRequestTestHTTP, CancelRedirect) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    d.set_cancel_in_received_redirect(true);
    TestURLRequest req(test_server_.GetURL("files/redirect-test.html"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
  }
}

TEST_F(URLRequestTestHTTP, DeferredRedirect) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    d.set_quit_on_redirect(true);
    TestURLRequest req(test_server_.GetURL("files/redirect-test.html"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.received_redirect_count());

    req.FollowDeferredRedirect();
    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::SUCCESS, req.status().status());

    FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.Append(FILE_PATH_LITERAL("net"));
    path = path.Append(FILE_PATH_LITERAL("data"));
    path = path.Append(FILE_PATH_LITERAL("url_request_unittest"));
    path = path.Append(FILE_PATH_LITERAL("with-headers.html"));

    std::string contents;
    EXPECT_TRUE(file_util::ReadFileToString(path, &contents));
    EXPECT_EQ(contents, d.data_received());
  }
}

TEST_F(URLRequestTestHTTP, CancelDeferredRedirect) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    d.set_quit_on_redirect(true);
    TestURLRequest req(test_server_.GetURL("files/redirect-test.html"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.received_redirect_count());

    req.Cancel();
    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
  }
}

TEST_F(URLRequestTestHTTP, VaryHeader) {
  ASSERT_TRUE(test_server_.Start());

  // populate the cache
  {
    TestDelegate d;
    URLRequest req(test_server_.GetURL("echoheadercache?foo"), &d);
    req.set_context(default_context_);
    HttpRequestHeaders headers;
    headers.SetHeader("foo", "1");
    req.SetExtraRequestHeaders(headers);
    req.Start();
    MessageLoop::current()->Run();
  }

  // expect a cache hit
  {
    TestDelegate d;
    URLRequest req(test_server_.GetURL("echoheadercache?foo"), &d);
    req.set_context(default_context_);
    HttpRequestHeaders headers;
    headers.SetHeader("foo", "1");
    req.SetExtraRequestHeaders(headers);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(req.was_cached());
  }

  // expect a cache miss
  {
    TestDelegate d;
    URLRequest req(test_server_.GetURL("echoheadercache?foo"), &d);
    req.set_context(default_context_);
    HttpRequestHeaders headers;
    headers.SetHeader("foo", "2");
    req.SetExtraRequestHeaders(headers);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_FALSE(req.was_cached());
  }
}

TEST_F(URLRequestTestHTTP, BasicAuth) {
  ASSERT_TRUE(test_server_.Start());

  // populate the cache
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    URLRequest r(test_server_.GetURL("auth-basic"), &d);
    r.set_context(default_context_);
    r.Start();

    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);
  }

  // repeat request with end-to-end validation.  since auth-basic results in a
  // cachable page, we expect this test to result in a 304.  in which case, the
  // response should be fetched from the cache.
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    URLRequest r(test_server_.GetURL("auth-basic"), &d);
    r.set_context(default_context_);
    r.set_load_flags(LOAD_VALIDATE_CACHE);
    r.Start();

    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    // Should be the same cached document.
    EXPECT_TRUE(r.was_cached());
  }
}

// Check that Set-Cookie headers in 401 responses are respected.
// http://crbug.com/6450
TEST_F(URLRequestTestHTTP, BasicAuthWithCookies) {
  ASSERT_TRUE(test_server_.Start());

  GURL url_requiring_auth =
      test_server_.GetURL("auth-basic?set-cookie-if-challenged");

  // Request a page that will give a 401 containing a Set-Cookie header.
  // Verify that when the transaction is restarted, it includes the new cookie.
  {
    TestNetworkDelegate network_delegate;  // must outlive URLRequest
    scoped_refptr<TestURLRequestContext> context(
        new TestURLRequestContext(true));
    context->set_network_delegate(&network_delegate);
    context->Init();

    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    URLRequest r(url_requiring_auth, &d);
    r.set_context(context);
    r.Start();

    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    // Make sure we sent the cookie in the restarted transaction.
    EXPECT_TRUE(d.data_received().find("Cookie: got_challenged=true")
        != std::string::npos);
  }

  // Same test as above, except this time the restart is initiated earlier
  // (without user intervention since identity is embedded in the URL).
  {
    TestNetworkDelegate network_delegate;  // must outlive URLRequest
    scoped_refptr<TestURLRequestContext> context(
        new TestURLRequestContext(true));
    context->set_network_delegate(&network_delegate);
    context->Init();

    TestDelegate d;

    GURL::Replacements replacements;
    std::string username("user2");
    std::string password("secret");
    replacements.SetUsernameStr(username);
    replacements.SetPasswordStr(password);
    GURL url_with_identity = url_requiring_auth.ReplaceComponents(replacements);

    URLRequest r(url_with_identity, &d);
    r.set_context(context);
    r.Start();

    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("user2/secret") != std::string::npos);

    // Make sure we sent the cookie in the restarted transaction.
    EXPECT_TRUE(d.data_received().find("Cookie: got_challenged=true")
        != std::string::npos);
  }
}

TEST_F(URLRequestTest, DelayedCookieCallback) {
  TestServer test_server(TestServer::TYPE_HTTP, FilePath());
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<URLRequestContext> context(new TestURLRequestContext());
  scoped_refptr<DelayedCookieMonster> delayed_cm =
      new DelayedCookieMonster();
  scoped_refptr<CookieStore> cookie_store = delayed_cm;
  context->set_cookie_store(delayed_cm);

  // Set up a cookie.
  {
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotSend=1"), &d);
    req.set_context(context);
    req.Start();
    MessageLoop::current()->Run();
    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
    EXPECT_EQ(1, d.set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestDelegate d;
    TestURLRequest req(test_server.GetURL("echoheader?Cookie"), &d);
    req.set_context(context);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);
    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSendCookies) {
  TestServer test_server(TestServer::TYPE_HTTP, FilePath());
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotSend=1"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();
    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestDelegate d;
    TestURLRequest req(test_server.GetURL("echoheader?Cookie"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);
    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }

  // Verify that the cookie isn't sent when LOAD_DO_NOT_SEND_COOKIES is set.
  {
    TestDelegate d;
    TestURLRequest req(test_server.GetURL("echoheader?Cookie"), &d);
    req.set_load_flags(LOAD_DO_NOT_SEND_COOKIES);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1")
                == std::string::npos);

    // LOAD_DO_NOT_SEND_COOKIES does not trigger OnGetCookies.
    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveCookies) {
  TestServer test_server(TestServer::TYPE_HTTP, FilePath());
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotUpdate=2"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
    EXPECT_EQ(1, d.set_cookie_count());
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestDelegate d;
    URLRequest req(test_server.GetURL(
        "set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"), &d);
    req.set_load_flags(LOAD_DO_NOT_SAVE_COOKIES);
    req.set_context(default_context_);
    req.Start();

    MessageLoop::current()->Run();

    // LOAD_DO_NOT_SAVE_COOKIES does not trigger OnSetCookie.
    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
    EXPECT_EQ(0, d.set_cookie_count());
  }

  // Verify the cookies weren't saved or updated.
  {
    TestDelegate d;
    TestURLRequest req(test_server.GetURL("echoheader?Cookie"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1")
                == std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2")
                != std::string::npos);

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
    EXPECT_EQ(0, d.set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSendCookies_ViaPolicy) {
  TestServer test_server(TestServer::TYPE_HTTP, FilePath());
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotSend=1"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestDelegate d;
    TestURLRequest req(test_server.GetURL("echoheader?Cookie"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }

  // Verify that the cookie isn't sent.
  {
    TestDelegate d;
    d.set_cookie_options(TestDelegate::NO_GET_COOKIES);
    TestURLRequest req(test_server.GetURL("echoheader?Cookie"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1")
                == std::string::npos);

    EXPECT_EQ(1, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveCookies_ViaPolicy) {
  TestServer test_server(TestServer::TYPE_HTTP, FilePath());
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotUpdate=2"),  &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestDelegate d;
    d.set_cookie_options(TestDelegate::NO_SET_COOKIE);
    URLRequest req(test_server.GetURL(
        "set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"), &d);
    req.set_context(default_context_);
    req.Start();

    MessageLoop::current()->Run();

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(2, d.blocked_set_cookie_count());
  }


  // Verify the cookies weren't saved or updated.
  {
    TestDelegate d;
    TestURLRequest req(test_server.GetURL("echoheader?Cookie"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1")
                == std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2")
                != std::string::npos);

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveEmptyCookies) {
  TestServer test_server(TestServer::TYPE_HTTP, FilePath());
  ASSERT_TRUE(test_server.Start());

  // Set up an empty cookie.
  {
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
    EXPECT_EQ(0, d.set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSendCookies_ViaPolicy_Async) {
  TestServer test_server(TestServer::TYPE_HTTP, FilePath());
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotSend=1"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestDelegate d;
    TestURLRequest req(test_server.GetURL("echoheader?Cookie"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }

  // Verify that the cookie isn't sent.
  {
    TestDelegate d;
    d.set_cookie_options(TestDelegate::NO_GET_COOKIES);
    TestURLRequest req(test_server.GetURL("echoheader?Cookie"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1")
                == std::string::npos);

    EXPECT_EQ(1, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveCookies_ViaPolicy_Async) {
  TestServer test_server(TestServer::TYPE_HTTP, FilePath());
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    URLRequest req(test_server.GetURL("set-cookie?CookieToNotUpdate=2"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestDelegate d;
    d.set_cookie_options(TestDelegate::NO_SET_COOKIE);
    URLRequest req(test_server.GetURL(
        "set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"), &d);
    req.set_context(default_context_);
    req.Start();

    MessageLoop::current()->Run();

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(2, d.blocked_set_cookie_count());
  }

  // Verify the cookies weren't saved or updated.
  {
    TestDelegate d;
    TestURLRequest req(test_server.GetURL("echoheader?Cookie"), &d);
    req.set_context(default_context_);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1")
                == std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2")
                != std::string::npos);

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }
}

void CheckCookiePolicyCallback(bool* was_run, const CookieList& cookies) {
  EXPECT_EQ(1U, cookies.size());
  EXPECT_FALSE(cookies[0].IsPersistent());
  *was_run = true;
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

TEST_F(URLRequestTest, CookiePolicy_ForceSession) {
  TestServer test_server(TestServer::TYPE_HTTP, FilePath());
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    d.set_cookie_options(TestDelegate::FORCE_SESSION);
    URLRequest req(test_server.GetURL(
        "set-cookie?A=1;expires=\"Fri, 05 Feb 2010 23:42:01 GMT\""), &d);
    req.set_context(default_context_);
    req.Start();  // Triggers an asynchronous cookie policy check.

    MessageLoop::current()->Run();

    EXPECT_EQ(0, d.blocked_get_cookies_count());
    EXPECT_EQ(0, d.blocked_set_cookie_count());
  }

  // Now, check the cookie store.
  bool was_run = false;
  default_context_->cookie_store()->GetCookieMonster()->GetAllCookiesAsync(
      base::Bind(&CheckCookiePolicyCallback, &was_run));
  MessageLoop::current()->RunAllPending();
  DCHECK(was_run);
}

// In this test, we do a POST which the server will 302 redirect.
// The subsequent transaction should use GET, and should not send the
// Content-Type header.
// http://code.google.com/p/chromium/issues/detail?id=843
TEST_F(URLRequestTestHTTP, Post302RedirectGet) {
  ASSERT_TRUE(test_server_.Start());

  const char kData[] = "hello world";

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("files/redirect-to-echoall"), &d);
  req.set_context(default_context_);
  req.set_method("POST");
  req.set_upload(CreateSimpleUploadData(kData));

  // Set headers (some of which are specific to the POST).
  HttpRequestHeaders headers;
  headers.AddHeadersFromString(
    "Content-Type: multipart/form-data; "
    "boundary=----WebKitFormBoundaryAADeAA+NAAWMAAwZ\r\n"
    "Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,"
    "text/plain;q=0.8,image/png,*/*;q=0.5\r\n"
    "Accept-Language: en-US,en\r\n"
    "Accept-Charset: ISO-8859-1,*,utf-8\r\n"
    "Content-Length: 11\r\n"
    "Origin: http://localhost:1337/");
  req.SetExtraRequestHeaders(headers);
  req.Start();
  MessageLoop::current()->Run();

  std::string mime_type;
  req.GetMimeType(&mime_type);
  EXPECT_EQ("text/html", mime_type);

  const std::string& data = d.data_received();

  // Check that the post-specific headers were stripped:
  EXPECT_FALSE(ContainsString(data, "Content-Length:"));
  EXPECT_FALSE(ContainsString(data, "Content-Type:"));
  EXPECT_FALSE(ContainsString(data, "Origin:"));

  // These extra request headers should not have been stripped.
  EXPECT_TRUE(ContainsString(data, "Accept:"));
  EXPECT_TRUE(ContainsString(data, "Accept-Language:"));
  EXPECT_TRUE(ContainsString(data, "Accept-Charset:"));
}

// The following tests check that we handle mutating the request method for
// HTTP redirects as expected.
// See http://crbug.com/56373 and http://crbug.com/102130.

TEST_F(URLRequestTestHTTP, Redirect301Tests) {
  ASSERT_TRUE(test_server_.Start());

  const GURL url = test_server_.GetURL("files/redirect301-to-echo");

  HTTPRedirectMethodTest(url, "POST", "GET", true);
  HTTPRedirectMethodTest(url, "PUT", "PUT", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);
}

TEST_F(URLRequestTestHTTP, Redirect302Tests) {
  ASSERT_TRUE(test_server_.Start());

  const GURL url = test_server_.GetURL("files/redirect302-to-echo");

  HTTPRedirectMethodTest(url, "POST", "GET", true);
  HTTPRedirectMethodTest(url, "PUT", "PUT", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);
}

TEST_F(URLRequestTestHTTP, Redirect303Tests) {
  ASSERT_TRUE(test_server_.Start());

  const GURL url = test_server_.GetURL("files/redirect303-to-echo");

  HTTPRedirectMethodTest(url, "POST", "GET", true);
  HTTPRedirectMethodTest(url, "PUT", "GET", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);
}

TEST_F(URLRequestTestHTTP, Redirect307Tests) {
  ASSERT_TRUE(test_server_.Start());

  const GURL url = test_server_.GetURL("files/redirect307-to-echo");

  HTTPRedirectMethodTest(url, "POST", "POST", true);
  HTTPRedirectMethodTest(url, "PUT", "PUT", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);
}

TEST_F(URLRequestTestHTTP, InterceptPost302RedirectGet) {
  ASSERT_TRUE(test_server_.Start());

  const char kData[] = "hello world";

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("empty.html"), &d);
  req.set_context(default_context_);
  req.set_method("POST");
  req.set_upload(CreateSimpleUploadData(kData).get());
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kContentLength,
                    base::UintToString(arraysize(kData) - 1));
  req.SetExtraRequestHeaders(headers);

  URLRequestRedirectJob* job =
      new URLRequestRedirectJob(&req, test_server_.GetURL("echo"));
  AddTestInterceptor()->set_main_intercept_job(job);

  req.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ("GET", req.method());
}

TEST_F(URLRequestTestHTTP, InterceptPost307RedirectPost) {
  ASSERT_TRUE(test_server_.Start());

  const char kData[] = "hello world";

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("empty.html"), &d);
  req.set_context(default_context_);
  req.set_method("POST");
  req.set_upload(CreateSimpleUploadData(kData).get());
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kContentLength,
                    base::UintToString(arraysize(kData) - 1));
  req.SetExtraRequestHeaders(headers);

  URLRequestRedirectJob* job =
      new URLRequestRedirectJob(&req, test_server_.GetURL("echo"));
  job->set_redirect_code(
      URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT);
  AddTestInterceptor()->set_main_intercept_job(job);

  req.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ("POST", req.method());
  EXPECT_EQ(kData, d.data_received());
}

// Custom URLRequestJobs for use with interceptor tests
class RestartTestJob : public URLRequestTestJob {
 public:
  explicit RestartTestJob(URLRequest* request)
    : URLRequestTestJob(request, true) {}
 protected:
  virtual void StartAsync() {
    this->NotifyRestartRequired();
  }
 private:
  ~RestartTestJob() {}
};

class CancelTestJob : public URLRequestTestJob {
 public:
  explicit CancelTestJob(URLRequest* request)
    : URLRequestTestJob(request, true) {}
 protected:
  virtual void StartAsync() {
    request_->Cancel();
  }
 private:
  ~CancelTestJob() {}
};

class CancelThenRestartTestJob : public URLRequestTestJob {
 public:
  explicit CancelThenRestartTestJob(URLRequest* request)
      : URLRequestTestJob(request, true) {
  }
 protected:
  virtual void StartAsync() {
    request_->Cancel();
    this->NotifyRestartRequired();
  }
 private:
  ~CancelThenRestartTestJob() {}
};

// An Interceptor for use with interceptor tests
class TestInterceptor : URLRequest::Interceptor {
 public:
  TestInterceptor()
      : intercept_main_request_(false), restart_main_request_(false),
        cancel_main_request_(false), cancel_then_restart_main_request_(false),
        simulate_main_network_error_(false),
        intercept_redirect_(false), cancel_redirect_request_(false),
        intercept_final_response_(false), cancel_final_request_(false),
        did_intercept_main_(false), did_restart_main_(false),
        did_cancel_main_(false), did_cancel_then_restart_main_(false),
        did_simulate_error_main_(false),
        did_intercept_redirect_(false), did_cancel_redirect_(false),
        did_intercept_final_(false), did_cancel_final_(false) {
    URLRequest::Deprecated::RegisterRequestInterceptor(this);
  }

  ~TestInterceptor() {
    URLRequest::Deprecated::UnregisterRequestInterceptor(this);
  }

  virtual URLRequestJob* MaybeIntercept(URLRequest* request) {
    if (restart_main_request_) {
      restart_main_request_ = false;
      did_restart_main_ = true;
      return new RestartTestJob(request);
    }
    if (cancel_main_request_) {
      cancel_main_request_ = false;
      did_cancel_main_ = true;
      return new CancelTestJob(request);
    }
    if (cancel_then_restart_main_request_) {
      cancel_then_restart_main_request_ = false;
      did_cancel_then_restart_main_ = true;
      return new CancelThenRestartTestJob(request);
    }
    if (simulate_main_network_error_) {
      simulate_main_network_error_ = false;
      did_simulate_error_main_ = true;
      // will error since the requeted url is not one of its canned urls
      return new URLRequestTestJob(request, true);
    }
    if (!intercept_main_request_)
      return NULL;
    intercept_main_request_ = false;
    did_intercept_main_ = true;
    return new URLRequestTestJob(request,
                                      main_headers_,
                                      main_data_,
                                      true);
  }

  virtual URLRequestJob* MaybeInterceptRedirect(URLRequest* request,
                                                     const GURL& location) {
    if (cancel_redirect_request_) {
      cancel_redirect_request_ = false;
      did_cancel_redirect_ = true;
      return new CancelTestJob(request);
    }
    if (!intercept_redirect_)
      return NULL;
    intercept_redirect_ = false;
    did_intercept_redirect_ = true;
    return new URLRequestTestJob(request,
                                      redirect_headers_,
                                      redirect_data_,
                                      true);
  }

  virtual URLRequestJob* MaybeInterceptResponse(URLRequest* request) {
    if (cancel_final_request_) {
      cancel_final_request_ = false;
      did_cancel_final_ = true;
      return new CancelTestJob(request);
    }
    if (!intercept_final_response_)
      return NULL;
    intercept_final_response_ = false;
    did_intercept_final_ = true;
    return new URLRequestTestJob(request,
                                      final_headers_,
                                      final_data_,
                                      true);
  }

  // Whether to intercept the main request, and if so the response to return.
  bool intercept_main_request_;
  std::string main_headers_;
  std::string main_data_;

  // Other actions we take at MaybeIntercept time
  bool restart_main_request_;
  bool cancel_main_request_;
  bool cancel_then_restart_main_request_;
  bool simulate_main_network_error_;

  // Whether to intercept redirects, and if so the response to return.
  bool intercept_redirect_;
  std::string redirect_headers_;
  std::string redirect_data_;

  // Other actions we can take at MaybeInterceptRedirect time
  bool cancel_redirect_request_;

  // Whether to intercept final response, and if so the response to return.
  bool intercept_final_response_;
  std::string final_headers_;
  std::string final_data_;

  // Other actions we can take at MaybeInterceptResponse time
  bool cancel_final_request_;

  // If we did something or not
  bool did_intercept_main_;
  bool did_restart_main_;
  bool did_cancel_main_;
  bool did_cancel_then_restart_main_;
  bool did_simulate_error_main_;
  bool did_intercept_redirect_;
  bool did_cancel_redirect_;
  bool did_intercept_final_;
  bool did_cancel_final_;

  // Static getters for canned response header and data strings

  static std::string ok_data() {
    return URLRequestTestJob::test_data_1();
  }

  static std::string ok_headers() {
    return URLRequestTestJob::test_headers();
  }

  static std::string redirect_data() {
    return std::string();
  }

  static std::string redirect_headers() {
    return URLRequestTestJob::test_redirect_headers();
  }

  static std::string error_data() {
    return std::string("ohhh nooooo mr. bill!");
  }

  static std::string error_headers() {
    return URLRequestTestJob::test_error_headers();
  }
};

TEST_F(URLRequestTest, Intercept) {
  TestInterceptor interceptor;

  // intercept the main request and respond with a simple response
  interceptor.intercept_main_request_ = true;
  interceptor.main_headers_ = TestInterceptor::ok_headers();
  interceptor.main_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  TestURLRequest req(GURL("http://test_intercept/foo"), &d);
  req.set_context(default_context_);
  URLRequest::UserData* user_data0 = new URLRequest::UserData();
  URLRequest::UserData* user_data1 = new URLRequest::UserData();
  URLRequest::UserData* user_data2 = new URLRequest::UserData();
  req.SetUserData(NULL, user_data0);
  req.SetUserData(&user_data1, user_data1);
  req.SetUserData(&user_data2, user_data2);
  req.set_method("GET");
  req.Start();
  MessageLoop::current()->Run();

  // Make sure we can retrieve our specific user data
  EXPECT_EQ(user_data0, req.GetUserData(NULL));
  EXPECT_EQ(user_data1, req.GetUserData(&user_data1));
  EXPECT_EQ(user_data2, req.GetUserData(&user_data2));

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_intercept_main_);

  // Check we got one good response
  EXPECT_TRUE(req.status().is_success());
  EXPECT_EQ(200, req.response_headers()->response_code());
  EXPECT_EQ(TestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestTest, InterceptRedirect) {
  TestInterceptor interceptor;

  // intercept the main request and respond with a redirect
  interceptor.intercept_main_request_ = true;
  interceptor.main_headers_ = TestInterceptor::redirect_headers();
  interceptor.main_data_ = TestInterceptor::redirect_data();

  // intercept that redirect and respond a final OK response
  interceptor.intercept_redirect_ = true;
  interceptor.redirect_headers_ =  TestInterceptor::ok_headers();
  interceptor.redirect_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  TestURLRequest req(GURL("http://test_intercept/foo"), &d);
  req.set_context(default_context_);
  req.set_method("GET");
  req.Start();
  MessageLoop::current()->Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_intercept_main_);
  EXPECT_TRUE(interceptor.did_intercept_redirect_);

  // Check we got one good response
  EXPECT_TRUE(req.status().is_success());
  if (req.status().is_success()) {
    EXPECT_EQ(200, req.response_headers()->response_code());
  }
  EXPECT_EQ(TestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestTest, InterceptServerError) {
  TestInterceptor interceptor;

  // intercept the main request to generate a server error response
  interceptor.intercept_main_request_ = true;
  interceptor.main_headers_ = TestInterceptor::error_headers();
  interceptor.main_data_ = TestInterceptor::error_data();

  // intercept that error and respond with an OK response
  interceptor.intercept_final_response_ = true;
  interceptor.final_headers_ = TestInterceptor::ok_headers();
  interceptor.final_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  TestURLRequest req(GURL("http://test_intercept/foo"), &d);
  req.set_context(default_context_);
  req.set_method("GET");
  req.Start();
  MessageLoop::current()->Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_intercept_main_);
  EXPECT_TRUE(interceptor.did_intercept_final_);

  // Check we got one good response
  EXPECT_TRUE(req.status().is_success());
  EXPECT_EQ(200, req.response_headers()->response_code());
  EXPECT_EQ(TestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestTest, InterceptNetworkError) {
  TestInterceptor interceptor;

  // intercept the main request to simulate a network error
  interceptor.simulate_main_network_error_ = true;

  // intercept that error and respond with an OK response
  interceptor.intercept_final_response_ = true;
  interceptor.final_headers_ = TestInterceptor::ok_headers();
  interceptor.final_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  TestURLRequest req(GURL("http://test_intercept/foo"), &d);
  req.set_context(default_context_);
  req.set_method("GET");
  req.Start();
  MessageLoop::current()->Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_simulate_error_main_);
  EXPECT_TRUE(interceptor.did_intercept_final_);

  // Check we received one good response
  EXPECT_TRUE(req.status().is_success());
  EXPECT_EQ(200, req.response_headers()->response_code());
  EXPECT_EQ(TestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestTest, InterceptRestartRequired) {
  TestInterceptor interceptor;

  // restart the main request
  interceptor.restart_main_request_ = true;

  // then intercept the new main request and respond with an OK response
  interceptor.intercept_main_request_ = true;
  interceptor.main_headers_ = TestInterceptor::ok_headers();
  interceptor.main_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  TestURLRequest req(GURL("http://test_intercept/foo"), &d);
  req.set_context(default_context_);
  req.set_method("GET");
  req.Start();
  MessageLoop::current()->Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_restart_main_);
  EXPECT_TRUE(interceptor.did_intercept_main_);

  // Check we received one good response
  EXPECT_TRUE(req.status().is_success());
  if (req.status().is_success()) {
    EXPECT_EQ(200, req.response_headers()->response_code());
  }
  EXPECT_EQ(TestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestTest, InterceptRespectsCancelMain) {
  TestInterceptor interceptor;

  // intercept the main request and cancel from within the restarted job
  interceptor.cancel_main_request_ = true;

  // setup to intercept final response and override it with an OK response
  interceptor.intercept_final_response_ = true;
  interceptor.final_headers_ = TestInterceptor::ok_headers();
  interceptor.final_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  TestURLRequest req(GURL("http://test_intercept/foo"), &d);
  req.set_context(default_context_);
  req.set_method("GET");
  req.Start();
  MessageLoop::current()->Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_cancel_main_);
  EXPECT_FALSE(interceptor.did_intercept_final_);

  // Check we see a canceled request
  EXPECT_FALSE(req.status().is_success());
  EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
}

TEST_F(URLRequestTest, InterceptRespectsCancelRedirect) {
  TestInterceptor interceptor;

  // intercept the main request and respond with a redirect
  interceptor.intercept_main_request_ = true;
  interceptor.main_headers_ = TestInterceptor::redirect_headers();
  interceptor.main_data_ = TestInterceptor::redirect_data();

  // intercept the redirect and cancel from within that job
  interceptor.cancel_redirect_request_ = true;

  // setup to intercept final response and override it with an OK response
  interceptor.intercept_final_response_ = true;
  interceptor.final_headers_ = TestInterceptor::ok_headers();
  interceptor.final_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  TestURLRequest req(GURL("http://test_intercept/foo"), &d);
  req.set_context(default_context_);
  req.set_method("GET");
  req.Start();
  MessageLoop::current()->Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_intercept_main_);
  EXPECT_TRUE(interceptor.did_cancel_redirect_);
  EXPECT_FALSE(interceptor.did_intercept_final_);

  // Check we see a canceled request
  EXPECT_FALSE(req.status().is_success());
  EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
}

TEST_F(URLRequestTest, InterceptRespectsCancelFinal) {
  TestInterceptor interceptor;

  // intercept the main request to simulate a network error
  interceptor.simulate_main_network_error_ = true;

  // setup to intercept final response and cancel from within that job
  interceptor.cancel_final_request_ = true;

  TestDelegate d;
  TestURLRequest req(GURL("http://test_intercept/foo"), &d);
  req.set_context(default_context_);
  req.set_method("GET");
  req.Start();
  MessageLoop::current()->Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_simulate_error_main_);
  EXPECT_TRUE(interceptor.did_cancel_final_);

  // Check we see a canceled request
  EXPECT_FALSE(req.status().is_success());
  EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
}

TEST_F(URLRequestTest, InterceptRespectsCancelInRestart) {
  TestInterceptor interceptor;

  // intercept the main request and cancel then restart from within that job
  interceptor.cancel_then_restart_main_request_ = true;

  // setup to intercept final response and override it with an OK response
  interceptor.intercept_final_response_ = true;
  interceptor.final_headers_ = TestInterceptor::ok_headers();
  interceptor.final_data_ = TestInterceptor::ok_data();

  TestDelegate d;
  TestURLRequest req(GURL("http://test_intercept/foo"), &d);
  req.set_context(default_context_);
  req.set_method("GET");
  req.Start();
  MessageLoop::current()->Run();

  // Check the interceptor got called as expected
  EXPECT_TRUE(interceptor.did_cancel_then_restart_main_);
  EXPECT_FALSE(interceptor.did_intercept_final_);

  // Check we see a canceled request
  EXPECT_FALSE(req.status().is_success());
  EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
}

// Check that two different URL requests have different identifiers.
TEST_F(URLRequestTest, Identifiers) {
  TestDelegate d;
  TestURLRequest req(GURL("http://example.com"), &d);
  TestURLRequest other_req(GURL("http://example.com"), &d);

  ASSERT_NE(req.identifier(), other_req.identifier());
}

// Check that a failure to connect to the proxy is reported to the network
// delegate.
TEST_F(URLRequestTest, NetworkDelegateProxyError) {
  MockHostResolver host_resolver;
  host_resolver.rules()->AddSimulatedFailure("*");

  TestNetworkDelegate network_delegate;  // must outlive URLRequests
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->SetProxyFromString("myproxy:70");
  context->set_host_resolver(&host_resolver);
  context->Init();

  TestDelegate d;
  TestURLRequest req(GURL("http://example.com"), &d);
  req.set_context(context);
  req.set_method("GET");

  req.Start();
  MessageLoop::current()->Run();

  // Check we see a failed request.
  EXPECT_FALSE(req.status().is_success());
  EXPECT_EQ(URLRequestStatus::FAILED, req.status().status());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, req.status().error());

  EXPECT_EQ(1, network_delegate.error_count());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, network_delegate.last_error());
  EXPECT_EQ(1, network_delegate.completed_requests());
}

// Check that it is impossible to change the referrer in the extra headers of
// an URLRequest.
TEST_F(URLRequestTest, DoNotOverrideReferrer) {
  TestServer test_server(TestServer::TYPE_HTTP, FilePath());
  ASSERT_TRUE(test_server.Start());

  // If extra headers contain referer and the request contains a referer,
  // only the latter shall be respected.
  {
    TestDelegate d;
    TestURLRequest req(test_server.GetURL("echoheader?Referer"), &d);
    req.set_referrer("http://foo.com/");
    req.set_context(default_context_);

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kReferer, "http://bar.com/");
    req.SetExtraRequestHeaders(headers);

    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ("http://foo.com/", d.data_received());
  }

  // If extra headers contain a referer but the request does not, no referer
  // shall be sent in the header.
  {
    TestDelegate d;
    TestURLRequest req(test_server.GetURL("echoheader?Referer"), &d);
    req.set_context(default_context_);

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kReferer, "http://bar.com/");
    req.SetExtraRequestHeaders(headers);
    req.set_load_flags(LOAD_VALIDATE_CACHE);

    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ("None", d.data_received());
  }
}

// Make sure that net::NetworkDelegate::NotifyCompleted is called if
// content is empty.
TEST_F(URLRequestTest, RequestCompletionForEmptyResponse) {
  TestDelegate d;
  TestURLRequest req(GURL("data:,"), &d);
  req.set_context(new TestURLRequestContext());
  req.set_context(default_context_);
  req.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ("", d.data_received());
  EXPECT_EQ(1, default_network_delegate_.completed_requests());
}

class URLRequestTestFTP : public URLRequestTest {
 public:
  URLRequestTestFTP() : test_server_(TestServer::TYPE_FTP, FilePath()) {
  }

 protected:
  TestServer test_server_;
};

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, FLAKY_FTPDirectoryListing) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  {
    TestURLRequest r(test_server_.GetURL("/"), &d);
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_LT(0, d.bytes_received());
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, FLAKY_FTPGetTestAnonymous) {
  ASSERT_TRUE(test_server_.Start());

  FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  {
    TestURLRequest r(test_server_.GetURL("/LICENSE"), &d);
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, FLAKY_FTPGetTest) {
  ASSERT_TRUE(test_server_.Start());

  FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  {
    TestURLRequest r(
        test_server_.GetURLWithUserAndPassword("/LICENSE", "chrome", "chrome"),
        &d);
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(test_server_.host_port_pair().host(),
              r.GetSocketAddress().host());
    EXPECT_EQ(test_server_.host_port_pair().port(),
              r.GetSocketAddress().port());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, FLAKY_FTPCheckWrongPassword) {
  ASSERT_TRUE(test_server_.Start());

  FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  {
    TestURLRequest r(
        test_server_.GetURLWithUserAndPassword("/LICENSE",
                                               "chrome",
                                               "wrong_password"),
        &d);
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 0);
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, FLAKY_FTPCheckWrongPasswordRestart) {
  ASSERT_TRUE(test_server_.Start());

  FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  // Set correct login credentials. The delegate will be asked for them when
  // the initial login with wrong credentials will fail.
  d.set_credentials(AuthCredentials(kChrome, kChrome));
  {
    TestURLRequest r(
        test_server_.GetURLWithUserAndPassword("/LICENSE",
                                               "chrome",
                                               "wrong_password"),
        &d);
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, FLAKY_FTPCheckWrongUser) {
  ASSERT_TRUE(test_server_.Start());

  FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  {
    TestURLRequest r(
        test_server_.GetURLWithUserAndPassword("/LICENSE",
                                               "wrong_user",
                                               "chrome"),
        &d);
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 0);
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, FLAKY_FTPCheckWrongUserRestart) {
  ASSERT_TRUE(test_server_.Start());

  FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");
  TestDelegate d;
  // Set correct login credentials. The delegate will be asked for them when
  // the initial login with wrong credentials will fail.
  d.set_credentials(AuthCredentials(kChrome, kChrome));
  {
    TestURLRequest r(
        test_server_.GetURLWithUserAndPassword("/LICENSE",
                                               "wrong_user",
                                               "chrome"),
        &d);
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, FLAKY_FTPCacheURLCredentials) {
  ASSERT_TRUE(test_server_.Start());

  FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");

  scoped_ptr<TestDelegate> d(new TestDelegate);
  {
    // Pass correct login identity in the URL.
    TestURLRequest r(
        test_server_.GetURLWithUserAndPassword("/LICENSE",
                                               "chrome",
                                               "chrome"),
        d.get());
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(d->bytes_received(), static_cast<int>(file_size));
  }

  d.reset(new TestDelegate);
  {
    // This request should use cached identity from previous request.
    TestURLRequest r(test_server_.GetURL("/LICENSE"), d.get());
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(d->bytes_received(), static_cast<int>(file_size));
  }
}

// Flaky, see http://crbug.com/25045.
TEST_F(URLRequestTestFTP, FLAKY_FTPCacheLoginBoxCredentials) {
  ASSERT_TRUE(test_server_.Start());

  FilePath app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.AppendASCII("LICENSE");

  scoped_ptr<TestDelegate> d(new TestDelegate);
  // Set correct login credentials. The delegate will be asked for them when
  // the initial login with wrong credentials will fail.
  d->set_credentials(AuthCredentials(kChrome, kChrome));
  {
    TestURLRequest r(
        test_server_.GetURLWithUserAndPassword("/LICENSE",
                                               "chrome",
                                               "wrong_password"),
        d.get());
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(d->bytes_received(), static_cast<int>(file_size));
  }

  // Use a new delegate without explicit credentials. The cached ones should be
  // used.
  d.reset(new TestDelegate);
  {
    // Don't pass wrong credentials in the URL, they would override valid cached
    // ones.
    TestURLRequest r(test_server_.GetURL("/LICENSE"), d.get());
    r.set_context(default_context_);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_FALSE(r.is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(d->bytes_received(), static_cast<int>(file_size));
  }
}

// Check that default A-L header is sent.
TEST_F(URLRequestTestHTTP, DefaultAcceptLanguage) {
  ASSERT_TRUE(test_server_.Start());

  TestNetworkDelegate network_delegate;  // must outlive URLRequests
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->set_accept_language("en");
  context->Init();

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("echoheader?Accept-Language"), &d);
  req.set_context(context);
  req.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ("en", d.data_received());
}

// Check that an empty A-L header is not sent. http://crbug.com/77365.
TEST_F(URLRequestTestHTTP, EmptyAcceptLanguage) {
  ASSERT_TRUE(test_server_.Start());

  TestNetworkDelegate network_delegate;  // must outlive URLRequests
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->Init();
  // We override the language after initialization because empty entries
  // get overridden by Init().
  context->set_accept_language("");

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("echoheader?Accept-Language"), &d);
  req.set_context(context);
  req.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ("None", d.data_received());
}

// Check that if request overrides the A-L header, the default is not appended.
// See http://crbug.com/20894
TEST_F(URLRequestTestHTTP, OverrideAcceptLanguage) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("echoheader?Accept-Language"), &d);
  req.set_context(default_context_);
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kAcceptLanguage, "ru");
  req.SetExtraRequestHeaders(headers);
  req.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(std::string("ru"), d.data_received());
}

// Check that default A-E header is sent.
TEST_F(URLRequestTestHTTP, DefaultAcceptEncoding) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("echoheader?Accept-Encoding"), &d);
  req.set_context(default_context_);
  HttpRequestHeaders headers;
  req.SetExtraRequestHeaders(headers);
  req.Start();
  MessageLoop::current()->Run();
  EXPECT_TRUE(ContainsString(d.data_received(), "gzip"));
}

// Check that if request overrides the A-E header, the default is not appended.
// See http://crbug.com/47381
TEST_F(URLRequestTestHTTP, OverrideAcceptEncoding) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("echoheader?Accept-Encoding"), &d);
  req.set_context(default_context_);
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kAcceptEncoding, "identity");
  req.SetExtraRequestHeaders(headers);
  req.Start();
  MessageLoop::current()->Run();
  EXPECT_FALSE(ContainsString(d.data_received(), "gzip"));
  EXPECT_TRUE(ContainsString(d.data_received(), "identity"));
}

// Check that default A-C header is sent.
TEST_F(URLRequestTestHTTP, DefaultAcceptCharset) {
  ASSERT_TRUE(test_server_.Start());

  TestNetworkDelegate network_delegate;  // must outlive URLRequests
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->set_accept_charset("en");
  context->Init();

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("echoheader?Accept-Charset"), &d);
  req.set_context(context);
  req.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ("en", d.data_received());
}

// Check that an empty A-C header is not sent. http://crbug.com/77365.
TEST_F(URLRequestTestHTTP, EmptyAcceptCharset) {
  ASSERT_TRUE(test_server_.Start());

  TestNetworkDelegate network_delegate;  // must outlive URLRequests
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext(true));
  context->set_network_delegate(&network_delegate);
  context->Init();
  // We override the accepted charset after initialization because empty
  // entries get overridden otherwise.
  context->set_accept_charset("");

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("echoheader?Accept-Charset"), &d);
  req.set_context(context);
  req.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ("None", d.data_received());
}

// Check that if request overrides the A-C header, the default is not appended.
// See http://crbug.com/20894
TEST_F(URLRequestTestHTTP, OverrideAcceptCharset) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("echoheader?Accept-Charset"), &d);
  req.set_context(default_context_);
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kAcceptCharset, "koi-8r");
  req.SetExtraRequestHeaders(headers);
  req.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(std::string("koi-8r"), d.data_received());
}

// Check that default User-Agent header is sent.
TEST_F(URLRequestTestHTTP, DefaultUserAgent) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("echoheader?User-Agent"), &d);
  req.set_context(default_context_);
  req.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(req.context()->GetUserAgent(req.url()), d.data_received());
}

// Check that if request overrides the User-Agent header,
// the default is not appended.
TEST_F(URLRequestTestHTTP, OverrideUserAgent) {
  ASSERT_TRUE(test_server_.Start());

  TestDelegate d;
  TestURLRequest req(test_server_.GetURL("echoheader?User-Agent"), &d);
  req.set_context(default_context_);
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kUserAgent, "Lynx (textmode)");
  req.SetExtraRequestHeaders(headers);
  req.Start();
  MessageLoop::current()->Run();
  // If the net tests are being run with ChromeFrame then we need to allow for
  // the 'chromeframe' suffix which is added to the user agent before the
  // closing parentheses.
  EXPECT_TRUE(StartsWithASCII(d.data_received(), "Lynx (textmode", true));
}

}  // namespace net
