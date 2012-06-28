// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log_unittest.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_stream.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util_spdy2.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace net::test_spdy2;

// TODO(ukai): factor out common part with spdy_http_stream_unittest.cc
//
namespace net {

namespace {

class TestSpdyStreamDelegate : public SpdyStream::Delegate {
 public:
  TestSpdyStreamDelegate(SpdyStream* stream,
                         IOBufferWithSize* buf,
                         const CompletionCallback& callback)
      : stream_(stream),
        buf_(buf),
        callback_(callback),
        send_headers_completed_(false),
        response_(new SpdyHeaderBlock),
        data_sent_(0),
        closed_(false) {}
  virtual ~TestSpdyStreamDelegate() {}

  virtual bool OnSendHeadersComplete(int status) {
    send_headers_completed_ = true;
    return true;
  }
  virtual int OnSendBody() {
    ADD_FAILURE() << "OnSendBody should not be called";
    return ERR_UNEXPECTED;
  }
  virtual int OnSendBodyComplete(int /*status*/, bool* /*eof*/) {
    ADD_FAILURE() << "OnSendBodyComplete should not be called";
    return ERR_UNEXPECTED;
  }

  virtual int OnResponseReceived(const SpdyHeaderBlock& response,
                                 base::Time response_time,
                                 int status) {
    EXPECT_TRUE(send_headers_completed_);
    *response_ = response;
    if (buf_) {
      EXPECT_EQ(ERR_IO_PENDING,
                stream_->WriteStreamData(buf_.get(), buf_->size(),
                                         DATA_FLAG_NONE));
    }
    return status;
  }
  virtual void OnDataReceived(const char* buffer, int bytes) {
    received_data_ += std::string(buffer, bytes);
  }
  virtual void OnDataSent(int length) {
    data_sent_ += length;
  }
  virtual void OnClose(int status) {
    closed_ = true;
    CompletionCallback callback = callback_;
    callback_.Reset();
    callback.Run(OK);
  }
  virtual void set_chunk_callback(net::ChunkCallback *) {}

  bool send_headers_completed() const { return send_headers_completed_; }
  const linked_ptr<SpdyHeaderBlock>& response() const {
    return response_;
  }
  const std::string& received_data() const { return received_data_; }
  int data_sent() const { return data_sent_; }
  bool closed() const {  return closed_; }

 private:
  SpdyStream* stream_;
  scoped_refptr<IOBufferWithSize> buf_;
  CompletionCallback callback_;
  bool send_headers_completed_;
  linked_ptr<SpdyHeaderBlock> response_;
  std::string received_data_;
  int data_sent_;
  bool closed_;
};

SpdyFrame* ConstructSpdyBodyFrame(const char* data, int length) {
  BufferedSpdyFramer framer(2);
  return framer.CreateDataFrame(1, data, length, DATA_FLAG_NONE);
}

}  // anonymous namespace

class SpdyStreamSpdy2Test : public testing::Test {
 protected:
  SpdyStreamSpdy2Test() {
  }

  scoped_refptr<SpdySession> CreateSpdySession() {
    HostPortPair host_port_pair("www.google.com", 80);
    HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
    scoped_refptr<SpdySession> session(
        session_->spdy_session_pool()->Get(pair, BoundNetLog()));
    return session;
  }

  virtual void SetUp() {
    SpdySession::set_default_protocol(kProtoSPDY2);
  }

  virtual void TearDown() {
    MessageLoop::current()->RunAllPending();
  }

  scoped_refptr<HttpNetworkSession> session_;

