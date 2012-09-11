// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_transaction_unittest.h"

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/base/load_flags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
typedef base::hash_map<std::string, const MockTransaction*> MockTransactionMap;
static MockTransactionMap mock_transactions;
}  // namespace

//-----------------------------------------------------------------------------
// mock transaction data

const MockTransaction kSimpleGET_Transaction = {
  "http://www.google.com/",
  "GET",
  base::Time(),
  "",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "Cache-Control: max-age=10000\n",
  base::Time(),
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_NORMAL,
  NULL,
  0
};

const MockTransaction kSimplePOST_Transaction = {
  "http://bugdatabase.com/edit",
  "POST",
  base::Time(),
  "",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "",
  base::Time(),
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_NORMAL,
  NULL,
  0
};

const MockTransaction kTypicalGET_Transaction = {
  "http://www.example.com/~foo/bar.html",
  "GET",
  base::Time(),
  "",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
  "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n",
  base::Time(),
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_NORMAL,
  NULL,
  0
};

const MockTransaction kETagGET_Transaction = {
  "http://www.google.com/foopy",
  "GET",
  base::Time(),
  "",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "Cache-Control: max-age=10000\n"
  "Etag: \"foopy\"\n",
  base::Time(),
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_NORMAL,
  NULL,
  0
};

const MockTransaction kRangeGET_Transaction = {
  "http://www.google.com/",
  "GET",
  base::Time(),
  "Range: 0-100\r\n",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "Cache-Control: max-age=10000\n",
  base::Time(),
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_NORMAL,
  NULL,
  0
};

static const MockTransaction* const kBuiltinMockTransactions[] = {
  &kSimpleGET_Transaction,
  &kSimplePOST_Transaction,
  &kTypicalGET_Transaction,
  &kETagGET_Transaction,
  &kRangeGET_Transaction
};

const MockTransaction* FindMockTransaction(const GURL& url) {
  // look for overrides:
  MockTransactionMap::const_iterator it = mock_transactions.find(url.spec());
  if (it != mock_transactions.end())
    return it->second;

  // look for builtins:
  for (size_t i = 0; i < arraysize(kBuiltinMockTransactions); ++i) {
    if (url == GURL(kBuiltinMockTransactions[i]->url))
      return kBuiltinMockTransactions[i];
  }
  return NULL;
}

void AddMockTransaction(const MockTransaction* trans) {
  mock_transactions[GURL(trans->url).spec()] = trans;
}

void RemoveMockTransaction(const MockTransaction* trans) {
  mock_transactions.erase(GURL(trans->url).spec());
}

MockHttpRequest::MockHttpRequest(const MockTransaction& t) {
  url = GURL(t.url);
  method = t.method;
  extra_headers.AddHeadersFromString(t.request_headers);
  load_flags = t.load_flags;
}

//-----------------------------------------------------------------------------

// static
int TestTransactionConsumer::quit_counter_ = 0;

TestTransactionConsumer::TestTransactionConsumer(
    net::HttpTransactionFactory* factory)
    : state_(IDLE),
      trans_(NULL),
      error_(net::OK) {
  // Disregard the error code.
  factory->CreateTransaction(&trans_, NULL);
  ++quit_counter_;
}

TestTransactionConsumer::~TestTransactionConsumer() {
}

void TestTransactionConsumer::Start(const net::HttpRequestInfo* request,
                                    const net::BoundNetLog& net_log) {
  state_ = STARTING;
  int result = trans_->Start(
      request, base::Bind(&TestTransactionConsumer::OnIOComplete,
                          base::Unretained(this)), net_log);
  if (result != net::ERR_IO_PENDING)
    DidStart(result);
}

void TestTransactionConsumer::DidStart(int result) {
  if (result != net::OK) {
    DidFinish(result);
  } else {
    Read();
  }
}

void TestTransactionConsumer::DidRead(int result) {
  if (result <= 0) {
    DidFinish(result);
  } else {
    content_.append(read_buf_->data(), result);
    Read();
  }
}

void TestTransactionConsumer::DidFinish(int result) {
  state_ = DONE;
  error_ = result;
  if (--quit_counter_ == 0)
    MessageLoop::current()->Quit();
}

