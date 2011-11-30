// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_connection_impl.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_pipelined_stream.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::StrEq;

namespace net {

class DummySocketParams : public base::RefCounted<DummySocketParams> {
 private:
  friend class base::RefCounted<DummySocketParams>;
};

REGISTER_SOCKET_PARAMS_FOR_POOL(MockTransportClientSocketPool,
                                DummySocketParams);

class MockPipelineDelegate : public HttpPipelinedConnectionImpl::Delegate {
 public:
  MOCK_METHOD1(OnPipelineHasCapacity, void(HttpPipelinedConnection* pipeline));
};

class SuddenCloseObserver : public MessageLoop::TaskObserver {
 public:
  SuddenCloseObserver(HttpStream* stream, int close_before_task)
      : stream_(stream),
        close_before_task_(close_before_task),
        current_task_(0) { }

  virtual void WillProcessTask(base::TimeTicks) OVERRIDE {
    ++current_task_;
    if (current_task_ == close_before_task_) {
      stream_->Close(false);
      MessageLoop::current()->RemoveTaskObserver(this);
    }
  }

  virtual void DidProcessTask(base::TimeTicks) OVERRIDE { }

 private:
  HttpStream* stream_;
  int close_before_task_;
  int current_task_;
};

class HttpPipelinedConnectionImplTest : public testing::Test {
 public:
  HttpPipelinedConnectionImplTest()
      : histograms_("a"),
        pool_(1, 1, &histograms_, &factory_) {
  }

  void TearDown() {
    MessageLoop::current()->RunAllPending();
  }

  void Initialize(MockRead* reads, size_t reads_count,
                  MockWrite* writes, size_t writes_count) {
    data_ = new DeterministicSocketData(reads, reads_count,
                                        writes, writes_count);
    data_->set_connect_data(MockConnect(false, 0));
    if (reads_count || writes_count) {
      data_->StopAfter(reads_count + writes_count);
    }
    factory_.AddSocketDataProvider(data_.get());
    scoped_refptr<DummySocketParams> params;
    ClientSocketHandle* connection = new ClientSocketHandle;
    connection->Init("a", params, MEDIUM, NULL, &pool_, BoundNetLog());
    pipeline_.reset(new HttpPipelinedConnectionImpl(connection, &delegate_,
                                                    ssl_config_, proxy_info_,
                                                    BoundNetLog(), false));
  }

  HttpRequestInfo* GetRequestInfo(const std::string& filename) {
    HttpRequestInfo* request_info = new HttpRequestInfo;
    request_info->url = GURL("http://localhost/" + filename);
    request_info->method = "GET";
    request_info_vector_.push_back(request_info);
    return request_info;
  }

  HttpStream* NewTestStream(const std::string& filename) {
    HttpStream* stream = pipeline_->CreateNewStream();
    HttpRequestInfo* request_info = GetRequestInfo(filename);
    int rv = stream->InitializeStream(request_info, BoundNetLog(), NULL);
    DCHECK_EQ(OK, rv);
    return stream;
  }

  void ExpectResponse(const std::string& expected,
                      scoped_ptr<HttpStream>& stream, bool async) {
    scoped_refptr<IOBuffer> buffer(new IOBuffer(expected.size()));

    if (async) {
      EXPECT_EQ(ERR_IO_PENDING,
                stream->ReadResponseBody(buffer.get(), expected.size(),
                                         &callback_));
      data_->RunFor(1);
      EXPECT_EQ(static_cast<int>(expected.size()), callback_.WaitForResult());
    } else {
      EXPECT_EQ(static_cast<int>(expected.size()),
                stream->ReadResponseBody(buffer.get(), expected.size(),
                                         &callback_));
    }
    std::string actual(buffer->data(), expected.size());
    EXPECT_THAT(actual, StrEq(expected));
  }

  void TestSyncRequest(scoped_ptr<HttpStream>& stream,
                       const std::string& filename) {
    HttpRequestHeaders headers;
    HttpResponseInfo response;
    EXPECT_EQ(OK, stream->SendRequest(headers, NULL, &response, &callback_));
    EXPECT_EQ(OK, stream->ReadResponseHeaders(&callback_));
    ExpectResponse(filename, stream, false);

    stream->Close(false);
  }

  DeterministicMockClientSocketFactory factory_;
  ClientSocketPoolHistograms histograms_;
  MockTransportClientSocketPool pool_;
  scoped_refptr<DeterministicSocketData> data_;

