// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_proxy_client_socket.h"

#include "base/utf_string_conversions.h"
#include "net/base/address_list.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "net/http/http_response_info.h"
#include "net/http/http_response_headers.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util.h"
#include "testing/platform_test.h"
#include "testing/gtest/include/gtest/gtest.h"

//-----------------------------------------------------------------------------

namespace {

static const char kUrl[] = "https://www.google.com/";
static const char kOriginHost[] = "www.google.com";
static const int kOriginPort = 443;
static const char kOriginHostPort[] = "www.google.com:443";
static const char kProxyUrl[] = "https://myproxy:6121/";
static const char kProxyHost[] = "myproxy";
static const int kProxyPort = 6121;
static const char kUserAgent[] = "Mozilla/1.0";

static const int kStreamId = 1;

static const char kMsg1[] = "\0hello!\xff";
static const int kLen1 = 8;
static const char kMsg2[] = "\00012345678\0";
static const int kLen2 = 10;
static const char kMsg3[] = "bye!";
static const int kLen3 = 4;
static const char kMsg33[] = "bye!bye!";
static const int kLen33 = kLen3 + kLen3;
static const char kMsg333[] = "bye!bye!bye!";
static const int kLen333 = kLen3 + kLen3 + kLen3;

}  // anonymous namespace

namespace net {

class SpdyProxyClientSocketTest : public PlatformTest {
 public:
  SpdyProxyClientSocketTest();

  virtual void TearDown();

 protected:
  void Initialize(MockRead* reads, size_t reads_count, MockWrite* writes,
                  size_t writes_count);
  spdy::SpdyFrame* ConstructConnectRequestFrame();
  spdy::SpdyFrame* ConstructConnectAuthRequestFrame();
  spdy::SpdyFrame* ConstructConnectReplyFrame();
  spdy::SpdyFrame* ConstructConnectAuthReplyFrame();
  spdy::SpdyFrame* ConstructNtlmAuthReplyFrame();
  spdy::SpdyFrame* ConstructConnectErrorReplyFrame();
  spdy::SpdyFrame* ConstructBodyFrame(const char* data, int length);
  scoped_refptr<IOBufferWithSize> CreateBuffer(const char* data, int size);
  void AssertConnectSucceeds();
  void AssertConnectFails(int result);
  void AssertConnectionEstablished();
  void AssertSyncReadEquals(const char* data, int len);
  void AssertAsyncReadEquals(const char* data, int len);
  void AssertReadStarts(const char* data, int len);
  void AssertReadReturns(const char* data, int len);
  void AssertAsyncWriteSucceeds(const char* data, int len);
  void AssertWriteReturns(const char* data, int len, int rv);
  void AssertWriteLength(int len);
  void AssertAsyncWriteWithReadsSucceeds(const char* data, int len,
                                        int num_reads);

  void AddAuthToCache() {
    const string16 kFoo(ASCIIToUTF16("foo"));
    const string16 kBar(ASCIIToUTF16("bar"));
    session_->http_auth_cache()->Add(GURL(kProxyUrl),
                                     "MyRealm1",
                                     HttpAuth::AUTH_SCHEME_BASIC,
                                     "Basic realm=MyRealm1",
                                     AuthCredentials(kFoo, kBar),
                                     "/");
  }

  void Run(int steps) {
    data_->StopAfter(steps);
    data_->Run();
  }

  scoped_ptr<SpdyProxyClientSocket> sock_;
  TestOldCompletionCallback read_callback_;
  TestOldCompletionCallback write_callback_;
  scoped_refptr<DeterministicSocketData> data_;

 private:
  scoped_refptr<HttpNetworkSession> session_;
  scoped_refptr<IOBuffer> read_buf_;
  SpdySessionDependencies session_deps_;
  MockConnect connect_data_;
  scoped_refptr<SpdySession> spdy_session_;
  scoped_refptr<SpdyStream> spdy_stream_;
  spdy::SpdyFramer framer_;

