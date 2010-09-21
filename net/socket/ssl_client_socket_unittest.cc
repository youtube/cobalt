// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

const net::SSLConfig kDefaultSSLConfig;

class SSLClientSocketTest : public PlatformTest {
 public:
  SSLClientSocketTest()
      : resolver_(net::CreateSystemHostResolver(
            net::HostResolver::kDefaultParallelism,
            NULL)),
        socket_factory_(net::ClientSocketFactory::GetDefaultFactory()) {
  }

  // PerformHTTPSRequest makes an HTTPS request to the testserver by requesting
  // the given path (i.e. "/"). If |response| is non-NULL then any resulting
  // bytes (including any HTTP headers etc) are appended to |response|.
  void PerformHTTPSRequest(const std::string& path, std::string* response);

 protected:
  scoped_refptr<net::HostResolver> resolver_;
  net::ClientSocketFactory* socket_factory_;
};

//-----------------------------------------------------------------------------

TEST_F(SSLClientSocketTest, Connect) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS, FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  net::CapturingNetLog log(net::CapturingNetLog::kUnbounded);
  net::ClientSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(transport,
          test_server.host_port_pair().host(), kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      log.entries(), 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);
    EXPECT_FALSE(sock->IsConnected());
    EXPECT_FALSE(net::LogContainsEndEvent(
        log.entries(), -1, net::NetLog::TYPE_SSL_CONNECT));

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }

  EXPECT_TRUE(sock->IsConnected());
  EXPECT_TRUE(net::LogContainsEndEvent(
      log.entries(), -1, net::NetLog::TYPE_SSL_CONNECT));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

TEST_F(SSLClientSocketTest, ConnectExpired) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS_EXPIRED_CERTIFICATE,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  net::CapturingNetLog log(net::CapturingNetLog::kUnbounded);
  net::ClientSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(transport,
          test_server.host_port_pair().host(), kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      log.entries(), 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);
    EXPECT_FALSE(sock->IsConnected());
    EXPECT_FALSE(net::LogContainsEndEvent(
        log.entries(), -1, net::NetLog::TYPE_SSL_CONNECT));

    rv = callback.WaitForResult();
    EXPECT_EQ(net::ERR_CERT_DATE_INVALID, rv);
  }

  // We cannot test sock->IsConnected(), as the NSS implementation disconnects
  // the socket when it encounters an error, whereas other implementations
  // leave it connected.

  EXPECT_TRUE(net::LogContainsEndEvent(
      log.entries(), -1, net::NetLog::TYPE_SSL_CONNECT));
}

TEST_F(SSLClientSocketTest, ConnectMismatched) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS_MISMATCHED_HOSTNAME,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  net::CapturingNetLog log(net::CapturingNetLog::kUnbounded);
  net::ClientSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(transport,
          test_server.host_port_pair().host(), kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);

  EXPECT_TRUE(net::LogContainsBeginEvent(
      log.entries(), 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv != net::ERR_CERT_COMMON_NAME_INVALID) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);
    EXPECT_FALSE(sock->IsConnected());
    EXPECT_FALSE(net::LogContainsEndEvent(
        log.entries(), -1, net::NetLog::TYPE_SSL_CONNECT));

    rv = callback.WaitForResult();
    EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID, rv);
  }

  // We cannot test sock->IsConnected(), as the NSS implementation disconnects
  // the socket when it encounters an error, whereas other implementations
  // leave it connected.

  EXPECT_TRUE(net::LogContainsEndEvent(
      log.entries(), -1, net::NetLog::TYPE_SSL_CONNECT));
}

