// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_body_drainer.h"

#include <cstring>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMagicChunkSize = 1024;
COMPILE_ASSERT(
    (HttpResponseBodyDrainer::kDrainBodyBufferSize % kMagicChunkSize) == 0,
    chunk_size_needs_to_divide_evenly_into_buffer_size);

class CloseResultWaiter {
 public:
  CloseResultWaiter()
      : result_(false),
        have_result_(false),
        waiting_for_result_(false) {}

  int WaitForResult() {
    DCHECK(!waiting_for_result_);
    while (!have_result_) {
      waiting_for_result_ = true;
      MessageLoop::current()->Run();
      waiting_for_result_ = false;
    }
    return result_;
  }

  void set_result(bool result) {
    result_ = result;
    have_result_ = true;
    if (waiting_for_result_)
      MessageLoop::current()->Quit();
  }

 private:
  int result_;
  bool have_result_;
  bool waiting_for_result_;

  DISALLOW_COPY_AND_ASSIGN(CloseResultWaiter);
};

class MockHttpStream : public HttpStream {
 public:
  MockHttpStream(CloseResultWaiter* result_waiter)
      : result_waiter_(result_waiter),
        user_callback_(NULL),
        closed_(false),
        stall_reads_forever_(false),
        num_chunks_(0),
        is_complete_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(ptr_factory_(this)) {}
  virtual ~MockHttpStream() {}

  // HttpStream implementation:
  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               const BoundNetLog& net_log,
                               OldCompletionCallback* callback) OVERRIDE {
    return ERR_UNEXPECTED;
  }
  virtual int SendRequest(const HttpRequestHeaders& request_headers,
                          UploadDataStream* request_body,
                          HttpResponseInfo* response,
                          OldCompletionCallback* callback) OVERRIDE {
    return ERR_UNEXPECTED;
  }
  virtual uint64 GetUploadProgress() const OVERRIDE { return 0; }
  virtual int ReadResponseHeaders(OldCompletionCallback* callback) OVERRIDE {
    return ERR_UNEXPECTED;
  }
  virtual const HttpResponseInfo* GetResponseInfo() const OVERRIDE {
    return NULL;
  }

  virtual bool CanFindEndOfResponse() const OVERRIDE { return true; }
  virtual bool IsMoreDataBuffered() const OVERRIDE { return false; }
  virtual bool IsConnectionReused() const OVERRIDE { return false; }
  virtual void SetConnectionReused() OVERRIDE {}
  virtual bool IsConnectionReusable() const OVERRIDE { return false; }
  virtual void GetSSLInfo(SSLInfo* ssl_info) OVERRIDE {}
  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) OVERRIDE {}

  // Mocked API
  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               OldCompletionCallback* callback) OVERRIDE;
  virtual void Close(bool not_reusable) OVERRIDE {
    DCHECK(!closed_);
    closed_ = true;
    result_waiter_->set_result(not_reusable);
  }

  virtual HttpStream* RenewStreamForAuth() OVERRIDE {
    return NULL;
  }

  virtual bool IsResponseBodyComplete() const OVERRIDE { return is_complete_; }

  virtual bool IsSpdyHttpStream() const OVERRIDE { return false; }

  virtual void LogNumRttVsBytesMetrics() const OVERRIDE {}

  virtual void Drain(HttpNetworkSession*) OVERRIDE {}

  // Methods to tweak/observer mock behavior:
  void StallReadsForever() { stall_reads_forever_ = true; }

  void set_num_chunks(int num_chunks) { num_chunks_ = num_chunks; }

 private:
  void CompleteRead();

  bool closed() const { return closed_; }

  CloseResultWaiter* const result_waiter_;
  scoped_refptr<IOBuffer> user_buf_;
  OldCompletionCallback* user_callback_;
  bool closed_;
  bool stall_reads_forever_;
  int num_chunks_;
  bool is_complete_;
  base::WeakPtrFactory<MockHttpStream> ptr_factory_;
};