  std::string user_agent_;
  GURL url_;
  HostPortPair proxy_host_port_;
  HostPortPair endpoint_host_port_pair_;
  ProxyServer proxy_;
  HostPortProxyPair endpoint_host_port_proxy_pair_;
  scoped_refptr<TransportSocketParams> transport_params_;

  DISALLOW_COPY_AND_ASSIGN(SpdyProxyClientSocketTest);
};

SpdyProxyClientSocketTest::SpdyProxyClientSocketTest()
    : sock_(NULL),
      read_callback_(),
      write_callback_(),
      data_(NULL),
      session_(NULL),
      read_buf_(NULL),
      session_deps_(),
      connect_data_(false, OK),
      spdy_session_(NULL),
      spdy_stream_(NULL),
      framer_(),
      user_agent_(kUserAgent),
      url_(kUrl),
      proxy_host_port_(kProxyHost, kProxyPort),
      endpoint_host_port_pair_(kOriginHost, kOriginPort),
      proxy_(ProxyServer::SCHEME_HTTPS, proxy_host_port_),
      endpoint_host_port_proxy_pair_(endpoint_host_port_pair_, proxy_),
      transport_params_(new TransportSocketParams(proxy_host_port_,
                                            LOWEST,
                                            false,
                                            false)) {
}

void SpdyProxyClientSocketTest::TearDown() {
  if (session_ != NULL)
    session_->spdy_session_pool()->CloseAllSessions();

  spdy::SpdyFramer::set_enable_compression_default(true);
  // Empty the current queue.
  MessageLoop::current()->RunAllPending();
  PlatformTest::TearDown();
}

void SpdyProxyClientSocketTest::Initialize(MockRead* reads,
                                           size_t reads_count,
                                           MockWrite* writes,
                                           size_t writes_count) {
  data_ = new DeterministicSocketData(reads, reads_count, writes, writes_count);
  data_->set_connect_data(connect_data_);
  data_->SetStop(2);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(
      data_.get());
  session_deps_.host_resolver->set_synchronous_mode(true);

  session_ = SpdySessionDependencies::SpdyCreateSessionDeterministic(
      &session_deps_);
  SpdySession::SetSSLMode(false);
  spdy::SpdyFramer::set_enable_compression_default(false);

  // Creates a new spdy session
  spdy_session_ =
      session_->spdy_session_pool()->Get(endpoint_host_port_proxy_pair_,
                                         BoundNetLog());

  // Perform the TCP connect
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK,
            connection->Init(endpoint_host_port_pair_.ToString(),
                             transport_params_,
                             LOWEST, NULL, session_->GetTransportSocketPool(),
                             BoundNetLog()));
  spdy_session_->InitializeWithSocket(connection.release(), false, OK);

  // Create the SPDY Stream
  ASSERT_EQ(
      OK,
      spdy_session_->CreateStream(url_, LOWEST, &spdy_stream_, BoundNetLog(),
                                  NULL));

  // Create the SpdyProxyClientSocket
  sock_.reset(
      new SpdyProxyClientSocket(spdy_stream_, user_agent_,
                                endpoint_host_port_pair_, url_,
                                proxy_host_port_, session_->http_auth_cache(),
                                session_->http_auth_handler_factory()));
}

scoped_refptr<IOBufferWithSize> SpdyProxyClientSocketTest::CreateBuffer(
    const char* data, int size) {
  scoped_refptr<IOBufferWithSize> buf(new IOBufferWithSize(size));
  memcpy(buf->data(), data, size);
  return buf;
}

void SpdyProxyClientSocketTest::AssertConnectSucceeds() {
  ASSERT_EQ(ERR_IO_PENDING, sock_->Connect(&read_callback_));
  data_->Run();
  ASSERT_EQ(OK, read_callback_.WaitForResult());
}

void SpdyProxyClientSocketTest::AssertConnectFails(int result) {
  ASSERT_EQ(ERR_IO_PENDING, sock_->Connect(&read_callback_));
  data_->Run();
  ASSERT_EQ(result, read_callback_.WaitForResult());
}

