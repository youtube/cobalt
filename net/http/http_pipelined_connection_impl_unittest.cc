// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_connection_impl.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
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

using testing::_;
using testing::NiceMock;
using testing::StrEq;

namespace net {

class DummySocketParams : public base::RefCounted<DummySocketParams> {
 private:
  friend class base::RefCounted<DummySocketParams>;
  ~DummySocketParams() {}
};

REGISTER_SOCKET_PARAMS_FOR_POOL(MockTransportClientSocketPool,
                                DummySocketParams);

namespace {

class MockPipelineDelegate : public HttpPipelinedConnection::Delegate {
 public:
  MOCK_METHOD1(OnPipelineHasCapacity, void(HttpPipelinedConnection* pipeline));
  MOCK_METHOD2(OnPipelineFeedback, void(
      HttpPipelinedConnection* pipeline,
      HttpPipelinedConnection::Feedback feedback));
};

class SuddenCloseObserver : public MessageLoop::TaskObserver {
 public:
  SuddenCloseObserver(HttpStream* stream, int close_before_task)
      : stream_(stream),
        close_before_task_(close_before_task),
        current_task_(0) { }

  virtual void WillProcessTask(base::TimeTicks) override {
    ++current_task_;
    if (current_task_ == close_before_task_) {
      stream_->Close(false);
      MessageLoop::current()->RemoveTaskObserver(this);
    }
  }

  virtual void DidProcessTask(base::TimeTicks) override { }

 private:
  HttpStream* stream_;
  int close_before_task_;
  int current_task_;
};

class HttpPipelinedConnectionImplTest : public testing::Test {
 public:
  HttpPipelinedConnectionImplTest()
      : histograms_("a"),
        pool_(1, 1, &histograms_, &factory_),
        origin_("host", 123) {
  }

  void TearDown() {
    MessageLoop::current()->RunUntilIdle();
  }

  void Initialize(MockRead* reads, size_t reads_count,
                  MockWrite* writes, size_t writes_count) {
    data_.reset(new DeterministicSocketData(reads, reads_count,
                                            writes, writes_count));
    data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
    if (reads_count || writes_count) {
      data_->StopAfter(reads_count + writes_count);
    }
    factory_.AddSocketDataProvider(data_.get());
    scoped_refptr<DummySocketParams> params;
    ClientSocketHandle* connection = new ClientSocketHandle;
    connection->Init("a", params, MEDIUM, CompletionCallback(), &pool_,
                     BoundNetLog());
    pipeline_.reset(new HttpPipelinedConnectionImpl(
        connection, &delegate_, origin_, ssl_config_, proxy_info_,
        BoundNetLog(), false, kProtoUnknown));
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
    int rv = stream->InitializeStream(
        request_info, BoundNetLog(), CompletionCallback());
    DCHECK_EQ(OK, rv);
    return stream;
  }

  void ExpectResponse(const std::string& expected,
                      scoped_ptr<HttpStream>& stream, bool async) {
    scoped_refptr<IOBuffer> buffer(new IOBuffer(expected.size()));

    if (async) {
      EXPECT_EQ(ERR_IO_PENDING,
                stream->ReadResponseBody(buffer.get(), expected.size(),
                                         callback_.callback()));
      data_->RunFor(1);
      EXPECT_EQ(static_cast<int>(expected.size()), callback_.WaitForResult());
    } else {
      EXPECT_EQ(static_cast<int>(expected.size()),
                stream->ReadResponseBody(buffer.get(), expected.size(),
                                         callback_.callback()));
    }
    std::string actual(buffer->data(), expected.size());
    EXPECT_THAT(actual, StrEq(expected));
  }

  void TestSyncRequest(scoped_ptr<HttpStream>& stream,
                       const std::string& filename) {
    HttpRequestHeaders headers;
    HttpResponseInfo response;
    EXPECT_EQ(OK, stream->SendRequest(headers, &response,
                                      callback_.callback()));
    EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
    ExpectResponse(filename, stream, false);

    stream->Close(false);
  }

  DeterministicMockClientSocketFactory factory_;
  ClientSocketPoolHistograms histograms_;
  MockTransportClientSocketPool pool_;
  scoped_ptr<DeterministicSocketData> data_;

