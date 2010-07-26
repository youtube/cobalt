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
  SpdyHttpStreamTest() {}

  void EnableCompression(bool enabled) {
    spdy::SpdyFramer::set_enable_compression_default(enabled);
  }

  virtual void TearDown() {
    MessageLoop::current()->RunAllPending();
  }
  int InitSession(MockRead* reads, size_t reads_count,
                  MockWrite* writes, size_t writes_count,
                  HostPortPair& host_port_pair) {
    data_ = new OrderedSocketData(reads, reads_count, writes, writes_count);
    session_deps_.socket_factory.AddSocketDataProvider(data_.get());
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    session_ = http_session_->spdy_session_pool()->
      Get(host_port_pair, http_session_.get(), BoundNetLog());
    tcp_params_ = new TCPSocketParams(host_port_pair.host(),
                                      host_port_pair.port(),
                                      MEDIUM, GURL(), false);
    return session_->Connect(host_port_pair.host(), tcp_params_, MEDIUM);
  }
  SpdySessionDependencies session_deps_;
  scoped_refptr<OrderedSocketData> data_;
  scoped_refptr<HttpNetworkSession> http_session_;
  scoped_refptr<SpdySession> session_;
  scoped_refptr<TCPSocketParams> tcp_params_;
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

  HostPortPair host_port_pair("www.google.com", 80);
  EXPECT_EQ(OK, InitSession(reads, arraysize(reads), writes, arraysize(writes),
      host_port_pair));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  TestCompletionCallback callback;
  HttpResponseInfo response;
  scoped_ptr<SpdyHttpStream> http_stream(new SpdyHttpStream());
  ASSERT_EQ(
      OK,
      http_stream->InitializeStream(session_, request, BoundNetLog(), NULL));
  http_stream->InitializeRequest(base::Time::Now(), NULL);

  EXPECT_EQ(ERR_IO_PENDING,
            http_stream->SendRequest(&response, &callback));
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(host_port_pair));
  http_session_->spdy_session_pool()->Remove(session_);
}

// Test case for bug: http://code.google.com/p/chromium/issues/detail?id=50058
TEST_F(SpdyHttpStreamTest, SpdyURLTest) {
  EnableCompression(false);
  SpdySession::SetSSLMode(false);

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 1),
  };
  MockRead reads[] = {
    MockRead(false, 0, 2),  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  EXPECT_EQ(OK, InitSession(reads, arraysize(reads), writes, arraysize(writes),
      host_port_pair));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/foo?query=what#anchor");
  TestCompletionCallback callback;
  HttpResponseInfo response;
  scoped_ptr<SpdyHttpStream> http_stream(new SpdyHttpStream());
  ASSERT_EQ(
      OK,
      http_stream->InitializeStream(session_, request, BoundNetLog(), NULL));
  http_stream->InitializeRequest(base::Time::Now(), NULL);

  spdy::SpdyHeaderBlock* spdy_header =
    http_stream->stream()->spdy_headers().get();
  if (spdy_header->find("url") != spdy_header->end())
    EXPECT_EQ("http://www.google.com/foo?query=what",
              spdy_header->find("url")->second);
  else
    FAIL() << "No url is set in spdy_header!";

  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(host_port_pair));
  http_session_->spdy_session_pool()->Remove(session_);
}

// TODO(willchan): Write a longer test for SpdyStream that exercises all
// methods.

}  // namespace net