void SpdyProxyClientSocketTest::AssertConnectionEstablished() {
  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_EQ(200, response->headers->response_code());
  ASSERT_EQ("Connection Established", response->headers->GetStatusText());
}

void SpdyProxyClientSocketTest::AssertSyncReadEquals(const char* data,
                                                     int len) {
  scoped_refptr<IOBuffer> buf(new IOBuffer(len));
  ASSERT_EQ(len, sock_->Read(buf, len, NULL));
  ASSERT_EQ(std::string(data, len), std::string(buf->data(), len));
  ASSERT_TRUE(sock_->IsConnected());
}

void SpdyProxyClientSocketTest::AssertAsyncReadEquals(const char* data,
                                                     int len) {
  data_->StopAfter(1);
  // Issue the read, which will be completed asynchronously
  scoped_refptr<IOBuffer> buf(new IOBuffer(len));
  ASSERT_EQ(ERR_IO_PENDING, sock_->Read(buf, len, &read_callback_));
  EXPECT_TRUE(sock_->IsConnected());
  data_->Run();

  EXPECT_TRUE(sock_->IsConnected());

  // Now the read will return
  EXPECT_EQ(len, read_callback_.WaitForResult());
  ASSERT_EQ(std::string(data, len), std::string(buf->data(), len));
}

void SpdyProxyClientSocketTest::AssertReadStarts(const char* data, int len) {
  data_->StopAfter(1);
  // Issue the read, which will be completed asynchronously
  read_buf_ = new IOBuffer(len);
  ASSERT_EQ(ERR_IO_PENDING, sock_->Read(read_buf_, len, &read_callback_));
  EXPECT_TRUE(sock_->IsConnected());
}

void SpdyProxyClientSocketTest::AssertReadReturns(const char* data, int len) {
  EXPECT_TRUE(sock_->IsConnected());

  // Now the read will return
  EXPECT_EQ(len, read_callback_.WaitForResult());
  ASSERT_EQ(std::string(data, len), std::string(read_buf_->data(), len));
}

void SpdyProxyClientSocketTest::AssertAsyncWriteSucceeds(const char* data,
                                                        int len) {
  AssertWriteReturns(data, len, ERR_IO_PENDING);
  data_->RunFor(1);
  AssertWriteLength(len);
}

void SpdyProxyClientSocketTest::AssertWriteReturns(const char* data, int len,
                                                   int rv) {
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(data, len));
  EXPECT_EQ(rv, sock_->Write(buf, buf->size(), &write_callback_));
}

void SpdyProxyClientSocketTest::AssertWriteLength(int len) {
  EXPECT_EQ(len, write_callback_.WaitForResult());
}

void SpdyProxyClientSocketTest::AssertAsyncWriteWithReadsSucceeds(
    const char* data, int len, int num_reads) {
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(data, len));

  EXPECT_EQ(ERR_IO_PENDING, sock_->Write(buf, buf->size(), &write_callback_));

  for (int i = 0; i < num_reads; i++) {
    Run(1);
    AssertSyncReadEquals(kMsg2, kLen2);
  }

  write_callback_.WaitForResult();
}

// Constructs a standard SPDY SYN_STREAM frame for a CONNECT request.
spdy::SpdyFrame* SpdyProxyClientSocketTest::ConstructConnectRequestFrame() {
  const SpdyHeaderInfo kSynStartHeader = {
    spdy::SYN_STREAM,
    kStreamId,
    0,
    net::ConvertRequestPriorityToSpdyPriority(LOWEST),
    spdy::CONTROL_FLAG_NONE,
    false,
    spdy::INVALID,
    NULL,
    0,
    spdy::DATA_FLAG_NONE
  };
  const char* const kConnectHeaders[] = {
    "method", "CONNECT",
    "url", kOriginHostPort,
    "host", kOriginHost,
    "user-agent", kUserAgent,
    "version", "HTTP/1.1",
  };
  return ConstructSpdyPacket(
      kSynStartHeader, NULL, 0, kConnectHeaders, arraysize(kConnectHeaders)/2);
}