int MockHttpStream::ReadResponseBody(
    IOBuffer* buf, int buf_len, OldCompletionCallback* callback) {
  DCHECK(callback);
  DCHECK(!user_callback_);
  DCHECK(buf);

  if (stall_reads_forever_)
    return ERR_IO_PENDING;

  if (num_chunks_ == 0)
    return ERR_UNEXPECTED;

  if (buf_len > kMagicChunkSize && num_chunks_ > 1) {
    user_buf_ = buf;
    user_callback_ = callback;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MockHttpStream::CompleteRead, ptr_factory_.GetWeakPtr()));
    return ERR_IO_PENDING;
  }

  num_chunks_--;
  if (!num_chunks_)
    is_complete_ = true;

  return buf_len;
}

void MockHttpStream::CompleteRead() {
  OldCompletionCallback* callback = user_callback_;
  std::memset(user_buf_->data(), 1, kMagicChunkSize);
  user_buf_ = NULL;
  user_callback_ = NULL;
  num_chunks_--;
  if (!num_chunks_)
    is_complete_ = true;
  callback->Run(kMagicChunkSize);
}

class HttpResponseBodyDrainerTest : public testing::Test {
 protected:
  HttpResponseBodyDrainerTest()
      : proxy_service_(ProxyService::CreateDirect()),
        ssl_config_service_(new SSLConfigServiceDefaults),
        http_server_properties_(new HttpServerPropertiesImpl),
        session_(CreateNetworkSession()),
        mock_stream_(new MockHttpStream(&result_waiter_)),
        drainer_(new HttpResponseBodyDrainer(mock_stream_)) {}

  ~HttpResponseBodyDrainerTest() {}

  HttpNetworkSession* CreateNetworkSession() const {
    HttpNetworkSession::Params params;
    params.proxy_service = proxy_service_.get();
    params.ssl_config_service = ssl_config_service_;
    params.http_server_properties = http_server_properties_.get();
    return new HttpNetworkSession(params);
  }

  scoped_ptr<ProxyService> proxy_service_;
  scoped_refptr<SSLConfigService> ssl_config_service_;
  scoped_ptr<HttpServerPropertiesImpl> http_server_properties_;
  const scoped_refptr<HttpNetworkSession> session_;
  CloseResultWaiter result_waiter_;
  MockHttpStream* const mock_stream_;  // Owned by |drainer_|.
  HttpResponseBodyDrainer* const drainer_;  // Deletes itself.
};

TEST_F(HttpResponseBodyDrainerTest, DrainBodySyncOK) {
  mock_stream_->set_num_chunks(1);
  drainer_->Start(session_);
  EXPECT_FALSE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, DrainBodyAsyncOK) {
  mock_stream_->set_num_chunks(3);
  drainer_->Start(session_);
  EXPECT_FALSE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, DrainBodySizeEqualsDrainBuffer) {
  mock_stream_->set_num_chunks(
      HttpResponseBodyDrainer::kDrainBodyBufferSize / kMagicChunkSize);
  drainer_->Start(session_);
  EXPECT_FALSE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, DrainBodyTimeOut) {
  mock_stream_->set_num_chunks(2);
  mock_stream_->StallReadsForever();
  drainer_->Start(session_);
  EXPECT_TRUE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, CancelledBySession) {
  mock_stream_->set_num_chunks(2);
  mock_stream_->StallReadsForever();
  drainer_->Start(session_);
  // HttpNetworkSession should delete |drainer_|.
}

TEST_F(HttpResponseBodyDrainerTest, DrainBodyTooLarge) {
  TestOldCompletionCallback callback;
  int too_many_chunks =
      HttpResponseBodyDrainer::kDrainBodyBufferSize / kMagicChunkSize;
  too_many_chunks += 1;  // Now it's too large.

  mock_stream_->set_num_chunks(too_many_chunks);
  drainer_->Start(session_);
  EXPECT_TRUE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, StartBodyTooLarge) {
  TestOldCompletionCallback callback;
  int too_many_chunks =
      HttpResponseBodyDrainer::kDrainBodyBufferSize / kMagicChunkSize;
  too_many_chunks += 1;  // Now it's too large.

  mock_stream_->set_num_chunks(0);
  drainer_->StartWithSize(session_, too_many_chunks * kMagicChunkSize);
  EXPECT_TRUE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, StartWithNothingToDo) {
  TestOldCompletionCallback callback;
  mock_stream_->set_num_chunks(0);
  drainer_->StartWithSize(session_, 0);
  EXPECT_FALSE(result_waiter_.WaitForResult());
}

}  // namespace

}  // namespace net
