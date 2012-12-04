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
#include "net/spdy/spdy_stream_test_util.h"
#include "net/spdy/spdy_test_util_spdy3.h"
#include "net/spdy/spdy_websocket_test_util_spdy3.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace net::test_spdy3;

// TODO(ukai): factor out common part with spdy_http_stream_unittest.cc
//
namespace net {

namespace {

SpdyFrame* ConstructSpdyBodyFrame(const char* data, int length) {
  BufferedSpdyFramer framer(3, false);
  return framer.CreateDataFrame(1, data, length, DATA_FLAG_NONE);
}

}  // anonymous namespace

namespace test {

class SpdyStreamSpdy3Test : public testing::Test {
 protected:
  SpdyStreamSpdy3Test() {
  }

  scoped_refptr<SpdySession> CreateSpdySession() {
    HostPortPair host_port_pair("www.google.com", 80);
    HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
    scoped_refptr<SpdySession> session(
        session_->spdy_session_pool()->Get(pair, BoundNetLog()));
    return session;
  }

  virtual void TearDown() {
    MessageLoop::current()->RunUntilIdle();
  }

  scoped_refptr<HttpNetworkSession> session_;
};

TEST_F(SpdyStreamSpdy3Test, SendDataAfterOpen) {
  SpdySessionDependencies session_deps;

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps);
  SpdySessionPoolPeer pool_peer_(session_->spdy_session_pool());

  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,
    1,
    0,
    ConvertRequestPriorityToSpdyPriority(LOWEST, 3),
    0,
    CONTROL_FLAG_NONE,
    false,
    INVALID,
    NULL,
    0,
    DATA_FLAG_NONE
  };
  static const char* const kGetHeaders[] = {
    ":method",
    "GET",
    ":scheme",
    "http",
    ":host",
    "www.google.com",
    ":path",
    "/",
    ":version",
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

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps.socket_factory->AddSocketDataProvider(&data);

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
      new TestSpdyStreamDelegate(
          stream.get(), NULL, buf.get(), callback.callback()));
  stream->SetDelegate(delegate.get());

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock);
  (*headers)[":method"] = "GET";
  (*headers)[":scheme"] = url.scheme();
  (*headers)[":host"] = url.host();
  (*headers)[":path"] = url.path();
  (*headers)[":version"] = "HTTP/1.1";
  stream->set_spdy_headers(headers.Pass());
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(true));

  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_TRUE(delegate->send_headers_completed());
  EXPECT_EQ("200", (*delegate->response())[":status"]);
  EXPECT_EQ("HTTP/1.1", (*delegate->response())[":version"]);
  EXPECT_EQ(std::string("\0hello!\xff", 8), delegate->received_data());
  EXPECT_EQ(8, delegate->data_sent());
  EXPECT_TRUE(delegate->closed());
}

TEST_F(SpdyStreamSpdy3Test, SendHeaderAndDataAfterOpen) {
  SpdySessionDependencies session_deps;

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps);
  SpdySessionPoolPeer pool_peer_(session_->spdy_session_pool());

  scoped_ptr<SpdyFrame> expected_request(ConstructSpdyWebSocketSynStream(
      1,
      "/chat",
      "server.example.com",
      "http://example.com"));
  scoped_ptr<SpdyFrame> expected_headers(ConstructSpdyWebSocketHeadersFrame(
      1, "6", true));
  scoped_ptr<SpdyFrame> expected_message(ConstructSpdyBodyFrame("hello!", 6));
  MockWrite writes[] = {
    CreateMockWrite(*expected_request),
    CreateMockWrite(*expected_headers),
    CreateMockWrite(*expected_message)
  };
  writes[0].sequence_number = 0;
  writes[1].sequence_number = 2;
  writes[1].sequence_number = 3;

  scoped_ptr<SpdyFrame> response(
      ConstructSpdyWebSocketSynReply(1));
  MockRead reads[] = {
    CreateMockRead(*response),
    MockRead(ASYNC, 0, 0), // EOF
  };
  reads[0].sequence_number = 1;
  reads[1].sequence_number = 4;

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());
  const char* kStreamUrl = "ws://server.example.com/chat";
  GURL url(kStreamUrl);

  HostPortPair host_port_pair("server.example.com", 80);
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
      session->CreateStream(url, HIGHEST, &stream, BoundNetLog(),
                            CompletionCallback()));
  scoped_refptr<IOBufferWithSize> buf(new IOBufferWithSize(6));
  memcpy(buf->data(), "hello!", 6);
  TestCompletionCallback callback;
  scoped_ptr<SpdyHeaderBlock> message_headers(new SpdyHeaderBlock);
  (*message_headers)[":opcode"] = "1";
  (*message_headers)[":length"] = "6";
  (*message_headers)[":fin"] = "1";

  scoped_ptr<TestSpdyStreamDelegate> delegate(
      new TestSpdyStreamDelegate(stream.get(),
                                 message_headers.release(),
                                 buf.get(),
                                 callback.callback()));
  stream->SetDelegate(delegate.get());

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock);
  (*headers)[":path"] = url.path();
  (*headers)[":host"] = url.host();
  (*headers)[":version"] = "WebSocket/13";
  (*headers)[":scheme"] = url.scheme();
  (*headers)[":origin"] = "http://example.com";
  stream->set_spdy_headers(headers.Pass());
  EXPECT_TRUE(stream->HasUrl());

  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(true));

  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_TRUE(delegate->send_headers_completed());
  EXPECT_EQ("101", (*delegate->response())[":status"]);
  EXPECT_EQ(1, delegate->headers_sent());
  EXPECT_EQ(std::string(), delegate->received_data());
  EXPECT_EQ(6, delegate->data_sent());
}