// Constructs a SPDY SYN_STREAM frame for a CONNECT request which includes
// Proxy-Authorization headers.
spdy::SpdyFrame* SpdyProxyClientSocketTest::ConstructConnectAuthRequestFrame() {
  const SpdyHeaderInfo kSynStartHeader = {
    spdy::SYN_STREAM,
    kStreamId,
    0,
    net::ConvertRequestPriorityToSpdyPriority(LOWEST),
    spdy::CONTROL_FLAG_NONE,
    false,
    spdy::INVALID,
    NULL,
    0,
    spdy::DATA_FLAG_NONE
  };
  const char* const kConnectHeaders[] = {
    "method", "CONNECT",
    "url", kOriginHostPort,
    "host", kOriginHost,
    "user-agent", kUserAgent,
    "version", "HTTP/1.1",
    "proxy-authorization", "Basic Zm9vOmJhcg==",
  };
  return ConstructSpdyPacket(
      kSynStartHeader, NULL, 0, kConnectHeaders, arraysize(kConnectHeaders)/2);
}

// Constructs a standard SPDY SYN_REPLY frame to match the SPDY CONNECT.
spdy::SpdyFrame* SpdyProxyClientSocketTest::ConstructConnectReplyFrame() {
  const char* const kStandardReplyHeaders[] = {
      "status", "200 Connection Established",
      "version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(NULL,
                                   0,
                                   false,
                                   kStreamId,
                                   LOWEST,
                                   spdy::SYN_REPLY,
                                   spdy::CONTROL_FLAG_NONE,
                                   kStandardReplyHeaders,
                                   arraysize(kStandardReplyHeaders));
}

// Constructs a standard SPDY SYN_REPLY frame to match the SPDY CONNECT.
spdy::SpdyFrame* SpdyProxyClientSocketTest::ConstructConnectAuthReplyFrame() {
  const char* const kStandardReplyHeaders[] = {
      "status", "407 Proxy Authentication Required",
      "version", "HTTP/1.1",
      "proxy-authenticate", "Basic realm=\"MyRealm1\"",
  };

  return ConstructSpdyControlFrame(NULL,
                                   0,
                                   false,
                                   kStreamId,
                                   LOWEST,
                                   spdy::SYN_REPLY,
                                   spdy::CONTROL_FLAG_NONE,
                                   kStandardReplyHeaders,
                                   arraysize(kStandardReplyHeaders));
}

// Constructs a SPDY SYN_REPLY frame to match the SPDY CONNECT which
// requires Proxy Authentication using NTLM.
spdy::SpdyFrame* SpdyProxyClientSocketTest::ConstructNtlmAuthReplyFrame() {
  const char* const kStandardReplyHeaders[] = {
      "status", "407 Proxy Authentication Required",
      "version", "HTTP/1.1",
      "proxy-authenticate", "NTLM",
  };

  return ConstructSpdyControlFrame(NULL,
                                   0,
                                   false,
                                   kStreamId,
                                   LOWEST,
                                   spdy::SYN_REPLY,
                                   spdy::CONTROL_FLAG_NONE,
                                   kStandardReplyHeaders,
                                   arraysize(kStandardReplyHeaders));
}

// Constructs a SPDY SYN_REPLY frame with an HTTP 500 error.
spdy::SpdyFrame* SpdyProxyClientSocketTest::ConstructConnectErrorReplyFrame() {
  const char* const kStandardReplyHeaders[] = {
      "status", "500 Internal Server Error",
      "version", "HTTP/1.1",
  };

  return ConstructSpdyControlFrame(NULL,
                                   0,
                                   false,
                                   kStreamId,
                                   LOWEST,
                                   spdy::SYN_REPLY,
                                   spdy::CONTROL_FLAG_NONE,
                                   kStandardReplyHeaders,
                                   arraysize(kStandardReplyHeaders));
}

