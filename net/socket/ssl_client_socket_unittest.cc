// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

#include "net/base/address_list.h"
#include "net/base/cert_verifier.h"
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
      : socket_factory_(net::ClientSocketFactory::GetDefaultFactory()),
        cert_verifier_(new net::CertVerifier) {
  }

 protected:
  net::SSLClientSocket* CreateSSLClientSocket(
      net::ClientSocket* transport_socket,
      const net::HostPortPair& host_and_port,
      const net::SSLConfig& ssl_config) {
    return socket_factory_->CreateSSLClientSocket(transport_socket,
                                                  host_and_port,
                                                  ssl_config,
                                                  NULL,
                                                  cert_verifier_.get());
  }

  net::ClientSocketFactory* socket_factory_;
  scoped_ptr<net::CertVerifier> cert_verifier_;
};

//-----------------------------------------------------------------------------

// LogContainsSSLConnectEndEvent returns true if the given index in the given
// log is an SSL connect end event. The NSS sockets will cork in an attempt to
// merge the first application data record with the Finished message when false
// starting. However, in order to avoid the server timing out the handshake,
// they'll give up waiting for application data and send the Finished after a
// timeout. This means that an SSL connect end event may appear as a socket
// write.
static bool LogContainsSSLConnectEndEvent(
    const net::CapturingNetLog::EntryList& log, int i) {
  return net::LogContainsEndEvent(log, i, net::NetLog::TYPE_SSL_CONNECT) ||
         net::LogContainsEvent(log, i, net::NetLog::TYPE_SOCKET_BYTES_SENT,
                                net::NetLog::PHASE_NONE);
};

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
      socket_factory_->CreateSSLClientSocket(
          transport, test_server.host_port_pair(), kDefaultSSLConfig,
          NULL, cert_verifier_.get()));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

TEST_F(SSLClientSocketTest, ConnectExpired) {
  net::TestServer::HTTPSOptions https_options(
      net::TestServer::HTTPSOptions::CERT_EXPIRED);
  net::TestServer test_server(https_options, FilePath());
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
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(net::ERR_CERT_DATE_INVALID, rv);

  // Rather than testing whether or not the underlying socket is connected,
  // test that the handshake has finished. This is because it may be
  // desirable to disconnect the socket before showing a user prompt, since
  // the user may take indefinitely long to respond.
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));
}

TEST_F(SSLClientSocketTest, ConnectMismatched) {
  net::TestServer::HTTPSOptions https_options(
      net::TestServer::HTTPSOptions::CERT_MISMATCHED_NAME);
  net::TestServer test_server(https_options, FilePath());
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
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID, rv);

  // Rather than testing whether or not the underlying socket is connected,
  // test that the handshake has finished. This is because it may be
  // desirable to disconnect the socket before showing a user prompt, since
  // the user may take indefinitely long to respond.
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));
}

// Attempt to connect to a page which requests a client certificate. It should
// return an error code on connect.
TEST_F(SSLClientSocketTest, ConnectClientAuthCertRequested) {
  net::TestServer::HTTPSOptions https_options;
  https_options.request_client_certificate = true;
  net::TestServer test_server(https_options, FilePath());
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
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  log.GetEntries(&entries);
  // Because we prematurely kill the handshake at CertificateRequest,
  // the server may still send data (notably the ServerHelloDone)
  // after the error is returned. As a result, the SSL_CONNECT may not
  // be the last entry. See http://crbug.com/54445. We use
  // ExpectLogContainsSomewhere instead of
  // LogContainsSSLConnectEndEvent to avoid assuming, e.g., only one
  // extra read instead of two. This occurs before the handshake ends,
  // so the corking logic of LogContainsSSLConnectEndEvent isn't
  // necessary.
  //
  // TODO(davidben): When SSL_RestartHandshakeAfterCertReq in NSS is
  // fixed and we can respond to the first CertificateRequest
  // without closing the socket, add a unit test for sending the
  // certificate. This test may still be useful as we'll want to close
  // the socket on a timeout if the user takes a long time to pick a
  // cert. Related bug: https://bugzilla.mozilla.org/show_bug.cgi?id=542832
  net::ExpectLogContainsSomewhere(
      entries, 0, net::NetLog::TYPE_SSL_CONNECT, net::NetLog::PHASE_END);
  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED, rv);
  EXPECT_FALSE(sock->IsConnected());
}