  SSLConfig ssl_config_;
  ProxyInfo proxy_info_;
  NiceMock<MockPipelineDelegate> delegate_;
  TestOldCompletionCallback callback_;
  scoped_ptr<HttpPipelinedConnectionImpl> pipeline_;
  ScopedVector<HttpRequestInfo> request_info_vector_;
};

TEST_F(HttpPipelinedConnectionImplTest, PipelineNotUsed) {
  Initialize(NULL, 0, NULL, 0);
}

TEST_F(HttpPipelinedConnectionImplTest, StreamNotUsed) {
  Initialize(NULL, 0, NULL, 0);

  scoped_ptr<HttpStream> stream(pipeline_->CreateNewStream());

  stream->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, StreamBoundButNotUsed) {
  Initialize(NULL, 0, NULL, 0);

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));

  stream->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, SyncSingleRequest) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 3, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));
  TestSyncRequest(stream, "ok.html");
}

TEST_F(HttpPipelinedConnectionImplTest, AsyncSingleRequest) {
  MockWrite writes[] = {
    MockWrite(true, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(true, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(true, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(true, 3, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequest(headers, NULL, &response, &callback_));
  data_->RunFor(1);
  EXPECT_LE(OK, callback_.WaitForResult());

  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(&callback_));
  data_->RunFor(2);
  EXPECT_LE(OK, callback_.WaitForResult());

  ExpectResponse("ok.html", stream, true);

  stream->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, LockStepAsyncRequests) {
  MockWrite writes[] = {
    MockWrite(true, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(true, 1, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(true, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(true, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(true, 4, "ok.html"),
    MockRead(true, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(true, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(true, 7, "ko.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(ERR_IO_PENDING,
            stream1->SendRequest(headers1, NULL, &response1, &callback_));

  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(ERR_IO_PENDING,
            stream2->SendRequest(headers2, NULL, &response2, &callback_));

  data_->RunFor(1);
  EXPECT_LE(OK, callback_.WaitForResult());
  data_->RunFor(1);
  EXPECT_LE(OK, callback_.WaitForResult());

  EXPECT_EQ(ERR_IO_PENDING, stream1->ReadResponseHeaders(&callback_));
  EXPECT_EQ(ERR_IO_PENDING, stream2->ReadResponseHeaders(&callback_));

  data_->RunFor(2);
  EXPECT_LE(OK, callback_.WaitForResult());

  ExpectResponse("ok.html", stream1, true);

  stream1->Close(false);

  data_->RunFor(2);
  EXPECT_LE(OK, callback_.WaitForResult());

  ExpectResponse("ko.html", stream2, true);

  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, TwoResponsesInOnePacket) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 2,
             "HTTP/1.1 200 OK\r\n"
             "Content-Length: 7\r\n\r\n"
             "ok.html"
             "HTTP/1.1 200 OK\r\n"
             "Content-Length: 7\r\n\r\n"
             "ko.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1, NULL, &response1, &callback_));
  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2, NULL, &response2, &callback_));

  EXPECT_EQ(OK, stream1->ReadResponseHeaders(&callback_));
  ExpectResponse("ok.html", stream1, false);
  stream1->Close(false);

  EXPECT_EQ(OK, stream2->ReadResponseHeaders(&callback_));
  ExpectResponse("ko.html", stream2, false);
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, SendOrderSwapped) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ko.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 4, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 3, "ko.html"),
    MockRead(false, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 7, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  TestSyncRequest(stream2, "ko.html");
  TestSyncRequest(stream1, "ok.html");
}

TEST_F(HttpPipelinedConnectionImplTest, ReadOrderSwapped) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 4, "ok.html"),
    MockRead(false, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 7, "ko.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1, NULL, &response1, &callback_));

  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2, NULL, &response2, &callback_));

  EXPECT_EQ(ERR_IO_PENDING, stream2->ReadResponseHeaders(&callback_));

  EXPECT_EQ(OK, stream1->ReadResponseHeaders(&callback_));
  ExpectResponse("ok.html", stream1, false);

  stream1->Close(false);

  EXPECT_LE(OK, callback_.WaitForResult());
  ExpectResponse("ko.html", stream2, false);

  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, SendWhileReading) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 3, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 4, "ok.html"),
    MockRead(false, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 7, "ko.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1, NULL, &response1, &callback_));
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(&callback_));

  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2, NULL, &response2, &callback_));

  ExpectResponse("ok.html", stream1, false);
  stream1->Close(false);

  EXPECT_EQ(OK, stream2->ReadResponseHeaders(&callback_));
  ExpectResponse("ko.html", stream2, false);
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, AsyncSendWhileAsyncReadBlocked) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(true, 3, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(true, 4, "ok.html"),
    MockRead(false, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 7, "ko.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1, NULL, &response1, &callback_));
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(&callback_));
  TestOldCompletionCallback callback1;
  std::string expected = "ok.html";
  scoped_refptr<IOBuffer> buffer(new IOBuffer(expected.size()));
  EXPECT_EQ(ERR_IO_PENDING,
            stream1->ReadResponseBody(buffer.get(), expected.size(),
                                      &callback1));

  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  TestOldCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            stream2->SendRequest(headers2, NULL, &response2, &callback2));

  data_->RunFor(1);
  EXPECT_LE(OK, callback2.WaitForResult());
  EXPECT_EQ(ERR_IO_PENDING, stream2->ReadResponseHeaders(&callback2));

  data_->RunFor(1);
  EXPECT_EQ(static_cast<int>(expected.size()), callback1.WaitForResult());
  std::string actual(buffer->data(), expected.size());
  EXPECT_THAT(actual, StrEq(expected));
  stream1->Close(false);

  data_->StopAfter(8);
  EXPECT_LE(OK, callback2.WaitForResult());
  ExpectResponse("ko.html", stream2, false);
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, UnusedStreamAllowsLaterUse) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 3, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> unused_stream(NewTestStream("unused.html"));
  unused_stream->Close(false);

  scoped_ptr<HttpStream> later_stream(NewTestStream("ok.html"));
  TestSyncRequest(later_stream, "ok.html");
}