spdy::SpdyFrame* SpdyProxyClientSocketTest::ConstructBodyFrame(const char* data,
                                                               int length) {
  return framer_.CreateDataFrame(kStreamId, data, length, spdy::DATA_FLAG_NONE);
}

// ----------- Connect

TEST_F(SpdyProxyClientSocketTest, ConnectSendsCorrectRequest) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectSucceeds();

  AssertConnectionEstablished();
}

TEST_F(SpdyProxyClientSocketTest, ConnectWithUnsupportedAuth) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructNtlmAuthReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_TUNNEL_CONNECTION_FAILED);
}

TEST_F(SpdyProxyClientSocketTest, ConnectWithAuthRequested) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectAuthReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_EQ(407, response->headers->response_code());
  ASSERT_EQ("Proxy Authentication Required",
            response->headers->GetStatusText());
}

TEST_F(SpdyProxyClientSocketTest, ConnectWithAuthCredentials) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectAuthRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));
  AddAuthToCache();

  AssertConnectSucceeds();

  AssertConnectionEstablished();
}

TEST_F(SpdyProxyClientSocketTest, ConnectWithAuthRestart) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  scoped_ptr<spdy::SpdyFrame> auth(ConstructConnectAuthRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectAuthReplyFrame());
  scoped_ptr<spdy::SpdyFrame> auth_resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_EQ(407, response->headers->response_code());
  ASSERT_EQ("Proxy Authentication Required",
            response->headers->GetStatusText());

  AddAuthToCache();

  ASSERT_EQ(OK, sock_->RestartWithAuth(&read_callback_));
  // A SpdyProxyClientSocket sits on a single SPDY stream which can
  // only be used for a single request/response.
  ASSERT_FALSE(sock_->IsConnectedAndIdle());
}

TEST_F(SpdyProxyClientSocketTest, ConnectFails) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    MockRead(true, 0, 1),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectFails(ERR_CONNECTION_CLOSED);

  ASSERT_FALSE(sock_->IsConnected());
}

// ----------- WasEverUsed

TEST_F(SpdyProxyClientSocketTest, WasEverUsedReturnsCorrectValues) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  EXPECT_FALSE(sock_->WasEverUsed());
  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->WasEverUsed());
  sock_->Disconnect();
  EXPECT_TRUE(sock_->WasEverUsed());
}

// ----------- GetPeerAddress

TEST_F(SpdyProxyClientSocketTest, GetPeerAddressReturnsCorrectValues) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  net::AddressList addr;
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, sock_->GetPeerAddress(&addr));

  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_EQ(OK, sock_->GetPeerAddress(&addr));

  sock_->Disconnect();
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, sock_->GetPeerAddress(&addr));
}

// ----------- Write

TEST_F(SpdyProxyClientSocketTest, WriteSendsDataInDataFrame) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<spdy::SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
    CreateMockWrite(*msg1, 2, false),
    CreateMockWrite(*msg2, 3, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  AssertAsyncWriteSucceeds(kMsg1, kLen1);
  AssertAsyncWriteSucceeds(kMsg2, kLen2);
}

TEST_F(SpdyProxyClientSocketTest, WriteSplitsLargeDataIntoMultipleFrames) {
  std::string chunk_data(kMaxSpdyFrameChunkSize, 'x');
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  scoped_ptr<spdy::SpdyFrame> chunk(ConstructBodyFrame(chunk_data.data(),
                                                       chunk_data.length()));
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
    CreateMockWrite(*chunk, 2, false),
    CreateMockWrite(*chunk, 3, false),
    CreateMockWrite(*chunk, 4, false)
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 5),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  std::string big_data(kMaxSpdyFrameChunkSize * 3, 'x');
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(big_data.data(),
                                                   big_data.length()));

  EXPECT_EQ(ERR_IO_PENDING, sock_->Write(buf, buf->size(), &write_callback_));
  data_->RunFor(3);

  EXPECT_EQ(buf->size(), write_callback_.WaitForResult());
}