// Connect to a server requesting optional client authentication. Send it a
// null certificate. It should allow the connection.
//
// TODO(davidben): Also test providing an actual certificate.
TEST_F(SSLClientSocketTest, ConnectClientAuthSendNullCert) {
  net::TestServer::HTTPSOptions https_options;
  https_options.request_client_certificate = true;
  net::TestServer test_server(https_options, FilePath());
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
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  // Our test server accepts certificate-less connections.
  // TODO(davidben): Add a test which requires them and verify the error.
  rv = sock->Connect(&callback);

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

// TODO(wtc): Add unit tests for IsConnectedAndIdle:
//   - Server closes an SSL connection (with a close_notify alert message).
//   - Server closes the underlying TCP connection directly.
//   - Server sends data unexpectedly.

TEST_F(SSLClientSocketTest, Read) {
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
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer(
      new net::IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(4096));
  for (;;) {
    rv = sock->Read(buf, 4096, &callback);
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
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
      socket_factory_->CreateSSLClientSocket(
          transport, test_server.host_port_pair(), kDefaultSSLConfig,
          NULL, cert_verifier_.get()));

  rv = sock->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  // Issue a "hanging" Read first.
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(4096));
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
  scoped_refptr<net::IOBuffer> request_buffer(
      new net::StringIOBuffer(request_text));

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
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer(
      new net::IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(1));
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
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer(
      new net::IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  // Do a partial read and then exit.  This test should not crash!
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(512));
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
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = sock->Connect(&callback);
  EXPECT_EQ(net::ERR_SSL_PROTOCOL_ERROR, rv);
}

// TODO(rsleevi): Not implemented for Schannel. As Schannel is only used when
// performing client authentication, it will not be tested here.
TEST_F(SSLClientSocketTest, CipherSuiteDisables) {
  // Rather than exhaustively disabling every RC4 ciphersuite defined at
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml,
  // only disabling those cipher suites that the test server actually
  // implements.
  const uint16 kCiphersToDisable[] = {
    0x0005,  // TLS_RSA_WITH_RC4_128_SHA
  };

  net::TestServer::HTTPSOptions https_options;
  // Enable only RC4 on the test server.
  https_options.bulk_ciphers =
      net::TestServer::HTTPSOptions::BULK_CIPHER_RC4;
  net::TestServer test_server(https_options, FilePath());
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

  net::SSLConfig ssl_config;
  for (size_t i = 0; i < arraysize(kCiphersToDisable); ++i)
    ssl_config.disabled_cipher_suites.push_back(kCiphersToDisable[i]);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(&callback);
  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));

  // NSS has special handling that maps a handshake_failure alert received
  // immediately after a client_hello to be a mismatched cipher suite error,
  // leading to ERR_SSL_VERSION_OR_CIPHER_MISMATCH. When using OpenSSL or
  // Secure Transport (OS X), the handshake_failure is bubbled up without any
  // interpretation, leading to ERR_SSL_PROTOCOL_ERROR. Either way, a failure
  // indicates that no cipher suite was negotiated with the test server.
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_TRUE(rv == net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH ||
              rv == net::ERR_SSL_PROTOCOL_ERROR);
  // The exact ordering differs between SSLClientSocketNSS (which issues an
  // extra read) and SSLClientSocketMac (which does not). Just make sure the
  // error appears somewhere in the log.
  log.GetEntries(&entries);
  net::ExpectLogContainsSomewhere(entries, 0,
                                  net::NetLog::TYPE_SSL_HANDSHAKE_ERROR,
                                  net::NetLog::PHASE_NONE);

  // We cannot test sock->IsConnected(), as the NSS implementation disconnects
  // the socket when it encounters an error, whereas other implementations
  // leave it connected.
  // Because this an error that the test server is mutually aware of, as opposed
  // to being an error such as a certificate name mismatch, which is
  // client-only, the exact index of the SSL connect end depends on how
  // quickly the test server closes the underlying socket. If the test server
  // closes before the IO message loop pumps messages, there may be a 0-byte
  // Read event in the NetLog due to TCPClientSocket picking up the EOF. As a
  // result, the SSL connect end event will be the second-to-last entry,
  // rather than the last entry.
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1) ||
              LogContainsSSLConnectEndEvent(entries, -2));
}