TEST_F(HttpPipelinedConnectionImplTest, UnsentStreamAllowsLaterUse) {
  MockWrite writes[] = {
    MockWrite(true, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 4, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(true, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(true, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(true, 3, "ok.html"),
    MockRead(false, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 7, "ko.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequest(headers, NULL, &response, &callback_));

  scoped_ptr<HttpStream> unsent_stream(NewTestStream("unsent.html"));
  HttpRequestHeaders unsent_headers;
  HttpResponseInfo unsent_response;
  EXPECT_EQ(ERR_IO_PENDING,
            unsent_stream->SendRequest(unsent_headers, NULL, &unsent_response,
                                       &callback_));
  unsent_stream->Close(false);

  data_->RunFor(1);
  EXPECT_LE(OK, callback_.WaitForResult());

  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(&callback_));
  data_->RunFor(2);
  EXPECT_LE(OK, callback_.WaitForResult());

  ExpectResponse("ok.html", stream, true);

  stream->Close(false);

  data_->StopAfter(8);
  scoped_ptr<HttpStream> later_stream(NewTestStream("ko.html"));
  TestSyncRequest(later_stream, "ko.html");
}

TEST_F(HttpPipelinedConnectionImplTest, FailedSend) {
  MockWrite writes[] = {
    MockWrite(true, ERR_FAILED),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  scoped_ptr<HttpStream> failed_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));
  scoped_ptr<HttpStream> closed_stream(NewTestStream("closed.html"));
  scoped_ptr<HttpStream> rejected_stream(NewTestStream("rejected.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  TestOldCompletionCallback failed_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            failed_stream->SendRequest(headers, NULL, &response,
                                       &failed_callback));
  TestOldCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->SendRequest(headers, NULL, &response,
                                        &evicted_callback));
  EXPECT_EQ(ERR_IO_PENDING,
            closed_stream->SendRequest(headers, NULL, &response,
                                       &callback_));
  closed_stream->Close(false);

  data_->RunFor(1);
  EXPECT_EQ(ERR_FAILED, failed_callback.WaitForResult());
  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            rejected_stream->SendRequest(headers, NULL, &response,
                                         &callback_));

  failed_stream->Close(true);
  evicted_stream->Close(true);
  rejected_stream->Close(true);
}

TEST_F(HttpPipelinedConnectionImplTest, ConnectionSuddenlyClosedAfterResponse) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /read_evicted.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 2, "GET /read_rejected.html HTTP/1.1\r\n\r\n"),
    MockWrite(true, ERR_SOCKET_NOT_CONNECTED, 5),
  };
  MockRead reads[] = {
    MockRead(false, 3, "HTTP/1.1 200 OK\r\n\r\n"),
    MockRead(false, 4, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> closed_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> read_evicted_stream(
      NewTestStream("read_evicted.html"));
  scoped_ptr<HttpStream> read_rejected_stream(
      NewTestStream("read_rejected.html"));
  scoped_ptr<HttpStream> send_closed_stream(
      NewTestStream("send_closed.html"));
  scoped_ptr<HttpStream> send_evicted_stream(
      NewTestStream("send_evicted.html"));
  scoped_ptr<HttpStream> send_rejected_stream(
      NewTestStream("send_rejected.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, closed_stream->SendRequest(headers, NULL, &response,
                                           &callback_));
  EXPECT_EQ(OK, read_evicted_stream->SendRequest(headers, NULL, &response,
                                                 &callback_));
  EXPECT_EQ(OK, read_rejected_stream->SendRequest(headers, NULL, &response,
                                                  &callback_));
  TestOldCompletionCallback send_closed_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            send_closed_stream->SendRequest(headers, NULL, &response,
                                            &send_closed_callback));
  TestOldCompletionCallback send_evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            send_evicted_stream->SendRequest(headers, NULL, &response,
                                             &send_evicted_callback));

  TestOldCompletionCallback read_evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            read_evicted_stream->ReadResponseHeaders(&read_evicted_callback));

  EXPECT_EQ(OK, closed_stream->ReadResponseHeaders(&callback_));
  ExpectResponse("ok.html", closed_stream, false);
  closed_stream->Close(true);

  EXPECT_EQ(ERR_PIPELINE_EVICTION, read_evicted_callback.WaitForResult());
  read_evicted_stream->Close(true);

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            read_rejected_stream->ReadResponseHeaders(&callback_));
  read_rejected_stream->Close(true);

  data_->RunFor(1);
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, send_closed_callback.WaitForResult());
  send_closed_stream->Close(true);

  EXPECT_EQ(ERR_PIPELINE_EVICTION, send_evicted_callback.WaitForResult());
  send_evicted_stream->Close(true);

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            send_rejected_stream->SendRequest(headers, NULL, &response,
                                              &callback_));
  send_rejected_stream->Close(true);
}