  HostPortPair origin_;
  SSLConfig ssl_config_;
  ProxyInfo proxy_info_;
  NiceMock<MockPipelineDelegate> delegate_;
  TestCompletionCallback callback_;
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
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));
  TestSyncRequest(stream, "ok.html");
}

TEST_F(HttpPipelinedConnectionImplTest, AsyncSingleRequest) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(ASYNC, 3, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(headers, &response,
                                                callback_.callback()));
  data_->RunFor(1);
  EXPECT_LE(OK, callback_.WaitForResult());

  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));
  data_->RunFor(2);
  EXPECT_LE(OK, callback_.WaitForResult());

  ExpectResponse("ok.html", stream, true);

  stream->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, LockStepAsyncRequests) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(ASYNC, 1, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(ASYNC, 4, "ok.html"),
    MockRead(ASYNC, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(ASYNC, 7, "ko.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(ERR_IO_PENDING, stream1->SendRequest(headers1, &response1,
                                                 callback_.callback()));

  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(ERR_IO_PENDING, stream2->SendRequest(headers2, &response2,
                                                 callback_.callback()));

  data_->RunFor(1);
  EXPECT_LE(OK, callback_.WaitForResult());
  data_->RunFor(1);
  EXPECT_LE(OK, callback_.WaitForResult());

  EXPECT_EQ(ERR_IO_PENDING, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(ERR_IO_PENDING, stream2->ReadResponseHeaders(callback_.callback()));

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
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2,
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
  EXPECT_EQ(OK, stream1->SendRequest(headers1,
                                     &response1, callback_.callback()));
  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2,
                                     &response2, callback_.callback()));

  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  ExpectResponse("ok.html", stream1, false);
  stream1->Close(false);

  EXPECT_EQ(OK, stream2->ReadResponseHeaders(callback_.callback()));
  ExpectResponse("ko.html", stream2, false);
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, SendOrderSwapped) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ko.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 4, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "ko.html"),
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 7, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  TestSyncRequest(stream2, "ko.html");
  TestSyncRequest(stream1, "ok.html");
}

TEST_F(HttpPipelinedConnectionImplTest, ReadOrderSwapped) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "ok.html"),
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 7, "ko.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1,
                                     &response1, callback_.callback()));

  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2,
                                     &response2, callback_.callback()));

  EXPECT_EQ(ERR_IO_PENDING, stream2->ReadResponseHeaders(callback_.callback()));

  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  ExpectResponse("ok.html", stream1, false);

  stream1->Close(false);

  EXPECT_LE(OK, callback_.WaitForResult());
  ExpectResponse("ko.html", stream2, false);

  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, SendWhileReading) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 3, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "ok.html"),
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 7, "ko.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1,
                                     &response1, callback_.callback()));
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));

  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2,
                                     &response2, callback_.callback()));

  ExpectResponse("ok.html", stream1, false);
  stream1->Close(false);

  EXPECT_EQ(OK, stream2->ReadResponseHeaders(callback_.callback()));
  ExpectResponse("ko.html", stream2, false);
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, AsyncSendWhileAsyncReadBlocked) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(ASYNC, 3, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(ASYNC, 4, "ok.html"),
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 7, "ko.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1,
                                     &response1, callback_.callback()));
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  TestCompletionCallback callback1;
  std::string expected = "ok.html";
  scoped_refptr<IOBuffer> buffer(new IOBuffer(expected.size()));
  EXPECT_EQ(ERR_IO_PENDING,
            stream1->ReadResponseBody(buffer.get(), expected.size(),
                                      callback1.callback()));

  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING, stream2->SendRequest(headers2, &response2,
                                                 callback2.callback()));

  data_->RunFor(1);
  EXPECT_LE(OK, callback2.WaitForResult());
  EXPECT_EQ(ERR_IO_PENDING, stream2->ReadResponseHeaders(callback2.callback()));

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
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> unused_stream(NewTestStream("unused.html"));
  unused_stream->Close(false);

  scoped_ptr<HttpStream> later_stream(NewTestStream("ok.html"));
  TestSyncRequest(later_stream, "ok.html");
}

