// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job.h"

#include "base/basictypes.h"
#include "net/http/http_transaction_unittest.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This is a header that signals the end of the data.
const char kGzipGata[] = "\x1f\x08b\x08\0\0\0\0\0\0\3\3\0\0\0\0\0\0\0\0";

void GZipServer(const net::HttpRequestInfo* request,
                std::string* response_status, std::string* response_headers,
                std::string* response_data) {
  response_data->assign(kGzipGata, sizeof(kGzipGata));
}

void NullServer(const net::HttpRequestInfo* request,
                std::string* response_status,
                std::string* response_headers,
                std::string* response_data) {
  FAIL();
}

const MockTransaction kGZip_Transaction = {
  net::OK,
  "http://www.google.com/gzyp",
  "GET",
  base::Time(),
  "",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "Cache-Control: max-age=10000\n"
  "Content-Encoding: gzip\n"
  "Content-Length: 30\n",  // Intentionally wrong.
  base::Time(),
  "",
  TEST_MODE_NORMAL,
  &GZipServer,
  0
};

}  // namespace

TEST(URLRequestJob, TransactionNotifiedWhenDone) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  TestURLRequest req(GURL(kGZip_Transaction.url), &d, &context);
  AddMockTransaction(&kGZip_Transaction);

  req.set_method("GET");
  req.Start();

  MessageLoop::current()->Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&kGZip_Transaction);
}

TEST(URLRequestJob, SyncTransactionNotifiedWhenDone) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  TestURLRequest req(GURL(kGZip_Transaction.url), &d, &context);
  MockTransaction transaction(kGZip_Transaction);
  transaction.test_mode = TEST_MODE_SYNC_ALL;
  AddMockTransaction(&transaction);

  req.set_method("GET");
  req.Start();

  MessageLoop::current()->Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&transaction);
}

// Tests that automatic retry triggers for certain errors on GETs, and is not
// triggered for other errors or with other methods.
TEST(URLRequestJob, RetryRequests) {
  const struct {
    net::Error transaction_result;
    const char* method;
    int expected_transaction_count;
  } tests[] = {
    {net::ERR_EMPTY_RESPONSE, "GET", 2},
    {net::ERR_CONNECTION_REFUSED, "GET", 2},
    {net::ERR_CONNECTION_REFUSED, "POST", 1},
    {net::ERR_CONNECTION_REFUSED, "PUT", 1},
    {net::ERR_ACCESS_DENIED, "GET", 1},
    {net::ERR_CONTENT_DECODING_FAILED, "GET", 1},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    const MockTransaction dead_transaction = {
      tests[i].transaction_result,
      "http://www.dead.com/",
      tests[i].method,
      base::Time(),
      "",
      net::LOAD_NORMAL,
      "",
      "",
      base::Time(),
      "",
      TEST_MODE_NORMAL,
      &NullServer,
      0
    };

    MockNetworkLayer network_layer;
    TestURLRequestContext context;
    context.set_http_transaction_factory(&network_layer);

    TestDelegate d;
    TestURLRequest req(GURL(dead_transaction.url), &d, &context);
    MockTransaction transaction(dead_transaction);
    transaction.test_mode = TEST_MODE_SYNC_ALL;
    AddMockTransaction(&transaction);

    req.set_method(tests[i].method);
    req.Start();

    MessageLoop::current()->Run();

    EXPECT_EQ(tests[i].expected_transaction_count,
              network_layer.transaction_count());

    RemoveMockTransaction(&transaction);
  }
}