TEST_F(HttpPipelinedConnectionImplTest, AbortWhileSending) {
  MockWrite writes[] = {
    MockWrite(true, 0, "GET /aborts.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  scoped_ptr<HttpStream> aborted_stream(NewTestStream("aborts.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  TestOldCompletionCallback aborted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            aborted_stream->SendRequest(headers, NULL, &response,
                                        &aborted_callback));
  TestOldCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->SendRequest(headers, NULL, &response,
                                        &evicted_callback));

  aborted_stream->Close(true);
  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  evicted_stream->Close(true);
  EXPECT_FALSE(aborted_callback.have_result());
}

TEST_F(HttpPipelinedConnectionImplTest, AbortWhileSendingSecondRequest) {
  MockWrite writes[] = {
    MockWrite(true, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(true, 1, "GET /aborts.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> aborted_stream(NewTestStream("aborts.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  TestOldCompletionCallback ok_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            ok_stream->SendRequest(headers, NULL, &response,
                                   &ok_callback));
  TestOldCompletionCallback aborted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            aborted_stream->SendRequest(headers, NULL, &response,
                                        &aborted_callback));
  TestOldCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->SendRequest(headers, NULL, &response,
                                        &evicted_callback));

  data_->RunFor(1);
  EXPECT_LE(OK, ok_callback.WaitForResult());
  MessageLoop::current()->RunAllPending();
  aborted_stream->Close(true);
  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  evicted_stream->Close(true);
  EXPECT_FALSE(aborted_callback.have_result());
  ok_stream->Close(true);
}

TEST_F(HttpPipelinedConnectionImplTest, AbortWhileReadingHeaders) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /aborts.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(true, ERR_FAILED, 2),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> aborted_stream(NewTestStream("aborts.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));
  scoped_ptr<HttpStream> rejected_stream(NewTestStream("rejected.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, aborted_stream->SendRequest(headers, NULL, &response,
                                            &callback_));
  EXPECT_EQ(OK, evicted_stream->SendRequest(headers, NULL, &response,
                                            &callback_));

  EXPECT_EQ(ERR_IO_PENDING, aborted_stream->ReadResponseHeaders(&callback_));
  TestOldCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(&evicted_callback));

  aborted_stream->Close(true);
  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  evicted_stream->Close(true);

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            rejected_stream->SendRequest(headers, NULL, &response, &callback_));
  rejected_stream->Close(true);
}