// Attempt to connect to a page which requests a client certificate. It should
// return an error code on connect.
TEST_F(SSLClientSocketTest, FLAKY_ConnectClientAuthCertRequested) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS_CLIENT_AUTH,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  net::CapturingNetLog log(net::CapturingNetLog::kUnbounded);
  net::ClientSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(transport,
          test_server.host_port_pair().host(), kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      log.entries(), 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv != net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);
    EXPECT_FALSE(sock->IsConnected());
    EXPECT_FALSE(net::LogContainsEndEvent(
        log.entries(), -1, net::NetLog::TYPE_SSL_CONNECT));

    rv = callback.WaitForResult();
    EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED, rv);
  }

  // We cannot test sock->IsConnected(), as the NSS implementation disconnects
  // the socket when it encounters an error, whereas other implementations
  // leave it connected.

  EXPECT_TRUE(net::LogContainsEndEvent(
      log.entries(), -1, net::NetLog::TYPE_SSL_CONNECT));
}

// Connect to a server requesting optional client authentication. Send it a
// null certificate. It should allow the connection.
//
// TODO(davidben): Also test providing an actual certificate.
TEST_F(SSLClientSocketTest, ConnectClientAuthSendNullCert) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS_CLIENT_AUTH,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  net::CapturingNetLog log(net::CapturingNetLog::kUnbounded);
  net::ClientSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  net::SSLConfig ssl_config = kDefaultSSLConfig;
  ssl_config.send_client_cert = true;
  ssl_config.client_cert = NULL;

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(transport,
          test_server.host_port_pair().host(), ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  // Our test server accepts certificate-less connections.
  // TODO(davidben): Add a test which requires them and verify the error.
  rv = sock->Connect(&callback);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      log.entries(), 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);
    EXPECT_FALSE(sock->IsConnected());
    EXPECT_FALSE(net::LogContainsEndEvent(
        log.entries(), -1, net::NetLog::TYPE_SSL_CONNECT));

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }

  EXPECT_TRUE(sock->IsConnected());
  EXPECT_TRUE(net::LogContainsEndEvent(
      log.entries(), -1, net::NetLog::TYPE_SSL_CONNECT));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

// TODO(wtc): Add unit tests for IsConnectedAndIdle:
//   - Server closes an SSL connection (with a close_notify alert message).
//   - Server closes the underlying TCP connection directly.
//   - Server sends data unexpectedly.

void SSLClientSocketTest::PerformHTTPSRequest(const std::string& path,
                                              std::string* response) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS, FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  net::ClientSocket* transport = new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source());
  int rv = transport->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(transport,
                                             test_server.host_port_pair().host(),
                                             kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }
  EXPECT_TRUE(sock->IsConnected());

  const std::string request_text = "GET " + path + " HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer =
      new net::IOBuffer(request_text.size());
  memcpy(request_buffer->data(), request_text.data(), request_text.size());

  rv = sock->Write(request_buffer, request_text.size(), &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(request_text.size()), rv);

  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(4096);
  for (;;) {
    rv = sock->Read(buf, 4096, &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
    if (response)
      *response += std::string(buf->data(), rv);
  }
}

TEST_F(SSLClientSocketTest, Read) {
  PerformHTTPSRequest("/", NULL /* don't care about reply contents */);
}

// Test the full duplex mode, with Read and Write pending at the same time.
// This test also serves as a regression test for http://crbug.com/29815.
TEST_F(SSLClientSocketTest, Read_FullDuplex) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS, FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;  // Used for everything except Write.
  TestCompletionCallback callback2;  // Used for Write only.

  net::ClientSocket* transport = new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source());
  int rv = transport->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(transport,
                                             test_server.host_port_pair().host(),
                                             kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }
  EXPECT_TRUE(sock->IsConnected());

  // Issue a "hanging" Read first.
  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(4096);
  rv = sock->Read(buf, 4096, &callback);
  // We haven't written the request, so there should be no response yet.
  ASSERT_EQ(net::ERR_IO_PENDING, rv);

  // Write the request.
  // The request is padded with a User-Agent header to a size that causes the
  // memio circular buffer (4k bytes) in SSLClientSocketNSS to wrap around.
  // This tests the fix for http://crbug.com/29815.
  std::string request_text = "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  for (int i = 0; i < 3800; ++i)
    request_text.push_back('*');
  request_text.append("\r\n\r\n");
  scoped_refptr<net::IOBuffer> request_buffer =
      new net::StringIOBuffer(request_text);

  rv = sock->Write(request_buffer, request_text.size(), &callback2);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback2.WaitForResult();
  EXPECT_EQ(static_cast<int>(request_text.size()), rv);

  // Now get the Read result.
  rv = callback.WaitForResult();
  EXPECT_GT(rv, 0);
}