TEST_F(HttpPipelinedConnectionImplTest, UnsentStreamAllowsLaterUse) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 4, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 7\r\n\r\n"),
    MockRead(ASYNC, 3, "ok.html"),
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 7, "ko.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(headers, &response,
                                                callback_.callback()));

  scoped_ptr<HttpStream> unsent_stream(NewTestStream("unsent.html"));
  HttpRequestHeaders unsent_headers;
  HttpResponseInfo unsent_response;
  EXPECT_EQ(ERR_IO_PENDING, unsent_stream->SendRequest(unsent_headers,
                                                       &unsent_response,
                                                       callback_.callback()));
  unsent_stream->Close(false);

  data_->RunFor(1);
  EXPECT_LE(OK, callback_.WaitForResult());

  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));
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
    MockWrite(ASYNC, ERR_FAILED),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  scoped_ptr<HttpStream> failed_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));
  scoped_ptr<HttpStream> closed_stream(NewTestStream("closed.html"));
  scoped_ptr<HttpStream> rejected_stream(NewTestStream("rejected.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  TestCompletionCallback failed_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            failed_stream->SendRequest(headers, &response,
                                       failed_callback.callback()));
  TestCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->SendRequest(headers, &response,
                                        evicted_callback.callback()));
  EXPECT_EQ(ERR_IO_PENDING, closed_stream->SendRequest(headers, &response,
                                                       callback_.callback()));
  closed_stream->Close(false);

  data_->RunFor(1);
  EXPECT_EQ(ERR_FAILED, failed_callback.WaitForResult());
  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            rejected_stream->SendRequest(headers, &response,
                                         callback_.callback()));

  failed_stream->Close(true);
  evicted_stream->Close(true);
  rejected_stream->Close(true);
}

TEST_F(HttpPipelinedConnectionImplTest, ConnectionSuddenlyClosedAfterResponse) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /read_evicted.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 2, "GET /read_rejected.html HTTP/1.1\r\n\r\n"),
    MockWrite(ASYNC, ERR_SOCKET_NOT_CONNECTED, 5),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "ok.html"),
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
  EXPECT_EQ(OK, closed_stream->SendRequest(headers,
                                           &response, callback_.callback()));
  EXPECT_EQ(OK, read_evicted_stream->SendRequest(headers, &response,
                                                 callback_.callback()));
  EXPECT_EQ(OK, read_rejected_stream->SendRequest(headers, &response,
                                                  callback_.callback()));
  TestCompletionCallback send_closed_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            send_closed_stream->SendRequest(headers, &response,
                                            send_closed_callback.callback()));
  TestCompletionCallback send_evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            send_evicted_stream->SendRequest(headers, &response,
                                             send_evicted_callback.callback()));

  TestCompletionCallback read_evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            read_evicted_stream->ReadResponseHeaders(
                read_evicted_callback.callback()));

  EXPECT_EQ(OK, closed_stream->ReadResponseHeaders(callback_.callback()));
  ExpectResponse("ok.html", closed_stream, false);
  closed_stream->Close(true);

  EXPECT_EQ(ERR_PIPELINE_EVICTION, read_evicted_callback.WaitForResult());
  read_evicted_stream->Close(true);

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            read_rejected_stream->ReadResponseHeaders(callback_.callback()));
  read_rejected_stream->Close(true);

  data_->RunFor(1);
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, send_closed_callback.WaitForResult());
  send_closed_stream->Close(true);

  EXPECT_EQ(ERR_PIPELINE_EVICTION, send_evicted_callback.WaitForResult());
  send_evicted_stream->Close(true);

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            send_rejected_stream->SendRequest(headers, &response,
                                              callback_.callback()));
  send_rejected_stream->Close(true);
}

TEST_F(HttpPipelinedConnectionImplTest, AbortWhileSending) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET /aborts.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  scoped_ptr<HttpStream> aborted_stream(NewTestStream("aborts.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  TestCompletionCallback aborted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            aborted_stream->SendRequest(headers, &response,
                                        aborted_callback.callback()));
  TestCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->SendRequest(headers, &response,
                                        evicted_callback.callback()));

  aborted_stream->Close(true);
  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  evicted_stream->Close(true);
  EXPECT_FALSE(aborted_callback.have_result());
}