// ----------- Read

TEST_F(SpdyProxyClientSocketTest, ReadReadsDataInDataFrame) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg1, 2, true),
    MockRead(true, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);  // SpdySession consumes the next read and sends it to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
}

TEST_F(SpdyProxyClientSocketTest, ReadDataFromBufferedFrames) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<spdy::SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg1, 2, true),
    CreateMockRead(*msg2, 3, true),
    MockRead(true, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);  // SpdySession consumes the next read and sends it to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
  Run(1);  // SpdySession consumes the next read and sends it to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_F(SpdyProxyClientSocketTest, ReadDataMultipleBufferedFrames) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<spdy::SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg1, 2, true),
    CreateMockRead(*msg2, 3, true),
    MockRead(true, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(2);  // SpdySession consumes the next two reads and sends then to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_F(SpdyProxyClientSocketTest, LargeReadWillMergeDataFromDifferentFrames) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<spdy::SpdyFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg3, 2, true),
    CreateMockRead(*msg3, 3, true),
    MockRead(true, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(2);  // SpdySession consumes the next two reads and sends then to
           // sock_ to be buffered.
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33, kLen33);
}

TEST_F(SpdyProxyClientSocketTest, MultipleShortReadsThenMoreRead) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<spdy::SpdyFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  scoped_ptr<spdy::SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg1, 2, true),
    CreateMockRead(*msg3, 3, true),
    CreateMockRead(*msg3, 4, true),
    CreateMockRead(*msg2, 5, true),
    MockRead(true, 0, 6),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(4);  // SpdySession consumes the next four reads and sends then to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33, kLen33);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_F(SpdyProxyClientSocketTest, ReadWillSplitDataFromLargeFrame) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<spdy::SpdyFrame> msg33(ConstructBodyFrame(kMsg33, kLen33));
  scoped_ptr<spdy::SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg1, 2, true),
    CreateMockRead(*msg33, 3, true),
    MockRead(true, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(2);  // SpdySession consumes the next two reads and sends then to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg3, kLen3);
}

TEST_F(SpdyProxyClientSocketTest, MultipleReadsFromSameLargeFrame) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg333(ConstructBodyFrame(kMsg333, kLen333));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg333, 2, true),
    MockRead(true, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);  // SpdySession consumes the next read and sends it to
           // sock_ to be buffered.
  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg33, kLen33);

  // Now attempt to do a read of more data than remains buffered
  scoped_refptr<IOBuffer> buf(new IOBuffer(kLen33));
  ASSERT_EQ(kLen3, sock_->Read(buf, kLen33, &read_callback_));
  ASSERT_EQ(std::string(kMsg3, kLen3), std::string(buf->data(), kLen3));
  ASSERT_TRUE(sock_->IsConnected());
}

TEST_F(SpdyProxyClientSocketTest, ReadAuthResponseBody) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectAuthReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<spdy::SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg1, 2, true),
    CreateMockRead(*msg2, 3, true),
    MockRead(true, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  Run(2);  // SpdySession consumes the next two reads and sends then to
           // sock_ to be buffered.
  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_F(SpdyProxyClientSocketTest, ReadErrorResponseBody) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectErrorReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<spdy::SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg1, 2, true),
    CreateMockRead(*msg2, 3, true),
    MockRead(true, 0, 4),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectFails(ERR_HTTPS_PROXY_TUNNEL_RESPONSE);

  Run(2);  // SpdySession consumes the next two reads and sends then to
           // sock_ to be buffered.
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, sock_->Read(NULL, 1, NULL));
  scoped_refptr<IOBuffer> buf(new IOBuffer(kLen1 + kLen2));
  scoped_ptr<HttpStream> stream(sock_->CreateConnectResponseStream());
  stream->ReadResponseBody(buf, kLen1 + kLen2, &read_callback_);
}

