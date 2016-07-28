// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#if __LB_ENABLE_NATIVE_HTTP_STACK__

#include "net/http/shell/http_transaction_shell_unittest.h"

#include "base/json/json_writer.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_transaction_unittest.h"

//-----------------------------------------------------------------------------

// Note: GetHeaders() and verification steps in SimpleHelper() are from
// this file:
// net/http/http_network_transaction_spdy2_unittest.cc

namespace {

// Takes in a Value created from a NetLogHttpResponseParameter, and returns
// a JSONified list of headers as a single string.  Uses single quotes instead
// of double quotes for easier comparison.  Returns false on failure.
bool GetHeaders(DictionaryValue* params, std::string* headers) {
  if (!params)
    return false;
  ListValue* header_list;
  if (!params->GetList("headers", &header_list))
    return false;
  std::string double_quote_headers;
  base::JSONWriter::Write(header_list, &double_quote_headers);
  ReplaceChars(double_quote_headers, "\"", "'", headers);
  return true;
}

}  // namespace

namespace net {

// Test class
class HttpTransactionShellTest : public PlatformTest {
 protected:
  struct SimpleHelperResult {
    int rv;
    std::string status_line;
    std::string response_data;
  };

  virtual void SetUp() {
    MessageLoop::current()->RunUntilIdle();
   // Create factory
    transaction_factory_.reset(
        new MockTransactionFactoryShell(network_params_));
  }

  virtual void TearDown() {
    MessageLoop::current()->RunUntilIdle();
    PlatformTest::TearDown();
  }

  // Helper function to create transaction and stream loader
  MockTransactionShell* CreateTransaction() {
    scoped_ptr<HttpTransaction> trans;
    transaction_factory_->CreateTransaction(&trans, NULL);
    return static_cast<MockTransactionShell*>(trans.release());
  }

  MockStreamShellLoader* GetStreamLoader(HttpTransaction* trans) {
    MockTransactionShell* mock_trans = static_cast<MockTransactionShell*>(trans);
    return mock_trans->GetStreamLoader();
  }

  // Helper functions to access HttpTransactionShell's member variables
  void SetTransactionReadBuffer(HttpTransactionShell* trans, IOBuffer* buffer,
                                int size) {
    trans->read_buf_ = buffer;
    trans->read_buf_len_ = size;
  }

  void SetTransactionHeader(HttpTransactionShell* trans, const char* tag,
                            const char* value) {
    trans->request_headers_.SetHeader(tag, value);
  }

  IOBuffer* GetTransactionReadBuffer(HttpTransactionShell* trans) {
    return trans->read_buf_;
  }

  int GetTransactionReadBufferLength(HttpTransactionShell* trans) {
    return trans->read_buf_len_;
  }

  HttpRequestHeaders* GetTransactionHeaders(HttpTransactionShell* trans) {
    return &trans->request_headers_;
  }

  HttpResponseInfo* GetTransactionResponse(HttpTransactionShell* trans) {
    return &trans->response_;
  }

  SimpleHelperResult SimpleHelper(const char* method,
                                  const char* scheme,
                                  const char* host,
                                  const char* path,
                                  const char* headers,
                                  const char* body) {
    SimpleHelperResult out;

    HttpRequestInfo request;
    request.method = method;
    std::string url = StringPrintf("%s://%s%s", scheme, host, path);
    request.url = GURL(url);
    // Bypass proxy since there is no real network communication in this test
    request.load_flags = LOAD_BYPASS_PROXY;

    scoped_ptr<HttpTransaction> trans(CreateTransaction());
    MockStreamShellLoader* loader = GetStreamLoader(trans.get());
    EXPECT_NE(static_cast<MockStreamShellLoader*>(NULL), loader);
    loader->SetReturnData(headers, body);

    TestCompletionCallback callback;

    CapturingBoundNetLog log;
    EXPECT_TRUE(log.bound().IsLoggingAllEvents());
    int rv = trans->Start(&request, callback.callback(), log.bound());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    if (ERR_IO_PENDING != rv) {
      // Skip to avoid lock down
      out.rv = ERR_UNEXPECTED;
      return out;
    }
    // Wait for callback
    out.rv = callback.WaitForResult();
    if (out.rv != OK)
      return out;
    // Read response
    const HttpResponseInfo* response = trans->GetResponseInfo();
    if (response == NULL || response->headers == NULL) {
      out.rv = ERR_UNEXPECTED;
      return out;
    }
    out.status_line = response->headers->GetStatusLine();

    rv = ReadTransaction(trans.get(), &out.response_data);
    EXPECT_EQ(OK, rv);

    // Verify log
    net::CapturingNetLog::CapturedEntryList entries;
    log.GetEntries(&entries);
    size_t pos = ExpectLogContainsSomewhere(
        entries, 0, NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST_HEADERS,
        NetLog::PHASE_NONE);
    ExpectLogContainsSomewhere(
        entries, pos,
        NetLog::TYPE_HTTP_TRANSACTION_READ_RESPONSE_HEADERS,
        NetLog::PHASE_NONE);

    std::string line;
    EXPECT_TRUE(entries[pos].GetStringValue("line", &line));
    std::string expected_line =
        StringPrintf("%s %s HTTP/1.1\r\n", method, path);
    EXPECT_EQ(expected_line, line);

    std::string verify_headers;
    EXPECT_TRUE(GetHeaders(entries[pos].params.get(), &verify_headers));
    std::string expected_headers =
        StringPrintf("['Host: %s','Connection: keep-alive']",
                     host);
    EXPECT_EQ(expected_headers, verify_headers);

    return out;
  }

 private:
  // Dummy network parameter object
  net::HttpNetworkSession::Params network_params_;
  // Mock factory
  scoped_ptr<MockTransactionFactoryShell> transaction_factory_;
};

TEST_F(HttpTransactionShellTest, Basic) {
  scoped_ptr<HttpTransaction> trans(CreateTransaction());
}

TEST_F(HttpTransactionShellTest, SimpleGET) {
  const char* method = "GET";
  const char* scheme = "http";
  const char* host = "www.google.com";
  const char* path = "/";
  const char* header = "HTTP/1.1 200 OK";
  const char* body = "hello world";

  SimpleHelperResult out = SimpleHelper(method, scheme, host, path,
                                        header, body);

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ(header, out.status_line);
  EXPECT_EQ(body, out.response_data);
}


TEST_F(HttpTransactionShellTest, StatusLineJunk2Bytes) {
  const char* method = "GET";
  const char* scheme = "http";
  const char* host = "www.google.com";
  const char* path = "/";
  const char* headers = "HTTP/1.1 404 Not Found\nServer: blah\n\n";
  const char* body = "DATA";

  SimpleHelperResult out = SimpleHelper(method, scheme, host, path,
                                        headers, body);


  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 404 Not Found", out.status_line);
  EXPECT_EQ(body, out.response_data);
}

}  // namespace net
#endif