TEST_F(SpdyStreamSpdy3Test, PushedStream) {
  const char kStreamUrl[] = "http://www.google.com/";

  SpdySessionDependencies session_deps;
  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps);
  SpdySessionPoolPeer pool_peer_(session_->spdy_session_pool());
  scoped_refptr<SpdySession> spdy_session(CreateSpdySession());

  MockRead reads[] = {
    MockRead(ASYNC, 0, 0), // EOF
  };

  OrderedSocketData data(reads, arraysize(reads), NULL, 0);
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps.socket_factory->AddSocketDataProvider(&data);

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
                                                    true,
                                                    net_log);
  stream->set_stream_id(2);
  EXPECT_FALSE(stream->response_received());
  EXPECT_FALSE(stream->HasUrl());

  // Set a couple of headers.
  SpdyHeaderBlock response;
  GURL url(kStreamUrl);
  response[":host"] = url.host();
  response[":scheme"] = url.scheme();
  response[":path"] = url.path();
  stream->OnResponseReceived(response);

  // Send some basic headers.
  SpdyHeaderBlock headers;
  response[":status"] = "200";
  response[":version"] = "OK";
  stream->OnHeaders(headers);

  stream->set_response_received();
  EXPECT_TRUE(stream->response_received());
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());
}

TEST_F(SpdyStreamSpdy3Test, StreamError) {
  SpdySessionDependencies session_deps;

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps);
  SpdySessionPoolPeer pool_peer_(session_->spdy_session_pool());

  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,
    1,
    0,
    ConvertRequestPriorityToSpdyPriority(LOWEST, 3),
    0,
    CONTROL_FLAG_NONE,
    false,
    INVALID,
    NULL,
    0,
    DATA_FLAG_NONE
  };
  static const char* const kGetHeaders[] = {
    ":method",
    "GET",
    ":scheme",
    "http",
    ":host",
    "www.google.com",
    ":path",
    "/",
    ":version",
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

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps.socket_factory->AddSocketDataProvider(&data);

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
      new TestSpdyStreamDelegate(
          stream.get(), NULL, buf.get(), callback.callback()));
  stream->SetDelegate(delegate.get());

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(new SpdyHeaderBlock);
  (*headers)[":method"] = "GET";
  (*headers)[":scheme"] = url.scheme();
  (*headers)[":host"] = url.host();
  (*headers)[":path"] = url.path();
  (*headers)[":version"] = "HTTP/1.1";
  stream->set_spdy_headers(headers.Pass());
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(true));

  EXPECT_EQ(OK, callback.WaitForResult());

  const SpdyStreamId stream_id = stream->stream_id();

  EXPECT_TRUE(delegate->send_headers_completed());
  EXPECT_EQ("200", (*delegate->response())[":status"]);
  EXPECT_EQ("HTTP/1.1", (*delegate->response())[":version"]);
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

}  // namespace test

}  // namespace net
