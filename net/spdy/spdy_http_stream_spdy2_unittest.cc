// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_stream.h"

#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/signature_creator.h"
#include "net/base/asn1_util.h"
#include "net/base/default_server_bound_cert_store.h"
#include "net/base/upload_data.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util_spdy2.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace net::test_spdy2;

namespace net {

class SpdyHttpStreamSpdy2Test : public testing::Test {
 public:
  OrderedSocketData* data() { return data_.get(); }
 protected:
  SpdyHttpStreamSpdy2Test() {}

  virtual void TearDown() {
    crypto::ECSignatureCreator::SetFactoryForTesting(NULL);
    UploadDataStream::ResetMergeChunks();
    MessageLoop::current()->RunUntilIdle();
  }

  void set_merge_chunks(bool merge) {
    UploadDataStream::set_merge_chunks(merge);
  }

  int InitSession(MockRead* reads, size_t reads_count,
                  MockWrite* writes, size_t writes_count,
                  HostPortPair& host_port_pair) {
    HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
    data_.reset(new OrderedSocketData(reads, reads_count,
                                      writes, writes_count));
    session_deps_.socket_factory->AddSocketDataProvider(data_.get());
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    session_ = http_session_->spdy_session_pool()->Get(pair, BoundNetLog());
    transport_params_ = new TransportSocketParams(host_port_pair,
                                                  MEDIUM, false, false,
                                                  OnHostResolutionCallback());
    TestCompletionCallback callback;
    scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
    EXPECT_EQ(ERR_IO_PENDING,
              connection->Init(host_port_pair.ToString(),
                               transport_params_,
                               MEDIUM,
                               callback.callback(),
                               http_session_->GetTransportSocketPool(
                                   HttpNetworkSession::NORMAL_SOCKET_POOL),
                               BoundNetLog()));
    EXPECT_EQ(OK, callback.WaitForResult());
    return session_->InitializeWithSocket(connection.release(), false, OK);
  }

  SpdySessionDependencies session_deps_;
  scoped_ptr<OrderedSocketData> data_;
  scoped_refptr<HttpNetworkSession> http_session_;
  scoped_refptr<SpdySession> session_;
  scoped_refptr<TransportSocketParams> transport_params_;
};

TEST_F(SpdyHttpStreamSpdy2Test, SendRequest) {
  scoped_ptr<SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 1),
  };
  scoped_ptr<SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(SYNCHRONOUS, 0, 3)  // EOF
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
      http_stream->InitializeStream(&request, net_log, CompletionCallback()));

  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(pair));

  // This triggers the MockWrite and read 2
  callback.WaitForResult();

  // This triggers read 3. The empty read causes the session to shut down.
  data()->CompleteRead();

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_FALSE(http_session_->spdy_session_pool()->HasSession(pair));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

TEST_F(SpdyHttpStreamSpdy2Test, SendChunkedPost) {
  set_merge_chunks(false);

  scoped_ptr<SpdyFrame> req(ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<SpdyFrame> chunk1(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> chunk2(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 0),
    CreateMockWrite(*chunk1, 1),  // POST upload frames
    CreateMockWrite(*chunk2, 2),
  };
  scoped_ptr<SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp, 3),
    CreateMockRead(*chunk1, 4),
    CreateMockRead(*chunk2, 5),
    MockRead(SYNCHRONOUS, 0, 6)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
  EXPECT_EQ(OK, InitSession(reads, arraysize(reads), writes, arraysize(writes),
                            host_port_pair));

  scoped_refptr<UploadData> upload_data(new UploadData());
  upload_data->set_is_chunked(true);
  upload_data->AppendChunk(kUploadData, kUploadDataSize, false);
  upload_data->AppendChunk(kUploadData, kUploadDataSize, true);
  UploadDataStream upload_stream(upload_data);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_stream;

  ASSERT_EQ(OK, upload_stream.InitSync());

  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  BoundNetLog net_log;
  SpdyHttpStream http_stream(session_.get(), true);
  ASSERT_EQ(
      OK,
      http_stream.InitializeStream(&request, net_log, CompletionCallback()));

  EXPECT_EQ(ERR_IO_PENDING, http_stream.SendRequest(
      headers, &response, callback.callback()));
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(pair));

  // This triggers the MockWrite and read 2
  callback.WaitForResult();

  // This triggers read 3. The empty read causes the session to shut down.
  data()->CompleteRead();
  MessageLoop::current()->RunUntilIdle();

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_FALSE(http_session_->spdy_session_pool()->HasSession(pair));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