 private:
  SpdyTestStateHelper spdy_state_;
};

TEST_F(SpdyStreamSpdy2Test, SendDataAfterOpen) {
  SpdySessionDependencies session_deps;

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps);
  SpdySessionPoolPeer pool_peer_(session_->spdy_session_pool());

  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,
    1,
    0,
    ConvertRequestPriorityToSpdyPriority(LOWEST, 2),
    CONTROL_FLAG_NONE,
    false,
    INVALID,
    NULL,
    0,
    DATA_FLAG_NONE
  };
  static const char* const kGetHeaders[] = {
    "method",
    "GET",
    "scheme",
    "http",
    "host",
    "www.google.com",
    "url",
    "/",
    "version",
    "HTTP/1.1",
  };
  scoped_ptr<SpdyFrame> req(
      ConstructSpdyPacket(
          kSynStartHeader, NULL, 0, kGetHeaders, arraysize(kGetHeaders) / 2));
  scoped_ptr<SpdyFrame> msg(
      ConstructSpdyBodyFrame("\0hello!\xff", 8));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*msg),
  };
  writes[0].sequence_number = 0;
  writes[1].sequence_number = 2;

  scoped_ptr<SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> echo(
      ConstructSpdyBodyFrame("\0hello!\xff", 8));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*echo),
    MockRead(ASYNC, 0, 0), // EOF
  };
  reads[0].sequence_number = 1;
  reads[1].sequence_number = 3;
  reads[2].sequence_number = 4;

  scoped_ptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data->set_connect_data(connect_data);

  session_deps.socket_factory->AddSocketDataProvider(data.get());

  scoped_refptr<SpdySession> session(CreateSpdySession());
  const char* kStreamUrl = "http://www.google.com/";
  GURL url(kStreamUrl);

  HostPortPair host_port_pair("www.google.com", 80);
  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(host_port_pair, LOWEST, false, false,
                                OnHostResolutionCallback()));

  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(host_port_pair.ToString(), transport_params,
                                 LOWEST, CompletionCallback(),
                                 session_->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));
  session->InitializeWithSocket(connection.release(), false, OK);

  scoped_refptr<SpdyStream> stream;
  ASSERT_EQ(
      OK,
      session->CreateStream(url, LOWEST, &stream, BoundNetLog(),
                            CompletionCallback()));
  scoped_refptr<IOBufferWithSize> buf(new IOBufferWithSize(8));
  memcpy(buf->data(), "\0hello!\xff", 8);
  TestCompletionCallback callback;

  scoped_ptr<TestSpdyStreamDelegate> delegate(
      new TestSpdyStreamDelegate(stream.get(), buf.get(), callback.callback()));
  stream->SetDelegate(delegate.get());

  EXPECT_FALSE(stream->HasUrl());

  linked_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock);
  (*headers)["method"] = "GET";
  (*headers)["scheme"] = url.scheme();
  (*headers)["host"] = url.host();
  (*headers)["url"] = url.path();
  (*headers)["version"] = "HTTP/1.1";
  stream->set_spdy_headers(headers);
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(true));

  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_TRUE(delegate->send_headers_completed());
  EXPECT_EQ("200", (*delegate->response())["status"]);
  EXPECT_EQ("HTTP/1.1", (*delegate->response())["version"]);
  EXPECT_EQ(std::string("\0hello!\xff", 8), delegate->received_data());
  EXPECT_EQ(8, delegate->data_sent());
  EXPECT_TRUE(delegate->closed());
}

TEST_F(SpdyStreamSpdy2Test, PushedStream) {
  const char kStreamUrl[] = "http://www.google.com/";

  SpdySessionDependencies session_deps;
  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps);
  SpdySessionPoolPeer pool_peer_(session_->spdy_session_pool());
  scoped_refptr<SpdySession> spdy_session(CreateSpdySession());

  MockRead reads[] = {
    MockRead(ASYNC, 0, 0), // EOF
  };

  scoped_ptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads), NULL, 0));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data->set_connect_data(connect_data);

  session_deps.socket_factory->AddSocketDataProvider(data.get());

  HostPortPair host_port_pair("www.google.com", 80);
  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(host_port_pair, LOWEST, false, false,
                                OnHostResolutionCallback()));

  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(host_port_pair.ToString(), transport_params,
                                 LOWEST, CompletionCallback(),
                                 session_->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));
  spdy_session->InitializeWithSocket(connection.release(), false, OK);
  BoundNetLog net_log;

  // Conjure up a stream.
  scoped_refptr<SpdyStream> stream = new SpdyStream(spdy_session,
                                                    2,
                                                    true,
                                                    net_log);
  EXPECT_FALSE(stream->response_received());
  EXPECT_FALSE(stream->HasUrl());

  // Set a couple of headers.
  SpdyHeaderBlock response;
  response["url"] = kStreamUrl;
  stream->OnResponseReceived(response);

  // Send some basic headers.
  SpdyHeaderBlock headers;
  response["status"] = "200";
  response["version"] = "OK";
  stream->OnHeaders(headers);

  stream->set_response_received();
  EXPECT_TRUE(stream->response_received());
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());
}