// ----------- Reads and Writes

TEST_F(SpdyProxyClientSocketTest, AsyncReadAroundWrite) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  scoped_ptr<spdy::SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
    CreateMockWrite(*msg2, 3, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<spdy::SpdyFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg1, 2, true),  // sync read
    CreateMockRead(*msg3, 4, true),  // async read
    MockRead(true, 0, 5),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);
  AssertSyncReadEquals(kMsg1, kLen1);

  AssertReadStarts(kMsg3, kLen3);
  // Read should block until after the write succeeds

  AssertAsyncWriteSucceeds(kMsg2, kLen2);  // Runs 1 step

  ASSERT_FALSE(read_callback_.have_result());
  Run(1);
  // Now the read will return
  AssertReadReturns(kMsg3, kLen3);
}

TEST_F(SpdyProxyClientSocketTest, AsyncWriteAroundReads) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  scoped_ptr<spdy::SpdyFrame> msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
    CreateMockWrite(*msg2, 4, true),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  scoped_ptr<spdy::SpdyFrame> msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg1, 2, true),
    CreateMockRead(*msg3, 3, true),
    MockRead(true, 0, 5),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);
  AssertSyncReadEquals(kMsg1, kLen1);
  // Write should block until the read completes
  AssertWriteReturns(kMsg2, kLen2, ERR_IO_PENDING);

  AssertAsyncReadEquals(kMsg3, kLen3);

  ASSERT_FALSE(write_callback_.have_result());

  // Now the write will complete
  Run(1);
  AssertWriteLength(kLen2);
}

// ----------- Reading/Writing on Closed socket

// Reading from an already closed socket should return 0
TEST_F(SpdyProxyClientSocketTest, ReadOnClosedSocketReturnsZero) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);

  ASSERT_EQ(0, sock_->Read(NULL, 1, NULL));
  ASSERT_EQ(ERR_CONNECTION_CLOSED, sock_->Read(NULL, 1, NULL));
  ASSERT_EQ(ERR_CONNECTION_CLOSED, sock_->Read(NULL, 1, NULL));
  ASSERT_FALSE(sock_->IsConnectedAndIdle());
}

// Read pending when socket is closed should return 0
TEST_F(SpdyProxyClientSocketTest, PendingReadOnCloseReturnsZero) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  AssertReadStarts(kMsg1, kLen1);

  Run(1);

  ASSERT_EQ(0, read_callback_.WaitForResult());
}

// Reading from a disconnected socket is an error
TEST_F(SpdyProxyClientSocketTest, ReadOnDisconnectSocketReturnsNotConnected) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  sock_->Disconnect();

  ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED, sock_->Read(NULL, 1, NULL));
}

// Reading buffered data from an already closed socket should return
// buffered data, then 0.
TEST_F(SpdyProxyClientSocketTest, ReadOnClosedSocketReturnsBufferedData) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*msg1, 2, true),
    MockRead(true, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(2);

  AssertSyncReadEquals(kMsg1, kLen1);
  ASSERT_EQ(0, sock_->Read(NULL, 1, NULL));
  ASSERT_EQ(ERR_CONNECTION_CLOSED, sock_->Read(NULL, 1, NULL));
  // Verify that read *still* returns ERR_CONNECTION_CLOSED
  ASSERT_EQ(ERR_CONNECTION_CLOSED, sock_->Read(NULL, 1, NULL));
}

// Calling Write() on a closed socket is an error
TEST_F(SpdyProxyClientSocketTest, WriteOnClosedStream) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  Run(1);  // Read EOF which will close the stream
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_CONNECTION_CLOSED, sock_->Write(buf, buf->size(), NULL));
}

// Calling Write() on a disconnected socket is an error
TEST_F(SpdyProxyClientSocketTest, WriteOnDisconnectedSocket) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  sock_->Disconnect();

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED, sock_->Write(buf, buf->size(), NULL));
}