TEST_F(HttpPipelinedConnectionImplTest, AbortWhileSendingSecondRequest) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(ASYNC, 1, "GET /aborts.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> aborted_stream(NewTestStream("aborts.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  TestCompletionCallback ok_callback;
  EXPECT_EQ(ERR_IO_PENDING, ok_stream->SendRequest(headers, &response,
                                                   ok_callback.callback()));
  TestCompletionCallback aborted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            aborted_stream->SendRequest(headers, &response,
                                        aborted_callback.callback()));
  TestCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->SendRequest(headers, &response,
                                        evicted_callback.callback()));

  data_->RunFor(1);
  EXPECT_LE(OK, ok_callback.WaitForResult());
  MessageLoop::current()->RunUntilIdle();
  aborted_stream->Close(true);
  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  evicted_stream->Close(true);
  EXPECT_FALSE(aborted_callback.have_result());
  ok_stream->Close(true);
}

TEST_F(HttpPipelinedConnectionImplTest, AbortWhileReadingHeaders) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /aborts.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, ERR_FAILED, 2),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> aborted_stream(NewTestStream("aborts.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));
  scoped_ptr<HttpStream> rejected_stream(NewTestStream("rejected.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK,
            aborted_stream->SendRequest(headers, &response,
                                        callback_.callback()));
  EXPECT_EQ(OK,
            evicted_stream->SendRequest(headers, &response,
                                        callback_.callback()));

  EXPECT_EQ(ERR_IO_PENDING,
            aborted_stream->ReadResponseHeaders(callback_.callback()));
  TestCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(evicted_callback.callback()));

  aborted_stream->Close(true);
  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  evicted_stream->Close(true);

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            rejected_stream->SendRequest(headers, &response,
                                         callback_.callback()));
  rejected_stream->Close(true);
}

TEST_F(HttpPipelinedConnectionImplTest, PendingResponseAbandoned) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /abandoned.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 2, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 4, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 5, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> abandoned_stream(NewTestStream("abandoned.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, ok_stream->SendRequest(headers, &response,
                                       callback_.callback()));
  EXPECT_EQ(OK, abandoned_stream->SendRequest(headers, &response,
                                              callback_.callback()));
  EXPECT_EQ(OK, evicted_stream->SendRequest(headers, &response,
                                            callback_.callback()));

  EXPECT_EQ(OK, ok_stream->ReadResponseHeaders(callback_.callback()));
  TestCompletionCallback abandoned_callback;
  EXPECT_EQ(ERR_IO_PENDING, abandoned_stream->ReadResponseHeaders(
      abandoned_callback.callback()));
  TestCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(evicted_callback.callback()));

  abandoned_stream->Close(false);

  ExpectResponse("ok.html", ok_stream, false);
  ok_stream->Close(false);

  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  evicted_stream->Close(true);
  EXPECT_FALSE(evicted_stream->IsConnectionReusable());
}

TEST_F(HttpPipelinedConnectionImplTest, DisconnectedAfterOneRequestRecovery) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /rejected.html HTTP/1.1\r\n\r\n"),
    MockWrite(ASYNC, ERR_SOCKET_NOT_CONNECTED, 5),
    MockWrite(SYNCHRONOUS, ERR_SOCKET_NOT_CONNECTED, 7),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "ok.html"),
    MockRead(SYNCHRONOUS, ERR_SOCKET_NOT_CONNECTED, 6),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> rejected_read_stream(NewTestStream("rejected.html"));
  scoped_ptr<HttpStream> evicted_send_stream(NewTestStream("evicted.html"));
  scoped_ptr<HttpStream> rejected_send_stream(NewTestStream("rejected.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, ok_stream->SendRequest(headers,
                                       &response, callback_.callback()));
  EXPECT_EQ(OK, rejected_read_stream->SendRequest(headers, &response,
                                                  callback_.callback()));

  EXPECT_EQ(OK, ok_stream->ReadResponseHeaders(callback_.callback()));
  ExpectResponse("ok.html", ok_stream, false);
  ok_stream->Close(false);

  TestCompletionCallback read_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_send_stream->SendRequest(headers, &response,
                                             read_callback.callback()));
  data_->RunFor(1);
  EXPECT_EQ(ERR_PIPELINE_EVICTION, read_callback.WaitForResult());

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            rejected_read_stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            rejected_send_stream->SendRequest(headers, &response,
                                              callback_.callback()));

  rejected_read_stream->Close(true);
  rejected_send_stream->Close(true);
}