TEST_F(HttpPipelinedConnectionImplTest, PendingResponseAbandoned) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /abandoned.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 2, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 3, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 4, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 5, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> abandoned_stream(NewTestStream("abandoned.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, ok_stream->SendRequest(headers, NULL, &response, &callback_));
  EXPECT_EQ(OK, abandoned_stream->SendRequest(headers, NULL, &response,
                                              &callback_));
  EXPECT_EQ(OK, evicted_stream->SendRequest(headers, NULL, &response,
                                            &callback_));

  EXPECT_EQ(OK, ok_stream->ReadResponseHeaders(&callback_));
  TestOldCompletionCallback abandoned_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            abandoned_stream->ReadResponseHeaders(&abandoned_callback));
  TestOldCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(&evicted_callback));

  abandoned_stream->Close(false);

  ExpectResponse("ok.html", ok_stream, false);
  ok_stream->Close(false);

  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  evicted_stream->Close(true);
  EXPECT_FALSE(evicted_stream->IsConnectionReusable());
}

TEST_F(HttpPipelinedConnectionImplTest, DisconnectedAfterOneRequestRecovery) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /rejected.html HTTP/1.1\r\n\r\n"),
    MockWrite(true, ERR_SOCKET_NOT_CONNECTED, 5),
    MockWrite(false, ERR_SOCKET_NOT_CONNECTED, 7),
  };
  MockRead reads[] = {
    MockRead(false, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 4, "ok.html"),
    MockRead(false, ERR_SOCKET_NOT_CONNECTED, 6),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> rejected_read_stream(NewTestStream("rejected.html"));
  scoped_ptr<HttpStream> evicted_send_stream(NewTestStream("evicted.html"));
  scoped_ptr<HttpStream> rejected_send_stream(NewTestStream("rejected.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, ok_stream->SendRequest(headers, NULL, &response, &callback_));
  EXPECT_EQ(OK, rejected_read_stream->SendRequest(
      headers, NULL, &response, &callback_));

  EXPECT_EQ(OK, ok_stream->ReadResponseHeaders(&callback_));
  ExpectResponse("ok.html", ok_stream, false);
  ok_stream->Close(false);

  TestOldCompletionCallback read_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_send_stream->SendRequest(headers, NULL, &response,
                                             &read_callback));
  data_->RunFor(1);
  EXPECT_EQ(ERR_PIPELINE_EVICTION, read_callback.WaitForResult());

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            rejected_read_stream->ReadResponseHeaders(&callback_));
  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            rejected_send_stream->SendRequest(headers, NULL, &response,
                                              &callback_));

  rejected_read_stream->Close(true);
  rejected_send_stream->Close(true);
}

TEST_F(HttpPipelinedConnectionImplTest, DisconnectedPendingReadRecovery) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 4, "ok.html"),
    MockRead(false, ERR_SOCKET_NOT_CONNECTED, 5),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, ok_stream->SendRequest(headers, NULL, &response, &callback_));
  EXPECT_EQ(OK, evicted_stream->SendRequest(
      headers, NULL, &response, &callback_));

  EXPECT_EQ(OK, ok_stream->ReadResponseHeaders(&callback_));
  ExpectResponse("ok.html", ok_stream, false);

  TestOldCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(&evicted_callback));

  ok_stream->Close(false);

  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  evicted_stream->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, CloseCalledBeforeNextReadLoop) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 4, "ok.html"),
    MockRead(false, ERR_SOCKET_NOT_CONNECTED, 5),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, ok_stream->SendRequest(headers, NULL, &response, &callback_));
  EXPECT_EQ(OK, evicted_stream->SendRequest(
      headers, NULL, &response, &callback_));

  EXPECT_EQ(OK, ok_stream->ReadResponseHeaders(&callback_));
  ExpectResponse("ok.html", ok_stream, false);

  TestOldCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(&evicted_callback));

  ok_stream->Close(false);
  evicted_stream->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, CloseCalledBeforeReadCallback) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(false, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(false, 4, "ok.html"),
    MockRead(false, ERR_SOCKET_NOT_CONNECTED, 5),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, ok_stream->SendRequest(headers, NULL, &response, &callback_));
  EXPECT_EQ(OK, evicted_stream->SendRequest(
      headers, NULL, &response, &callback_));

  EXPECT_EQ(OK, ok_stream->ReadResponseHeaders(&callback_));
  ExpectResponse("ok.html", ok_stream, false);

  TestOldCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(&evicted_callback));

  ok_stream->Close(false);

  // The posted tasks should be:
  // 1. DoReadHeadersLoop, which will post:
  // 2. InvokeUserCallback
  SuddenCloseObserver observer(evicted_stream.get(), 2);
  MessageLoop::current()->AddTaskObserver(&observer);
  MessageLoop::current()->RunAllPending();
  EXPECT_FALSE(evicted_callback.have_result());
}