// If the socket is closed with a pending Write(), the callback
// should be called with ERR_CONNECTION_CLOSED.
TEST_F(SpdyProxyClientSocketTest, WritePendingOnClose) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
    MockWrite(true, ERR_IO_PENDING, 2),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING, sock_->Write(buf, buf->size(), &write_callback_));

  Run(1);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, write_callback_.WaitForResult());
}

// If the socket is Disconnected with a pending Write(), the callback
// should not be called.
TEST_F(SpdyProxyClientSocketTest, DisconnectWithWritePending) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
    MockWrite(false, 0, 2),  // EOF
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 3),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING, sock_->Write(buf, buf->size(), &write_callback_));

  sock_->Disconnect();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(write_callback_.have_result());
}

// If the socket is Disconnected with a pending Read(), the callback
// should not be called.
TEST_F(SpdyProxyClientSocketTest, DisconnectWithReadPending) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBuffer> buf(new IOBuffer(kLen1));
  ASSERT_EQ(ERR_IO_PENDING, sock_->Read(buf, kLen1, &read_callback_));

  sock_->Disconnect();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(read_callback_.have_result());
}

// If the socket is Reset when both a read and write are pending,
// both should be called back.
TEST_F(SpdyProxyClientSocketTest, RstWithReadAndWritePending) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
    MockWrite(true, ERR_IO_PENDING, 2),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> rst(ConstructSpdyRstStream(1, spdy::CANCEL));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*rst, 3, true),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBuffer> read_buf(new IOBuffer(kLen1));
  ASSERT_EQ(ERR_IO_PENDING, sock_->Read(read_buf, kLen1, &read_callback_));

  scoped_refptr<IOBufferWithSize> write_buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING, sock_->Write(write_buf, write_buf->size(),
                                         &write_callback_));

  Run(2);

  EXPECT_TRUE(sock_.get());
  EXPECT_TRUE(read_callback_.have_result());
  EXPECT_TRUE(write_callback_.have_result());
}

// CompletionCallback that causes the SpdyProxyClientSocket to be
// deleted when Run is invoked.
class DeleteSockCallback : public TestOldCompletionCallback {
 public:
  explicit DeleteSockCallback(scoped_ptr<SpdyProxyClientSocket>* sock)
    : sock_(sock) {
  }

  virtual ~DeleteSockCallback() {
  }

  virtual void RunWithParams(const Tuple1<int>& params) OVERRIDE {
    sock_->reset(NULL);
    TestOldCompletionCallback::RunWithParams(params);
  }

 private:
  scoped_ptr<SpdyProxyClientSocket>* sock_;
};

// If the socket is Reset when both a read and write are pending, and the
// read callback causes the socket to be deleted, the write callback should
// not be called.
TEST_F(SpdyProxyClientSocketTest, RstWithReadAndWritePendingDelete) {
  scoped_ptr<spdy::SpdyFrame> conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
    CreateMockWrite(*conn, 0, false),
    MockWrite(true, ERR_IO_PENDING, 2),
  };

  scoped_ptr<spdy::SpdyFrame> resp(ConstructConnectReplyFrame());
  scoped_ptr<spdy::SpdyFrame> rst(ConstructSpdyRstStream(1, spdy::CANCEL));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, true),
    CreateMockRead(*rst, 3, true),
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  DeleteSockCallback read_callback(&sock_);

  scoped_refptr<IOBuffer> read_buf(new IOBuffer(kLen1));
  ASSERT_EQ(ERR_IO_PENDING, sock_->Read(read_buf, kLen1, &read_callback));

  scoped_refptr<IOBufferWithSize> write_buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING, sock_->Write(write_buf, write_buf->size(),
                                         &write_callback_));

  Run(2);

  EXPECT_FALSE(sock_.get());
  EXPECT_TRUE(read_callback.have_result());
  EXPECT_FALSE(write_callback_.have_result());
}

}  // namespace net