TEST_F(HttpPipelinedConnectionImplTest, DisconnectedPendingReadRecovery) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "ok.html"),
    MockRead(SYNCHRONOUS, ERR_SOCKET_NOT_CONNECTED, 5),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, ok_stream->SendRequest(headers,
                                       &response, callback_.callback()));
  EXPECT_EQ(OK, evicted_stream->SendRequest(headers, &response,
                                            callback_.callback()));

  EXPECT_EQ(OK, ok_stream->ReadResponseHeaders(callback_.callback()));
  ExpectResponse("ok.html", ok_stream, false);

  TestCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(evicted_callback.callback()));

  ok_stream->Close(false);

  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  evicted_stream->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, CloseCalledBeforeNextReadLoop) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "ok.html"),
    MockRead(SYNCHRONOUS, ERR_SOCKET_NOT_CONNECTED, 5),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, ok_stream->SendRequest(headers,
                                       &response, callback_.callback()));
  EXPECT_EQ(OK, evicted_stream->SendRequest(headers, &response,
                                            callback_.callback()));

  EXPECT_EQ(OK, ok_stream->ReadResponseHeaders(callback_.callback()));
  ExpectResponse("ok.html", ok_stream, false);

  TestCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(evicted_callback.callback()));

  ok_stream->Close(false);
  evicted_stream->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, CloseCalledBeforeReadCallback) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "ok.html"),
    MockRead(SYNCHRONOUS, ERR_SOCKET_NOT_CONNECTED, 5),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, ok_stream->SendRequest(headers,
                                       &response, callback_.callback()));
  EXPECT_EQ(OK, evicted_stream->SendRequest(headers, &response,
                                            callback_.callback()));

  EXPECT_EQ(OK, ok_stream->ReadResponseHeaders(callback_.callback()));
  ExpectResponse("ok.html", ok_stream, false);

  TestCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(evicted_callback.callback()));

  ok_stream->Close(false);

  // The posted tasks should be:
  // 1. DoReadHeadersLoop, which will post:
  // 2. InvokeUserCallback
  SuddenCloseObserver observer(evicted_stream.get(), 2);
  MessageLoop::current()->AddTaskObserver(&observer);
  MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(evicted_callback.have_result());
}

class StreamDeleter {
 public:
  StreamDeleter(HttpStream* stream) :
      stream_(stream),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
          base::Bind(&StreamDeleter::OnIOComplete, base::Unretained(this)))) {
  }

  const CompletionCallback& callback() { return callback_; }

 private:
  void OnIOComplete(int result) {
    delete stream_;
  }

  HttpStream* stream_;
  CompletionCallback callback_;
};

TEST_F(HttpPipelinedConnectionImplTest, CloseCalledDuringSendCallback) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  HttpStream* stream(NewTestStream("ok.html"));

  StreamDeleter deleter(stream);
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(headers, &response,
                                                deleter.callback()));
  data_->RunFor(1);
}

TEST_F(HttpPipelinedConnectionImplTest, CloseCalledDuringReadCallback) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 7\r\n\r\n"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  HttpStream* stream(NewTestStream("ok.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, stream->SendRequest(headers,
                                    &response, callback_.callback()));

  StreamDeleter deleter(stream);
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(deleter.callback()));
  data_->RunFor(1);
}

