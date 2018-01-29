// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_UNITTEST_H_
#define NET_HTTP_HTTP_TRANSACTION_UNITTEST_H_

#include "net/http/http_transaction.h"

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"

namespace net {
class IOBuffer;
}

//-----------------------------------------------------------------------------
// mock transaction data

// these flags may be combined to form the test_mode field
enum {
  TEST_MODE_NORMAL = 0,
  TEST_MODE_SYNC_NET_START = 1 << 0,
  TEST_MODE_SYNC_NET_READ  = 1 << 1,
  TEST_MODE_SYNC_CACHE_START = 1 << 2,
  TEST_MODE_SYNC_CACHE_READ  = 1 << 3,
  TEST_MODE_SYNC_CACHE_WRITE  = 1 << 4,
  TEST_MODE_SYNC_ALL = (TEST_MODE_SYNC_NET_START | TEST_MODE_SYNC_NET_READ |
                        TEST_MODE_SYNC_CACHE_START | TEST_MODE_SYNC_CACHE_READ |
                        TEST_MODE_SYNC_CACHE_WRITE)
};

typedef void (*MockTransactionHandler)(const net::HttpRequestInfo* request,
                                       std::string* response_status,
                                       std::string* response_headers,
                                       std::string* response_data);

struct MockTransaction {
  const char* url;
  const char* method;
  // If |request_time| is unspecified, the current time will be used.
  base::Time request_time;
  const char* request_headers;
  int load_flags;
  const char* status;
  const char* response_headers;
  // If |response_time| is unspecified, the current time will be used.
  base::Time response_time;
  const char* data;
  int test_mode;
  MockTransactionHandler handler;
  net::CertStatus cert_status;
};

extern const MockTransaction kSimpleGET_Transaction;
extern const MockTransaction kSimplePOST_Transaction;
extern const MockTransaction kTypicalGET_Transaction;
extern const MockTransaction kETagGET_Transaction;
extern const MockTransaction kRangeGET_Transaction;

// returns the mock transaction for the given URL
const MockTransaction* FindMockTransaction(const GURL& url);

// Add/Remove a mock transaction that can be accessed via FindMockTransaction.
// There can be only one MockTransaction associated with a given URL.
void AddMockTransaction(const MockTransaction* trans);
void RemoveMockTransaction(const MockTransaction* trans);

struct ScopedMockTransaction : MockTransaction {
  ScopedMockTransaction() {
    AddMockTransaction(this);
  }
  explicit ScopedMockTransaction(const MockTransaction& t)
      : MockTransaction(t) {
    AddMockTransaction(this);
  }
  ~ScopedMockTransaction() {
    RemoveMockTransaction(this);
  }
};

//-----------------------------------------------------------------------------
// mock http request

class MockHttpRequest : public net::HttpRequestInfo {
 public:
  explicit MockHttpRequest(const MockTransaction& t);
};

//-----------------------------------------------------------------------------
// use this class to test completely consuming a transaction

class TestTransactionConsumer {
 public:
  explicit TestTransactionConsumer(net::HttpTransactionFactory* factory);
  virtual ~TestTransactionConsumer();

  void Start(const net::HttpRequestInfo* request,
             const net::BoundNetLog& net_log);

  bool is_done() const { return state_ == DONE; }
  int error() const { return error_; }

  const net::HttpResponseInfo* response_info() const {
    return trans_->GetResponseInfo();
  }
  const std::string& content() const { return content_; }

 private:
  enum State {
    IDLE,
    STARTING,
    READING,
    DONE
  };

  void DidStart(int result);
  void DidRead(int result);
  void DidFinish(int result);
  void Read();

  void OnIOComplete(int result);

  State state_;
  scoped_ptr<net::HttpTransaction> trans_;
  std::string content_;
  scoped_refptr<net::IOBuffer> read_buf_;
  int error_;

  static int quit_counter_;
};

//-----------------------------------------------------------------------------
// mock network layer

class MockNetworkLayer;

// This transaction class inspects the available set of mock transactions to
// find data for the request URL.  It supports IO operations that complete
// synchronously or asynchronously to help exercise different code paths in the
// HttpCache implementation.
class MockNetworkTransaction : public net::HttpTransaction {
 public:
  explicit MockNetworkTransaction(MockNetworkLayer* factory);
  virtual ~MockNetworkTransaction();

  virtual int Start(const net::HttpRequestInfo* request,
                    const net::CompletionCallback& callback,
                    const net::BoundNetLog& net_log) override;

  virtual int RestartIgnoringLastError(
      const net::CompletionCallback& callback) override;

  virtual int RestartWithCertificate(
      net::X509Certificate* client_cert,
      const net::CompletionCallback& callback) override;

  virtual int RestartWithAuth(
      const net::AuthCredentials& credentials,
      const net::CompletionCallback& callback) override;

  virtual bool IsReadyToRestartForAuth() override;

  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) override;

  virtual void StopCaching() override;

  virtual void DoneReading() override;

  virtual const net::HttpResponseInfo* GetResponseInfo() const override;

  virtual net::LoadState GetLoadState() const override;

  virtual net::UploadProgress GetUploadProgress() const override;

 private:
  void CallbackLater(const net::CompletionCallback& callback, int result);
  void RunCallback(const net::CompletionCallback& callback, int result);

  base::WeakPtrFactory<MockNetworkTransaction> weak_factory_;
  net::HttpResponseInfo response_;
  std::string data_;
  int data_cursor_;
  int test_mode_;
  base::WeakPtr<MockNetworkLayer> transaction_factory_;
};

class MockNetworkLayer : public net::HttpTransactionFactory,
                         public base::SupportsWeakPtr<MockNetworkLayer> {
 public:
  MockNetworkLayer();
  virtual ~MockNetworkLayer();

  int transaction_count() const { return transaction_count_; }
  bool done_reading_called() const { return done_reading_called_; }
  void TransactionDoneReading();

  // net::HttpTransactionFactory:
  virtual int CreateTransaction(
      scoped_ptr<net::HttpTransaction>* trans,
      net::HttpTransactionDelegate* delegate) override;
  virtual net::HttpCache* GetCache() override;
  virtual net::HttpNetworkSession* GetSession() override;

 private:
  int transaction_count_;
  bool done_reading_called_;
};

//-----------------------------------------------------------------------------
// helpers

// read the transaction completely
int ReadTransaction(net::HttpTransaction* trans, std::string* result);

#endif  // NET_HTTP_HTTP_TRANSACTION_UNITTEST_H_