TEST_F(SpdyStreamSpdy2Test, StreamError) {
  SpdySessionDependencies session_deps;

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps);
  SpdySessionPoolPeer pool_peer_(session_->spdy_session_pool());

  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,
    1,
    0,
    ConvertRequestPriorityToSpdyPriority(LOWEST, 2),
    CONTROL_FLAG_NONE,
    false,
    INVALID,
    NULL,
    0,
    DATA_FLAG_NONE
  };
  static const char* const kGetHeaders[] = {
    "method",
    "GET",
    "scheme",
    "http",
    "host",
    "www.google.com",
    "url",
    "/",
    "version",
    "HTTP/1.1",
  };
  scoped_ptr<SpdyFrame> req(
      ConstructSpdyPacket(
          kSynStartHeader, NULL, 0, kGetHeaders, arraysize(kGetHeaders) / 2));
  scoped_ptr<SpdyFrame> msg(
      ConstructSpdyBodyFrame("\0hello!\xff", 8));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*msg),
  };
  writes[0].sequence_number = 0;
  writes[1].sequence_number = 2;

  scoped_ptr<SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> echo(
      ConstructSpdyBodyFrame("\0hello!\xff", 8));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*echo),
    MockRead(ASYNC, 0, 0),  // EOF
  };
  reads[0].sequence_number = 1;
  reads[1].sequence_number = 3;
  reads[2].sequence_number = 4;

  CapturingBoundNetLog log;

  scoped_ptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data->set_connect_data(connect_data);

  session_deps.socket_factory->AddSocketDataProvider(data.get());

  scoped_refptr<SpdySession> session(CreateSpdySession());
  const char* kStreamUrl = "http://www.google.com/";
  GURL url(kStreamUrl);

  HostPortPair host_port_pair("www.google.com", 80);
  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(host_port_pair, LOWEST, false, false,
                                OnHostResolutionCallback()));

  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(host_port_pair.ToString(), transport_params,
                                 LOWEST, CompletionCallback(),
                                 session_->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 log.bound()));
  session->InitializeWithSocket(connection.release(), false, OK);

  scoped_refptr<SpdyStream> stream;
  ASSERT_EQ(
      OK,
      session->CreateStream(url, LOWEST, &stream, log.bound(),
                            CompletionCallback()));
  scoped_refptr<IOBufferWithSize> buf(new IOBufferWithSize(8));
  memcpy(buf->data(), "\0hello!\xff", 8);
  TestCompletionCallback callback;

  scoped_ptr<TestSpdyStreamDelegate> delegate(
      new TestSpdyStreamDelegate(stream.get(), buf.get(), callback.callback()));
  stream->SetDelegate(delegate.get());

  EXPECT_FALSE(stream->HasUrl());

  linked_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock);
  (*headers)["method"] = "GET";
  (*headers)["scheme"] = url.scheme();
  (*headers)["host"] = url.host();
  (*headers)["url"] = url.path();
  (*headers)["version"] = "HTTP/1.1";
  stream->set_spdy_headers(headers);
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(true));

  const SpdyStreamId stream_id = stream->stream_id();

  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_TRUE(delegate->send_headers_completed());
  EXPECT_EQ("200", (*delegate->response())["status"]);
  EXPECT_EQ("HTTP/1.1", (*delegate->response())["version"]);
  EXPECT_EQ(std::string("\0hello!\xff", 8), delegate->received_data());
  EXPECT_EQ(8, delegate->data_sent());
  EXPECT_TRUE(delegate->closed());

  // Check that the NetLog was filled reasonably.
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged SPDY_STREAM_ERROR correctly.
  int pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_SPDY_STREAM_ERROR,
      net::NetLog::PHASE_NONE);

  int stream_id2;
  ASSERT_TRUE(entries[pos].GetIntegerValue("stream_id", &stream_id2));
  EXPECT_EQ(static_cast<int>(stream_id), stream_id2);
}

}  // namespace net