TEST_F(HttpPipelinedConnectionImplTest,
       CloseCalledDuringReadCallbackWithPendingRead) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /failed.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /evicted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 7\r\n\r\n"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  HttpStream* failed_stream(NewTestStream("failed.html"));
  HttpStream* evicted_stream(NewTestStream("evicted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, failed_stream->SendRequest(headers, &response,
                                           callback_.callback()));
  EXPECT_EQ(OK, evicted_stream->SendRequest(headers, &response,
                                            callback_.callback()));

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
    MockWrite(SYNCHRONOUS, 0, "GET /deleter.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /deleted.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 7\r\n\r\n"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> deleter_stream(NewTestStream("deleter.html"));
  HttpStream* deleted_stream(NewTestStream("deleted.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, deleter_stream->SendRequest(headers,
                                            &response, callback_.callback()));
  EXPECT_EQ(OK, deleted_stream->SendRequest(headers,
                                            &response, callback_.callback()));

  StreamDeleter deleter(deleted_stream);
  EXPECT_EQ(ERR_IO_PENDING,
            deleter_stream->ReadResponseHeaders(deleter.callback()));
  EXPECT_EQ(ERR_IO_PENDING,
            deleted_stream->ReadResponseHeaders(callback_.callback()));
  data_->RunFor(1);
}

TEST_F(HttpPipelinedConnectionImplTest, CloseBeforeSendCallbackRuns) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET /close.html HTTP/1.1\r\n\r\n"),
    MockWrite(ASYNC, 1, "GET /dummy.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  scoped_ptr<HttpStream> close_stream(NewTestStream("close.html"));
  scoped_ptr<HttpStream> dummy_stream(NewTestStream("dummy.html"));

  scoped_ptr<TestCompletionCallback> close_callback(
      new TestCompletionCallback);
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(ERR_IO_PENDING,
            close_stream->SendRequest(headers,
                                      &response, close_callback->callback()));

  data_->RunFor(1);
  EXPECT_FALSE(close_callback->have_result());

  close_stream->Close(false);
  close_stream.reset();
  close_callback.reset();

  MessageLoop::current()->RunUntilIdle();
}

TEST_F(HttpPipelinedConnectionImplTest, CloseBeforeReadCallbackRuns) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /close.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 3, "GET /dummy.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 7\r\n\r\n"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> close_stream(NewTestStream("close.html"));
  scoped_ptr<HttpStream> dummy_stream(NewTestStream("dummy.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, close_stream->SendRequest(headers,
                                          &response, callback_.callback()));

  scoped_ptr<TestCompletionCallback> close_callback(
      new TestCompletionCallback);
  EXPECT_EQ(ERR_IO_PENDING,
            close_stream->ReadResponseHeaders(close_callback->callback()));

  data_->RunFor(1);
  EXPECT_FALSE(close_callback->have_result());

  close_stream->Close(false);
  close_stream.reset();
  close_callback.reset();

  MessageLoop::current()->RunUntilIdle();
}

TEST_F(HttpPipelinedConnectionImplTest, AbortWhileSendQueued) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(ASYNC, 1, "GET /ko.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ko.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  TestCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING, stream1->SendRequest(headers1, &response1,
                                                 callback1.callback()));

  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING, stream2->SendRequest(headers2, &response2,
                                                 callback2.callback()));

  stream2.reset();
  stream1->Close(true);

  EXPECT_FALSE(callback2.have_result());
}

TEST_F(HttpPipelinedConnectionImplTest, NoGapBetweenCloseAndEviction) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /close.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 2, "GET /dummy.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 7\r\n\r\n"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> close_stream(NewTestStream("close.html"));
  scoped_ptr<HttpStream> dummy_stream(NewTestStream("dummy.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, close_stream->SendRequest(headers, &response,
                                          callback_.callback()));

  TestCompletionCallback close_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            close_stream->ReadResponseHeaders(close_callback.callback()));

  EXPECT_EQ(OK, dummy_stream->SendRequest(headers, &response,
                                          callback_.callback()));

  TestCompletionCallback dummy_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            dummy_stream->ReadResponseHeaders(dummy_callback.callback()));

  close_stream->Close(true);
  close_stream.reset();

  EXPECT_TRUE(dummy_callback.have_result());
  EXPECT_EQ(ERR_PIPELINE_EVICTION, dummy_callback.WaitForResult());
  dummy_stream->Close(true);
  dummy_stream.reset();
  pipeline_.reset();
}