class StreamDeleter {
 public:
  StreamDeleter(HttpStream* stream) :
      stream_(stream),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &StreamDeleter::OnIOComplete)) {
  }

  OldCompletionCallbackImpl<StreamDeleter>* callback() { return &callback_; }

 private:
  void OnIOComplete(int result) {
    delete stream_;
  }

  HttpStream* stream_;
  OldCompletionCallbackImpl<StreamDeleter> callback_;
};

TEST_F(HttpPipelinedConnectionImplTest, CloseCalledDuringSendCallback) {
  MockWrite writes[] = {
    MockWrite(true, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  HttpStream* stream(NewTestStream("ok.html"));

  StreamDeleter deleter(stream);
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(headers, NULL, &response,
                                                deleter.callback()));
  data_->RunFor(1);
}

TEST_F(HttpPipelinedConnectionImplTest, CloseCalledDuringReadCallback) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(true, 2, "Content-Length: 7\r\n\r\n"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  HttpStream* stream(NewTestStream("ok.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, stream->SendRequest(headers, NULL, &response, &callback_));

  StreamDeleter deleter(stream);
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(deleter.callback()));
  data_->RunFor(1);
}

TEST_F(HttpPipelinedConnectionImplTest,
       CloseCalledDuringReadCallbackWithPendingRead) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /failed.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(true, 3, "Content-Length: 7\r\n\r\n"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  HttpStream* failed_stream(NewTestStream("failed.html"));
  HttpStream* evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK,
            failed_stream->SendRequest(headers, NULL, &response, &callback_));
  EXPECT_EQ(OK,
            evicted_stream->SendRequest(headers, NULL, &response, &callback_));

  StreamDeleter failed_deleter(failed_stream);
  EXPECT_EQ(ERR_IO_PENDING,
            failed_stream->ReadResponseHeaders(failed_deleter.callback()));
  StreamDeleter evicted_deleter(evicted_stream);
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(evicted_deleter.callback()));
  data_->RunFor(1);
}