// Test to ensure the SpdyStream state machine does not get confused when a
// chunk becomes available while a write is pending.
TEST_F(SpdyHttpStreamSpdy2Test, DelayedSendChunkedPost) {
  set_merge_chunks(false);

  const char kUploadData1[] = "12345678";
  const int kUploadData1Size = arraysize(kUploadData1)-1;
  scoped_ptr<SpdyFrame> req(ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<SpdyFrame> chunk1(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> chunk2(
      ConstructSpdyBodyFrame(1, kUploadData1, kUploadData1Size, false));
  scoped_ptr<SpdyFrame> chunk3(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 0),
    CreateMockWrite(*chunk1, 1),  // POST upload frames
    CreateMockWrite(*chunk2, 2),
    CreateMockWrite(*chunk3, 3),
  };
  scoped_ptr<SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp, 4),
    CreateMockRead(*chunk1, 5),
    CreateMockRead(*chunk2, 6),
    CreateMockRead(*chunk3, 7),
    MockRead(ASYNC, 0, 8)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));

  DeterministicMockClientSocketFactory* socket_factory =
      session_deps_.deterministic_socket_factory.get();
  socket_factory->AddSocketDataProvider(&data);

  http_session_ = SpdySessionDependencies::SpdyCreateSessionDeterministic(
      &session_deps_);
  session_ = http_session_->spdy_session_pool()->Get(pair, BoundNetLog());
  transport_params_ = new TransportSocketParams(host_port_pair,
                                                MEDIUM, false, false,
                                                OnHostResolutionCallback());

  TestCompletionCallback callback;
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);

  EXPECT_EQ(ERR_IO_PENDING,
            connection->Init(host_port_pair.ToString(),
                             transport_params_,
                             MEDIUM,
                             callback.callback(),
                             http_session_->GetTransportSocketPool(
                                 HttpNetworkSession::NORMAL_SOCKET_POOL),
                             BoundNetLog()));

  callback.WaitForResult();
  EXPECT_EQ(OK,
            session_->InitializeWithSocket(connection.release(), false, OK));

  scoped_refptr<UploadData> upload_data(new UploadData());
  upload_data->set_is_chunked(true);
  UploadDataStream upload_stream(upload_data);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_stream;

  ASSERT_EQ(OK, upload_stream.InitSync());
  upload_data->AppendChunk(kUploadData, kUploadDataSize, false);

  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(
      new SpdyHttpStream(session_.get(), true));
  ASSERT_EQ(OK, http_stream->InitializeStream(&request,
                                              net_log,
                                              CompletionCallback()));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(pair));

  // Complete the initial request write and the first chunk.
  data.RunFor(2);
  ASSERT_TRUE(callback.have_result());
  EXPECT_GT(callback.WaitForResult(), 0);

  // Now append the final two chunks which will enqueue two more writes.
  upload_data->AppendChunk(kUploadData1, kUploadData1Size, false);
  upload_data->AppendChunk(kUploadData, kUploadDataSize, true);

  // Finish writing all the chunks.
  data.RunFor(2);

  // Read response headers.
  data.RunFor(1);
  ASSERT_EQ(OK, http_stream->ReadResponseHeaders(callback.callback()));

  // Read and check |chunk1| response.
  data.RunFor(1);
  scoped_refptr<IOBuffer> buf1(new IOBuffer(kUploadDataSize));
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(buf1,
                                          kUploadDataSize,
                                          callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf1->data(), kUploadDataSize));

  // Read and check |chunk2| response.
  data.RunFor(1);
  scoped_refptr<IOBuffer> buf2(new IOBuffer(kUploadData1Size));
  ASSERT_EQ(kUploadData1Size,
            http_stream->ReadResponseBody(buf2,
                                          kUploadData1Size,
                                          callback.callback()));
  EXPECT_EQ(kUploadData1, std::string(buf2->data(), kUploadData1Size));

  // Read and check |chunk3| response.
  data.RunFor(1);
  scoped_refptr<IOBuffer> buf3(new IOBuffer(kUploadDataSize));
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(buf3,
                                          kUploadDataSize,
                                          callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf3->data(), kUploadDataSize));

  // Finish reading the |EOF|.
  data.RunFor(1);
  ASSERT_TRUE(response.headers.get());
  ASSERT_EQ(200, response.headers->response_code());
  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
}

// Test case for bug: http://code.google.com/p/chromium/issues/detail?id=50058
TEST_F(SpdyHttpStreamSpdy2Test, SpdyURLTest) {
  const char * const full_url = "http://www.google.com/foo?query=what#anchor";
  const char * const base_url = "http://www.google.com/foo?query=what";
  scoped_ptr<SpdyFrame> req(ConstructSpdyGet(base_url, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 1),
  };
  scoped_ptr<SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(SYNCHRONOUS, 0, 3)  // EOF
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
      http_stream->InitializeStream(&request, net_log, CompletionCallback()));

  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));

  const SpdyHeaderBlock& spdy_header =
    http_stream->stream()->spdy_headers();
  if (spdy_header.find("url") != spdy_header.end())
    EXPECT_EQ("/foo?query=what", spdy_header.find("url")->second);
  else
    FAIL() << "No url is set in spdy_header!";

  // This triggers the MockWrite and read 2
  callback.WaitForResult();

  // This triggers read 3. The empty read causes the session to shut down.
  data()->CompleteRead();

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_FALSE(http_session_->spdy_session_pool()->HasSession(pair));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

// TODO(willchan): Write a longer test for SpdyStream that exercises all
// methods.

}  // namespace net