void TestTransactionConsumer::Read() {
  state_ = READING;
  read_buf_ = new net::IOBuffer(1024);
  int result = trans_->Read(read_buf_, 1024,
                            base::Bind(&TestTransactionConsumer::OnIOComplete,
                                       base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    DidRead(result);
}

void TestTransactionConsumer::OnIOComplete(int result) {
  switch (state_) {
    case STARTING:
      DidStart(result);
      break;
    case READING:
      DidRead(result);
      break;
    default:
      NOTREACHED();
  }
}

MockNetworkTransaction::MockNetworkTransaction(MockNetworkLayer* factory)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      data_cursor_(0),
      transaction_factory_(factory->AsWeakPtr()) {
}

MockNetworkTransaction::~MockNetworkTransaction() {}

int MockNetworkTransaction::Start(const net::HttpRequestInfo* request,
                                  const net::CompletionCallback& callback,
                                  const net::BoundNetLog& net_log) {
  const MockTransaction* t = FindMockTransaction(request->url);
  if (!t)
    return net::ERR_FAILED;

  std::string resp_status = t->status;
  std::string resp_headers = t->response_headers;
  std::string resp_data = t->data;
  if (t->handler)
    (t->handler)(request, &resp_status, &resp_headers, &resp_data);

  std::string header_data = base::StringPrintf(
      "%s\n%s\n", resp_status.c_str(), resp_headers.c_str());
  std::replace(header_data.begin(), header_data.end(), '\n', '\0');

  response_.request_time = base::Time::Now();
  if (!t->request_time.is_null())
    response_.request_time = t->request_time;

  response_.was_cached = false;

  response_.response_time = base::Time::Now();
  if (!t->response_time.is_null())
    response_.response_time = t->response_time;

  response_.headers = new net::HttpResponseHeaders(header_data);
  response_.ssl_info.cert_status = t->cert_status;
  data_ = resp_data;
  test_mode_ = t->test_mode;

  if (test_mode_ & TEST_MODE_SYNC_NET_START)
    return net::OK;

  CallbackLater(callback, net::OK);
  return net::ERR_IO_PENDING;
}

int MockNetworkTransaction::RestartIgnoringLastError(
    const net::CompletionCallback& callback) {
  return net::ERR_FAILED;
}

int MockNetworkTransaction::RestartWithCertificate(
    net::X509Certificate* client_cert,
    const net::CompletionCallback& callback) {
  return net::ERR_FAILED;
}

int MockNetworkTransaction::RestartWithAuth(
    const net::AuthCredentials& credentials,
    const net::CompletionCallback& callback) {
  return net::ERR_FAILED;
}

bool MockNetworkTransaction::IsReadyToRestartForAuth() {
  return false;
}

int MockNetworkTransaction::Read(net::IOBuffer* buf, int buf_len,
                                 const net::CompletionCallback& callback) {
  int data_len = static_cast<int>(data_.size());
  int num = std::min(buf_len, data_len - data_cursor_);
  if (num) {
    memcpy(buf->data(), data_.data() + data_cursor_, num);
    data_cursor_ += num;
  }
  if (test_mode_ & TEST_MODE_SYNC_NET_READ)
    return num;

  CallbackLater(callback, num);
  return net::ERR_IO_PENDING;
}

void MockNetworkTransaction::StopCaching() {}

void MockNetworkTransaction::DoneReading() {
  if (transaction_factory_)
    transaction_factory_->TransactionDoneReading();
}

const net::HttpResponseInfo* MockNetworkTransaction::GetResponseInfo() const {
  return &response_;
}

net::LoadState MockNetworkTransaction::GetLoadState() const {
  if (data_cursor_)
    return net::LOAD_STATE_READING_RESPONSE;
  return net::LOAD_STATE_IDLE;
}

net::UploadProgress MockNetworkTransaction::GetUploadProgress() const {
  return net::UploadProgress();
}

void MockNetworkTransaction::CallbackLater(
    const net::CompletionCallback& callback, int result) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&MockNetworkTransaction::RunCallback,
                            weak_factory_.GetWeakPtr(), callback, result));
}

void MockNetworkTransaction::RunCallback(
    const net::CompletionCallback& callback, int result) {
  callback.Run(result);
}

MockNetworkLayer::MockNetworkLayer()
    : transaction_count_(0), done_reading_called_(false) {}

MockNetworkLayer::~MockNetworkLayer() {}

void MockNetworkLayer::TransactionDoneReading() {
  done_reading_called_ = true;
}

int MockNetworkLayer::CreateTransaction(
    scoped_ptr<net::HttpTransaction>* trans,
    net::HttpTransactionDelegate* delegate) {
  transaction_count_++;
  trans->reset(new MockNetworkTransaction(this));
  return net::OK;
}

net::HttpCache* MockNetworkLayer::GetCache() {
  return NULL;
}

net::HttpNetworkSession* MockNetworkLayer::GetSession() {
  return NULL;
}

//-----------------------------------------------------------------------------
// helpers

int ReadTransaction(net::HttpTransaction* trans, std::string* result) {
  int rv;

  net::TestCompletionCallback callback;

  std::string content;
  do {
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(256));
    rv = trans->Read(buf, 256, callback.callback());
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    if (rv > 0)
      content.append(buf->data(), rv);
    else if (rv < 0)
      return rv;
  } while (rv > 0);

  result->swap(content);
  return net::OK;
}
