// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class SpdyHttpStreamTest : public testing::Test {
 public:
  OrderedSocketData* data() { return data_; }
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
    HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
    data_ = new OrderedSocketData(reads, reads_count, writes, writes_count);
    session_deps_.socket_factory->AddSocketDataProvider(data_.get());
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    session_ = http_session_->spdy_session_pool()->
      Get(pair, http_session_->mutable_spdy_settings(), BoundNetLog());
    tcp_params_ = new TCPSocketParams(host_port_pair.host(),
                                      host_port_pair.port(),
                                      MEDIUM, GURL(), false);
    TestCompletionCallback callback;
    scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
    EXPECT_EQ(ERR_IO_PENDING,
              connection->Init(host_port_pair.ToString(), tcp_params_, MEDIUM,
                                &callback, http_session_->tcp_socket_pool(),
                                BoundNetLog()));
    EXPECT_EQ(OK, callback.WaitForResult());
    return session_->InitializeWithSocket(connection.release(), false, OK);
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

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 1),
  };
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(false, 0, 3)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
  EXPECT_EQ(OK, InitSession(reads, arraysize(reads), writes, arraysize(writes),
      host_port_pair));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(
      new SpdyHttpStream(session_.get(), true));
  ASSERT_EQ(
      OK,
      http_stream->InitializeStream(&request, net_log, NULL));

  EXPECT_EQ(ERR_IO_PENDING,
            http_stream->SendRequest(headers, NULL, &response, &callback));
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(pair));

  // This triggers the MockWrite and read 2
  callback.WaitForResult();

  // This triggers read 3. The empty read causes the session to shut down.
  data()->CompleteRead();

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_TRUE(!http_session_->spdy_session_pool()->HasSession(pair));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

// Test case for bug: http://code.google.com/p/chromium/issues/detail?id=50058
TEST_F(SpdyHttpStreamTest, SpdyURLTest) {
  EnableCompression(false);
  SpdySession::SetSSLMode(false);

  const char * const full_url = "http://www.google.com/foo?query=what#anchor";
  const char * const base_url = "http://www.google.com/foo?query=what";
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(base_url, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 1),
  };
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(false, 0, 3)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
  EXPECT_EQ(OK, InitSession(reads, arraysize(reads), writes, arraysize(writes),
      host_port_pair));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL(full_url);
  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(new SpdyHttpStream(session_, true));
  ASSERT_EQ(
      OK,
      http_stream->InitializeStream(&request, net_log, NULL));

  EXPECT_EQ(ERR_IO_PENDING,
            http_stream->SendRequest(headers, NULL, &response, &callback));

  spdy::SpdyHeaderBlock* spdy_header =
    http_stream->stream()->spdy_headers().get();
  EXPECT_TRUE(spdy_header != NULL);
  if (spdy_header->find("url") != spdy_header->end())
    EXPECT_EQ("/foo?query=what", spdy_header->find("url")->second);
  else
    FAIL() << "No url is set in spdy_header!";

  // This triggers the MockWrite and read 2
  callback.WaitForResult();

  // This triggers read 3. The empty read causes the session to shut down.
  data()->CompleteRead();

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_TRUE(!http_session_->spdy_session_pool()->HasSession(pair));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

// TODO(willchan): Write a longer test for SpdyStream that exercises all
// methods.

}  // namespace net