TEST_F(SSLClientSocketTest, Read_SmallChunks) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS, FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  net::ClientSocket* transport = new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source());
  int rv = transport->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(transport,
          test_server.host_port_pair().host(), kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer =
      new net::IOBuffer(arraysize(request_text) - 1);
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(1);
  for (;;) {
    rv = sock->Read(buf, 1, &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

TEST_F(SSLClientSocketTest, Read_Interrupted) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS, FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  net::ClientSocket* transport = new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source());
  int rv = transport->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(transport,
          test_server.host_port_pair().host(), kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv != net::OK) {
    ASSERT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);
  }

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer =
      new net::IOBuffer(arraysize(request_text) - 1);
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  // Do a partial read and then exit.  This test should not crash!
  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(512);
  rv = sock->Read(buf, 512, &callback);
  EXPECT_TRUE(rv > 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_GT(rv, 0);
}

// Regression test for http://crbug.com/42538
TEST_F(SSLClientSocketTest, PrematureApplicationData) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS, FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  TestCompletionCallback callback;

  static const unsigned char application_data[] = {
    0x17, 0x03, 0x01, 0x00, 0x4a, 0x02, 0x00, 0x00, 0x46, 0x03, 0x01, 0x4b,
    0xc2, 0xf8, 0xb2, 0xc1, 0x56, 0x42, 0xb9, 0x57, 0x7f, 0xde, 0x87, 0x46,
    0xf7, 0xa3, 0x52, 0x42, 0x21, 0xf0, 0x13, 0x1c, 0x9c, 0x83, 0x88, 0xd6,
    0x93, 0x0c, 0xf6, 0x36, 0x30, 0x05, 0x7e, 0x20, 0xb5, 0xb5, 0x73, 0x36,
    0x53, 0x83, 0x0a, 0xfc, 0x17, 0x63, 0xbf, 0xa0, 0xe4, 0x42, 0x90, 0x0d,
    0x2f, 0x18, 0x6d, 0x20, 0xd8, 0x36, 0x3f, 0xfc, 0xe6, 0x01, 0xfa, 0x0f,
    0xa5, 0x75, 0x7f, 0x09, 0x00, 0x04, 0x00, 0x16, 0x03, 0x01, 0x11, 0x57,
    0x0b, 0x00, 0x11, 0x53, 0x00, 0x11, 0x50, 0x00, 0x06, 0x22, 0x30, 0x82,
    0x06, 0x1e, 0x30, 0x82, 0x05, 0x06, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02,
    0x0a
  };

  // All reads and writes complete synchronously (async=false).
  net::MockRead data_reads[] = {
    net::MockRead(false, reinterpret_cast<const char*>(application_data),
                  arraysize(application_data)),
    net::MockRead(false, net::OK),
  };

  net::StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                     NULL, 0);

  net::ClientSocket* transport =
      new net::MockTCPClientSocket(addr, NULL, &data);
  int rv = transport->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(
          transport, test_server.host_port_pair().host(), kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  EXPECT_EQ(net::ERR_SSL_PROTOCOL_ERROR, rv);
}

// CorkedFalseStart tries to test that, in a full handshake, an application
// data record is contained in the same packet as the Finished handshake
// message. This test is inheriently false-negative-flaky: if everything is
// good it'll always pass but, if things break, it'll non-deterministicly fail.
//
// WARNING: do not mark this test as flaky. See above.
TEST_F(SSLClientSocketTest, CorkedFalseStart) {
  std::string response;
  PerformHTTPSRequest("/corked-false-start", &response);
  // "979bdf01cb3c" is a random string which is included in the response to
  // make it easy to grep for.
  ASSERT_TRUE(std::string::npos != response.find("979bdf01cb3c"));
}
