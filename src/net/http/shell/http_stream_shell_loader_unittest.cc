// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#if __LB_ENABLE_NATIVE_HTTP_STACK__

#include "net/http/shell/http_stream_shell_loader.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/http/http_network_session.h"  // HttpNetworkSession::Params
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

const size_t kOutputSize = 1024;  // Just large enough for this test.
// The number of bytes that can fit in a buffer of kOutputSize.
const size_t kMaxPayloadSize =
    kOutputSize - HttpStreamShellLoader::kChunkHeaderFooterSize;

// Test object for HttpStreamShellLoader
class HttpStreamShellLoaderLiveTest : public PlatformTest {
 public:
  HttpStreamShellLoaderLiveTest() : io_thread_("IOThread"),
                                    event_(false, false),
                                    using_proxy_(false) {}

  virtual void SetUp() {
    net::HttpStreamShellLoaderGlobalInit();
    MessageLoop::current()->RunUntilIdle();
    io_thread_.Start();
  }

  virtual void TearDown() {
    io_thread_.Stop();
    MessageLoop::current()->RunUntilIdle();
    net::HttpStreamShellLoaderGlobalDeinit();
  }

  void ExecOpenConnection() {
    DCHECK(io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    loader_ = net::CreateHttpStreamShellLoader();
    if (loader_) {
      res_ = loader_->Open(&request_info_, net_log_);
    } else {
      res_ = ERR_UNEXPECTED;
    }
    event_.Signal();
  }

  // Open/close connection
  bool OpenConnection() {
    DCHECK(!io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    io_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&HttpStreamShellLoaderLiveTest::ExecOpenConnection,
                   base::Unretained(this)));
    event_.Wait();
    return res_;
  }

  void ExecCloseConnection() {
    DCHECK(io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    loader_->Close(true);
    event_.Signal();
  }

  void CloseConnection() {
    DCHECK(!io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    io_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&HttpStreamShellLoaderLiveTest::ExecCloseConnection,
                   base::Unretained(this)));
    event_.Wait();
    loader_ = NULL;
  }