TEST_F(HttpPipelinedConnectionImplTest, RecoverFromDrainOnRedirect) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /redirect.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2,
             "HTTP/1.1 302 OK\r\n"
             "Content-Length: 8\r\n\r\n"
             "redirect"),
    MockRead(SYNCHRONOUS, 3,
             "HTTP/1.1 200 OK\r\n"
             "Content-Length: 7\r\n\r\n"
             "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("redirect.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ok.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1,
                                     &response1, callback_.callback()));
  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2,
                                     &response2, callback_.callback()));

  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  stream1.release()->Drain(NULL);

  EXPECT_EQ(OK, stream2->ReadResponseHeaders(callback_.callback()));
  ExpectResponse("ok.html", stream2, false);
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, EvictAfterDrainOfUnknownSize) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /redirect.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2,
             "HTTP/1.1 302 OK\r\n\r\n"
             "redirect"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("redirect.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ok.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1,
                                     &response1, callback_.callback()));
  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2,
                                     &response2, callback_.callback()));

  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  stream1.release()->Drain(NULL);

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            stream2->ReadResponseHeaders(callback_.callback()));
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, EvictAfterFailedDrain) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /redirect.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2,
             "HTTP/1.1 302 OK\r\n"
             "Content-Length: 8\r\n\r\n"),
    MockRead(SYNCHRONOUS, ERR_SOCKET_NOT_CONNECTED, 3),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("redirect.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ok.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1,
                                     &response1, callback_.callback()));
  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2,
                                     &response2, callback_.callback()));


  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  stream1.release()->Drain(NULL);

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            stream2->ReadResponseHeaders(callback_.callback()));
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, EvictIfDrainingChunkedEncoding) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /redirect.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 2,
             "HTTP/1.1 302 OK\r\n"
             "Transfer-Encoding: chunked\r\n\r\n"),
    MockRead(SYNCHRONOUS, 3,
             "jibberish"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> stream1(NewTestStream("redirect.html"));
  scoped_ptr<HttpStream> stream2(NewTestStream("ok.html"));

  HttpRequestHeaders headers1;
  HttpResponseInfo response1;
  EXPECT_EQ(OK, stream1->SendRequest(headers1,
                                     &response1, callback_.callback()));
  HttpRequestHeaders headers2;
  HttpResponseInfo response2;
  EXPECT_EQ(OK, stream2->SendRequest(headers2,
                                     &response2, callback_.callback()));


  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  stream1.release()->Drain(NULL);

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            stream2->ReadResponseHeaders(callback_.callback()));
  stream2->Close(false);
}

TEST_F(HttpPipelinedConnectionImplTest, EvictionDueToMissingContentLength) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "GET /evicted.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 2, "GET /rejected.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 3, "HTTP/1.1 200 OK\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "ok.html"),
    MockRead(SYNCHRONOUS, OK, 5),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  scoped_ptr<HttpStream> ok_stream(NewTestStream("ok.html"));
  scoped_ptr<HttpStream> evicted_stream(NewTestStream("evicted.html"));
  scoped_ptr<HttpStream> rejected_stream(NewTestStream("rejected.html"));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, ok_stream->SendRequest(headers,
                                       &response, callback_.callback()));
  EXPECT_EQ(OK, evicted_stream->SendRequest(headers,
                                            &response, callback_.callback()));
  EXPECT_EQ(OK, rejected_stream->SendRequest(headers,
                                             &response, callback_.callback()));

  TestCompletionCallback ok_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            ok_stream->ReadResponseHeaders(ok_callback.callback()));

  TestCompletionCallback evicted_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            evicted_stream->ReadResponseHeaders(evicted_callback.callback()));

  data_->RunFor(1);
  EXPECT_LE(OK, ok_callback.WaitForResult());
  data_->StopAfter(10);

  ExpectResponse("ok.html", ok_stream, false);
  ok_stream->Close(false);

  EXPECT_EQ(ERR_PIPELINE_EVICTION,
            rejected_stream->ReadResponseHeaders(callback_.callback()));
  rejected_stream->Close(true);
  EXPECT_EQ(ERR_PIPELINE_EVICTION, evicted_callback.WaitForResult());
  evicted_stream->Close(true);
}

