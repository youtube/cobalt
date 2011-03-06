// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_body_drainer.h"

#include <cstring>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_network_session.h"
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
        ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {}
  virtual ~MockHttpStream() {}

  // HttpStream implementation:
  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               const BoundNetLog& net_log,
                               CompletionCallback* callback) {
    return ERR_UNEXPECTED;
  }
  virtual int SendRequest(const HttpRequestHeaders& request_headers,
                          UploadDataStream* request_body,
                          HttpResponseInfo* response,
                          CompletionCallback* callback) {
    return ERR_UNEXPECTED;
  }
  virtual uint64 GetUploadProgress() const  { return 0; }
  virtual int ReadResponseHeaders(CompletionCallback* callback) {
    return ERR_UNEXPECTED;
  }
  virtual const HttpResponseInfo* GetResponseInfo() const { return NULL; }

  virtual bool CanFindEndOfResponse() const { return true; }
  virtual bool IsMoreDataBuffered() const { return false; }
  virtual bool IsConnectionReused() const { return false; }
  virtual void SetConnectionReused() {}
  virtual void GetSSLInfo(SSLInfo* ssl_info) {}
  virtual void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) {}

  // Mocked API
  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               CompletionCallback* callback);
  virtual void Close(bool not_reusable) {
    DCHECK(!closed_);
    closed_ = true;
    result_waiter_->set_result(not_reusable);
  }

  virtual HttpStream* RenewStreamForAuth() {
    return NULL;
  }

  virtual bool IsResponseBodyComplete() const { return is_complete_; }

  virtual bool IsSpdyHttpStream() const { return false; }

  // Methods to tweak/observer mock behavior:
  void StallReadsForever() { stall_reads_forever_ = true; }

  void set_num_chunks(int num_chunks) { num_chunks_ = num_chunks; }

 private:
  void CompleteRead();

  bool closed() const { return closed_; }

  CloseResultWaiter* const result_waiter_;
  scoped_refptr<IOBuffer> user_buf_;
  CompletionCallback* user_callback_;
  bool closed_;
  bool stall_reads_forever_;
  int num_chunks_;
  bool is_complete_;
  ScopedRunnableMethodFactory<MockHttpStream> method_factory_;
};

int MockHttpStream::ReadResponseBody(
    IOBuffer* buf, int buf_len, CompletionCallback* callback) {
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
        method_factory_.NewRunnableMethod(&MockHttpStream::CompleteRead));
    return ERR_IO_PENDING;
  }

  num_chunks_--;
  if (!num_chunks_)
    is_complete_ = true;

  return buf_len;
}

void MockHttpStream::CompleteRead() {
  CompletionCallback* callback = user_callback_;
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
        session_(CreateNetworkSession()),
        mock_stream_(new MockHttpStream(&result_waiter_)),
        drainer_(new HttpResponseBodyDrainer(mock_stream_)) {}

  ~HttpResponseBodyDrainerTest() {}

  HttpNetworkSession* CreateNetworkSession() const {
    HttpNetworkSession::Params params;
    params.proxy_service = proxy_service_;
    params.ssl_config_service = ssl_config_service_;
    return new HttpNetworkSession(params);
  }

  scoped_refptr<ProxyService> proxy_service_;
  scoped_refptr<SSLConfigService> ssl_config_service_;
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
  TestCompletionCallback callback;
  int too_many_chunks =
      HttpResponseBodyDrainer::kDrainBodyBufferSize / kMagicChunkSize;
  too_many_chunks += 1;  // Now it's too large.

  mock_stream_->set_num_chunks(too_many_chunks);
  drainer_->Start(session_);
  EXPECT_TRUE(result_waiter_.WaitForResult());
}

}  // namespace

}  // namespace net