  void ExecProxyResolver() {
    DCHECK(io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    // Use the system proxy settings.
    scoped_ptr<net::ProxyConfigService> proxy_config_service(
        net::ProxyService::CreateSystemProxyConfigService(
             NULL, MessageLoop::current()));
    network_params_.proxy_service =
        net::ProxyService::CreateUsingSystemProxyResolver(
            proxy_config_service.release(), 0, NULL);
    // Send out resolve request
    CompletionCallback callback(
        base::Bind(&HttpStreamShellLoaderLiveTest::OnCompletion,
                   base::Unretained(this)));
    // Resolve proxy
    int res = network_params_.proxy_service->ResolveProxy(request_info_.url,
        &proxy_info_, callback, NULL, net_log_);
  }

  void OnCompletion(int result) {
    DCHECK(io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    EXPECT_EQ(OK, result);
    event_.Signal();
  }

  int ResolveProxy() {
    DCHECK(!io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    if (request_info_.load_flags & LOAD_BYPASS_PROXY) {
      proxy_info_.UseDirect();
      return OK;
    }

    io_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&HttpStreamShellLoaderLiveTest::ExecProxyResolver,
                   base::Unretained(this)));
    event_.Wait();

    // Send proxy information to stream.
    using_proxy_ = !proxy_info_.is_empty() &&
                   (proxy_info_.is_http() || proxy_info_.is_https());
    if (using_proxy_) {
      // Remove unsupported proxies from the list.
      proxy_info_.RemoveProxiesWithoutScheme(ProxyServer::SCHEME_DIRECT |
          ProxyServer::SCHEME_HTTP | ProxyServer::SCHEME_HTTPS);
      if (proxy_info_.is_empty()) {
        // No proxies/direct to choose from. This happens when we don't support
        // any of the proxies in the returned list.
        return ERR_NO_SUPPORTED_PROXIES;
      }
      loader_->SetProxy(&proxy_info_);
    } else {
      loader_->SetProxy(NULL);
    }
    return OK;
  }

  void OnCallbackReceived(int result) {
    DCHECK(io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    res_ = result;
    event_.Signal();
  }

  void ExecSendRequest() {
    DCHECK(io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    CompletionCallback callback(
        base::Bind(&HttpStreamShellLoaderLiveTest::OnCallbackReceived,
                   base::Unretained(this)));
    res_ = loader_->SendRequest(request_line_,
        request_headers_, &response_, callback);
    if (res_ != ERR_IO_PENDING) {
      event_.Signal();
    }
  }

  int SendRequest() {
    DCHECK(!io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    DCHECK(loader_);
    io_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&HttpStreamShellLoaderLiveTest::ExecSendRequest,
                   base::Unretained(this)));
    event_.Wait();
    if (res_ != OK) {
      io_thread_.message_loop()->PostTask(FROM_HERE,
          base::Bind(&HttpStreamShellLoaderLiveTest::ExecCloseConnection,
                     base::Unretained(this)));
      event_.Wait();
    }
    return res_;
  }

  void ExecReadResponseHeaders() {
    DCHECK(io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    CompletionCallback callback(
        base::Bind(&HttpStreamShellLoaderLiveTest::OnCallbackReceived,
                   base::Unretained(this)));
    res_ = loader_->ReadResponseHeaders(callback);
    if (res_ != ERR_IO_PENDING) {
      event_.Signal();
    }
  }

  // Read response headers and wait for the result
  int ReadResponseHeaders() {
    DCHECK(!io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    DCHECK(loader_);
    io_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&HttpStreamShellLoaderLiveTest::ExecReadResponseHeaders,
                   base::Unretained(this)));
    event_.Wait();
    if (res_ != OK) {
      io_thread_.message_loop()->PostTask(FROM_HERE,
          base::Bind(&HttpStreamShellLoaderLiveTest::ExecCloseConnection,
                     base::Unretained(this)));
      event_.Wait();
    }
    return res_;
  }

  void ExecReadResponseBody(scoped_refptr<IOBuffer> buf, size_t read_len) {
    DCHECK(io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    CompletionCallback callback(
        base::Bind(&HttpStreamShellLoaderLiveTest::OnCallbackReceived,
                   base::Unretained(this)));
    res_ = loader_->ReadResponseBody(buf, read_len, callback);
    if (res_ != ERR_IO_PENDING) {
      event_.Signal();
    }
  }

  void CheckResponseBodyComplete() {
    DCHECK(io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    EXPECT_TRUE(loader_->IsResponseBodyComplete());
    event_.Signal();
  }

  // Read data and wait for result
  int ReadResponseBody(scoped_refptr<IOBuffer> buf, size_t read_len) {
    DCHECK(!io_thread_.message_loop_proxy()->BelongsToCurrentThread());
    DCHECK(loader_);
    io_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&HttpStreamShellLoaderLiveTest::ExecReadResponseBody,
                   base::Unretained(this), buf, read_len));
    event_.Wait();
    EXPECT_GE(res_, 0);
    if (res_ == OK) {
      io_thread_.message_loop()->PostTask(FROM_HERE,
          base::Bind(&HttpStreamShellLoaderLiveTest::CheckResponseBodyComplete,
                     base::Unretained(this)));
      event_.Wait();
    }
    return res_;
  }

  // Generate request_info_, request_line_ and request_headers_.
  void GenerateRequstInfo(const char* method, const char* scheme,
                          const char* host, const char* path) {
    std::string url_str = StringPrintf("%s://%s%s", scheme, host, path);
    request_info_.url = GURL(url_str);
    request_info_.method = method;

    request_line_ = StringPrintf("%s %s HTTP/1.1", method, path);

    std::string headers_str = StringPrintf(
        "User-Agent: Mozilla/5.0 [en] (Test)\r\n"
        "Host: %s\r\n"
        "Accept: */*\r\n"
        "Accept-Encoding: gzip\r\n"
        "Accept-Language: en\r\n"
        "Accept-Charset: iso-8859-1, *, utf-8", host);
    request_headers_.AddHeadersFromString(headers_str);
  }

  const HttpResponseInfo* Response() const { return &response_; }

  bool UsingProxy() const { return using_proxy_; }

 private:
  scoped_refptr<HttpStreamShellLoader> loader_;

  HttpResponseInfo response_;
  BoundNetLog net_log_;
  HttpNetworkSession::Params network_params_;
  ProxyInfo proxy_info_;
  base::Thread io_thread_;
  base::WaitableEvent event_;
  bool using_proxy_;
  // Request data
  HttpRequestInfo request_info_;
  HttpRequestHeaders request_headers_;
  std::string request_line_;

  int res_;
};

TEST_F(HttpStreamShellLoaderLiveTest, TestHttpConnection) {
  GenerateRequstInfo("GET", "http", "www.google.com", "/");
  // Open connection, exit if failed
  int res = OpenConnection();
  ASSERT_EQ(OK, res);
  res = ResolveProxy();
  EXPECT_EQ(OK, res);
  // Send request
  res = SendRequest();
  EXPECT_EQ(OK, res);
  // Read response headers
  res = ReadResponseHeaders();
  EXPECT_EQ(OK, res);

  // Read response body
  const int kLoadBufferSize = 16 * 1024;
  scoped_refptr<GrowableIOBuffer> buffer = new GrowableIOBuffer();
  buffer->SetCapacity(kLoadBufferSize);

  while ((res = ReadResponseBody(buffer, buffer->capacity())) > 0) {
    EXPECT_GE(buffer->capacity(), res);
  }
  EXPECT_EQ(res, OK);
  EXPECT_NE(Response()->headers.get(), static_cast<HttpResponseHeaders*>(NULL));
  if (Response()->headers.get())
    EXPECT_EQ(Response()->headers->GetStatusLine(), "HTTP/1.1 200 OK");

  // Close connection
  CloseConnection();
}

TEST_F(HttpStreamShellLoaderLiveTest, TestHttpsConnection) {
  GenerateRequstInfo("GET", "https", "www.google.com", "/");
  // Open connection, exit if failed
  int res = OpenConnection();
  ASSERT_EQ(OK, res);
  res = ResolveProxy();
  EXPECT_EQ(OK, res);
  // Send request
  res = SendRequest();
  EXPECT_EQ(OK, res);
  // Read response headers
  res = ReadResponseHeaders();
  EXPECT_EQ(OK, res);

  // Read response body
  const int kLoadBufferSize = 16 * 1024;
  scoped_refptr<GrowableIOBuffer> buffer = new GrowableIOBuffer();
  buffer->SetCapacity(kLoadBufferSize);

  while ((res = ReadResponseBody(buffer, buffer->capacity())) > 0) {
    EXPECT_GE(buffer->capacity(), res);
  }
  EXPECT_NE(Response()->headers.get(), static_cast<HttpResponseHeaders*>(NULL));
  if (Response()->headers.get())
    EXPECT_EQ(Response()->headers->GetStatusLine(), "HTTP/1.1 200 OK");

  // Close connection
  CloseConnection();
}

TEST_F(HttpStreamShellLoaderLiveTest, TestWrongURLConnection) {
  GenerateRequstInfo("GET", "http", "SERVER.NOT.FOUND", "/");
  // Open connection, exit if failed
  int res = OpenConnection();
  ASSERT_EQ(OK, res);
  res = ResolveProxy();
  EXPECT_EQ(OK, res);
  // Send request
  res = SendRequest();
  if (res == OK) {
    // Error may happen in SendRequest or ReadResponseHeaders. So if
    // SendRequest returns OK, continue to read response headers.
    res = ReadResponseHeaders();
  }
  // Errors are different with or without proxy.
  if (UsingProxy()) {
    // With a proxy, 503 error is returned by proxy server
    EXPECT_EQ(ERR_SERVICE_UNAVAILABLE, res);
  } else {
    // Without a proxy, url can not be resolved.
    EXPECT_EQ(ERR_NAME_NOT_RESOLVED, res);
  }

  // Close connection
  CloseConnection();
}

}  // namespace net
#endif