TEST_F(HttpPipelinedConnectionImplTest, FeedbackOnSocketError) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_FAILED, 1),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  EXPECT_CALL(delegate_,
              OnPipelineFeedback(
                  pipeline_.get(),
                  HttpPipelinedConnection::PIPELINE_SOCKET_ERROR))
      .Times(1);

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, stream->SendRequest(headers,
                                    &response, callback_.callback()));
  EXPECT_EQ(ERR_FAILED, stream->ReadResponseHeaders(callback_.callback()));
}

TEST_F(HttpPipelinedConnectionImplTest, FeedbackOnNoInternetConnection) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_INTERNET_DISCONNECTED, 1),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  EXPECT_CALL(delegate_, OnPipelineFeedback(_, _))
      .Times(0);

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, stream->SendRequest(headers,
                                    &response, callback_.callback()));
  EXPECT_EQ(ERR_INTERNET_DISCONNECTED,
            stream->ReadResponseHeaders(callback_.callback()));
}

TEST_F(HttpPipelinedConnectionImplTest, FeedbackOnHttp10) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.0 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 7\r\n"),
    MockRead(SYNCHRONOUS, 3, "Connection: keep-alive\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  EXPECT_CALL(delegate_,
              OnPipelineFeedback(pipeline_.get(),
                                 HttpPipelinedConnection::OLD_HTTP_VERSION))
      .Times(1);

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));
  TestSyncRequest(stream, "ok.html");
}

TEST_F(HttpPipelinedConnectionImplTest, FeedbackOnMustClose) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 2, "Content-Length: 7\r\n"),
    MockRead(SYNCHRONOUS, 3, "Connection: close\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  EXPECT_CALL(delegate_,
              OnPipelineFeedback(
                  pipeline_.get(),
                  HttpPipelinedConnection::MUST_CLOSE_CONNECTION))
      .Times(1);

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));
  TestSyncRequest(stream, "ok.html");
}

TEST_F(HttpPipelinedConnectionImplTest, FeedbackOnNoContentLength) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n\r\n"),
    MockRead(SYNCHRONOUS, 2, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  EXPECT_CALL(delegate_,
              OnPipelineFeedback(
                  pipeline_.get(),
                  HttpPipelinedConnection::MUST_CLOSE_CONNECTION))
      .Times(1);

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));
  TestSyncRequest(stream, "ok.html");
}

TEST_F(HttpPipelinedConnectionImplTest, FeedbackOnAuthenticationRequired) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 401 Unauthorized\r\n"),
    MockRead(SYNCHRONOUS, 2, "WWW-Authenticate: NTLM\r\n"),
    MockRead(SYNCHRONOUS, 3, "Content-Length: 7\r\n\r\n"),
    MockRead(SYNCHRONOUS, 4, "ok.html"),
  };
  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  EXPECT_CALL(delegate_,
              OnPipelineFeedback(
                  pipeline_.get(),
                  HttpPipelinedConnection::AUTHENTICATION_REQUIRED))
      .Times(1);

  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));
  TestSyncRequest(stream, "ok.html");
}

TEST_F(HttpPipelinedConnectionImplTest, OnPipelineHasCapacity) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(0);
  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));

  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(1);
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  EXPECT_EQ(OK, stream->SendRequest(headers,
                                    &response, callback_.callback()));

  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(0);
  MessageLoop::current()->RunUntilIdle();

  stream->Close(false);
  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(1);
  stream.reset(NULL);
}

TEST_F(HttpPipelinedConnectionImplTest, OnPipelineHasCapacityWithoutSend) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET /ok.html HTTP/1.1\r\n\r\n"),
  };
  Initialize(NULL, 0, writes, arraysize(writes));

  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(0);
  scoped_ptr<HttpStream> stream(NewTestStream("ok.html"));

  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(1);
  MessageLoop::current()->RunUntilIdle();

  stream->Close(false);
  EXPECT_CALL(delegate_, OnPipelineHasCapacity(pipeline_.get())).Times(1);
  stream.reset(NULL);
}

}  // anonymous namespace

}  // namespace net