TEST_F(HttpPipelinedConnectionImplTest, CloseOtherDuringReadCallback) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /deleter.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /deleted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(true, 3, "Content-Length: 7\r\n\r\n"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> deleter_stream(NewTestStream("deleter.html"));
  HttpStream* deleted_stream(NewTestStream("deleted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK,
            deleter_stream->SendRequest(headers, NULL, &response, &callback_));
  EXPECT_EQ(OK,
            deleted_stream->SendRequest(headers, NULL, &response, &callback_));

  StreamDeleter deleter(deleted_stream);
  EXPECT_EQ(ERR_IO_PENDING,
            deleter_stream->ReadResponseHeaders(deleter.callback()));
  EXPECT_EQ(ERR_IO_PENDING, deleted_stream->ReadResponseHeaders(&callback_));
  data_->RunFor(1);
}

TEST_F(HttpPipelinedConnectionImplTest, CloseBeforeSendCallbackRuns) {
  MockWrite writes[] = {
    MockWrite(true, 0, "GET /close.html HTTP/1.1\r\n\r\n"),
    MockWrite(true, 1, "GET /dummy.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  scoped_ptr<HttpStream> close_stream(NewTestStream("close.html"));
  scoped_ptr<HttpStream> dummy_stream(NewTestStream("dummy.html"));

  scoped_ptr<TestOldCompletionCallback> close_callback(
      new TestOldCompletionCallback);
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(ERR_IO_PENDING, close_stream->SendRequest(
      headers, NULL, &response, close_callback.get()));

  data_->RunFor(1);
  EXPECT_FALSE(close_callback->have_result());

  close_stream->Close(false);
  close_stream.reset();
  close_callback.reset();

  MessageLoop::current()->RunAllPending();
}

TEST_F(HttpPipelinedConnectionImplTest, CloseBeforeReadCallbackRuns) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /close.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 3, "GET /dummy.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(true, 2, "Content-Length: 7\r\n\r\n"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> close_stream(NewTestStream("close.html"));
  scoped_ptr<HttpStream> dummy_stream(NewTestStream("dummy.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK,
            close_stream->SendRequest(headers, NULL, &response, &callback_));

  scoped_ptr<TestOldCompletionCallback> close_callback(
      new TestOldCompletionCallback);
  EXPECT_EQ(ERR_IO_PENDING,
            close_stream->ReadResponseHeaders(close_callback.get()));

  data_->RunFor(1);
  EXPECT_FALSE(close_callback->have_result());

  close_stream->Close(false);
  close_stream.reset();
  close_callback.reset();

  MessageLoop::current()->RunAllPending();
}

TEST_F(HttpPipelinedConnectionImplTest, RecoverFromDrainOnRedirect) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /redirect.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 2,
             "HTTP/1.1 302 OK\r\n"
             "Content-Length: 8\r\n\r\n"
             "redirect"),
    MockRead(false, 3,
             "HTTP/1.1 200 OK\r\n"
             "Content-Length: 7\r\n\r\n"
             "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("redirect.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ok.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1, NULL, &response1, &callback_));
  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2, NULL, &response2, &callback_));

  EXPECT_EQ(OK, stream1->ReadResponseHeaders(&callback_));
  stream1.release()->Drain(NULL);

  EXPECT_EQ(OK, stream2->ReadResponseHeaders(&callback_));
  ExpectResponse("ok.html", stream2, false);
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, EvictAfterDrainOfUnknownSize) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /redirect.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 2,
             "HTTP/1.1 302 OK\r\n\r\n"
             "redirect"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("redirect.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ok.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1, NULL, &response1, &callback_));
  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2, NULL, &response2, &callback_));

  EXPECT_EQ(OK, stream1->ReadResponseHeaders(&callback_));
  stream1.release()->Drain(NULL);

  EXPECT_EQ(ERR_PIPELINE_EVICTION, stream2->ReadResponseHeaders(&callback_));
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, EvictAfterFailedDrain) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /redirect.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 2,
             "HTTP/1.1 302 OK\r\n"
             "Content-Length: 8\r\n\r\n"),
    MockRead(false, ERR_SOCKET_NOT_CONNECTED, 3),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("redirect.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ok.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1, NULL, &response1, &callback_));
  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2, NULL, &response2, &callback_));

  EXPECT_EQ(OK, stream1->ReadResponseHeaders(&callback_));
  stream1.release()->Drain(NULL);

  EXPECT_EQ(ERR_PIPELINE_EVICTION, stream2->ReadResponseHeaders(&callback_));
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, EvictIfDrainingChunkedEncoding) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /redirect.html HTTP/1.1\r\n\r\n"),
    MockWrite(false, 1, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 2,
             "HTTP/1.1 302 OK\r\n"
             "Transfer-Encoding: chunked\r\n\r\n"),
    MockRead(false, 3,
             "jibberish"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("redirect.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ok.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1, NULL, &response1, &callback_));
  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2, NULL, &response2, &callback_));

  EXPECT_EQ(OK, stream1->ReadResponseHeaders(&callback_));
  stream1.release()->Drain(NULL);

  EXPECT_EQ(ERR_PIPELINE_EVICTION, stream2->ReadResponseHeaders(&callback_));
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, OnPipelineHasCapacity) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(0);
  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));

  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(1);
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, stream->SendRequest(headers, NULL, &response, &callback_));

  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(0);
  MessageLoop::current()->RunAllPending();

  stream->Close(false);
  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(1);
  stream.reset(NULL);
}

TEST_F(HttpPipelinedConnectionImplTest, OnPipelineHasCapacityWithoutSend) {
  MockWrite writes[] = {
    MockWrite(false, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(0);
  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));

  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(1);
  MessageLoop::current()->RunAllPending();

  stream->Close(false);
  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(1);
  stream.reset(NULL);
}

}  // namespace net
