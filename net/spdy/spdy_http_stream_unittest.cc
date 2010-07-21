// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class SpdyHttpStreamTest : public testing::Test {
 protected:
  SpdyHttpStreamTest(){}

  void EnableCompression(bool enabled) {
    spdy::SpdyFramer::set_enable_compression_default(enabled);
  }

  virtual void TearDown() {
    MessageLoop::current()->RunAllPending();
  }
};

TEST_F(SpdyHttpStreamTest, SendRequest) {
  EnableCompression(false);
  SpdySession::SetSSLMode(false);

  SpdySessionDependencies session_deps;
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 1),
  };
  MockRead reads[] = {
    MockRead(false, 0, 2)  // EOF
  };
  scoped_refptr<OrderedSocketData> data(
      new OrderedSocketData(reads, arraysize(reads),
                            writes, arraysize(writes)));
  session_deps.socket_factory.AddSocketDataProvider(data.get());

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));
  scoped_refptr<SpdySessionPool> spdy_session_pool(
      http_session->spdy_session_pool());
  HostPortPair host_port_pair("www.google.com", 80);
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(
          host_port_pair, http_session.get(), BoundNetLog());
  scoped_refptr<TCPSocketParams> tcp_params =
      new TCPSocketParams(host_port_pair.host, host_port_pair.port,
                          MEDIUM, GURL(), false);
  int rv = session->Connect(host_port_pair.host, tcp_params, MEDIUM);
  ASSERT_EQ(OK, rv);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  TestCompletionCallback callback;
  HttpResponseInfo response;
  scoped_ptr<SpdyHttpStream> http_stream(new SpdyHttpStream());
  ASSERT_EQ(
      OK,
      http_stream->InitializeStream(session, request, BoundNetLog(), NULL));
  http_stream->InitializeRequest(base::Time::Now(), NULL);
  EXPECT_EQ(ERR_IO_PENDING,
            http_stream->SendRequest(&response, &callback));
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(spdy_session_pool->HasSession(host_port_pair));
  spdy_session_pool->Remove(session);
}

// TODO(willchan): Write a longer test for SpdyStream that exercises all
// methods.

}  // namespace net
