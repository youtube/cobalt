// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test suite uses SSLClientSocket to test the implementation of
// SSLServerSocket. In order to establish connections between the sockets
// we need two additional classes:
// 1. FakeSocket
//    Connects SSL socket to FakeDataChannel. This class is just a stub.
//
// 2. FakeDataChannel
//    Implements the actual exchange of data between two FakeSockets.
//
// Implementations of these two classes are included in this file.

#include "net/socket/ssl_server_socket.h"

#include <stdlib.h>

#include <queue>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "crypto/nss_util.h"
#include "crypto/rsa_private_key.h"
#include "net/base/address_list.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/ssl_config_service.h"
#include "net/base/ssl_info.h"
#include "net/base/x509_certificate.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

class FakeDataChannel {
 public:
  FakeDataChannel()
      : read_callback_(NULL),
        read_buf_len_(0),
        ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
  }

  virtual int Read(IOBuffer* buf, int buf_len,
                   OldCompletionCallback* callback) {
    if (data_.empty()) {
      read_callback_ = callback;
      read_buf_ = buf;
      read_buf_len_ = buf_len;
      return net::ERR_IO_PENDING;
    }
    return PropogateData(buf, buf_len);
  }

  virtual int Write(IOBuffer* buf, int buf_len,
                    OldCompletionCallback* callback) {
    data_.push(new net::DrainableIOBuffer(buf, buf_len));
    MessageLoop::current()->PostTask(
        FROM_HERE, task_factory_.NewRunnableMethod(
            &FakeDataChannel::DoReadCallback));
    return buf_len;
  }

 private:
  void DoReadCallback() {
    if (!read_callback_ || data_.empty())
      return;

    int copied = PropogateData(read_buf_, read_buf_len_);
    net::OldCompletionCallback* callback = read_callback_;
    read_callback_ = NULL;
    read_buf_ = NULL;
    read_buf_len_ = 0;
    callback->Run(copied);
  }

  int PropogateData(scoped_refptr<net::IOBuffer> read_buf, int read_buf_len) {
    scoped_refptr<net::DrainableIOBuffer> buf = data_.front();
    int copied = std::min(buf->BytesRemaining(), read_buf_len);
    memcpy(read_buf->data(), buf->data(), copied);
    buf->DidConsume(copied);

    if (!buf->BytesRemaining())
      data_.pop();
    return copied;
  }

  net::OldCompletionCallback* read_callback_;
  scoped_refptr<net::IOBuffer> read_buf_;
  int read_buf_len_;

  std::queue<scoped_refptr<net::DrainableIOBuffer> > data_;

  ScopedRunnableMethodFactory<FakeDataChannel> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeDataChannel);
};

class FakeSocket : public StreamSocket {
 public:
  FakeSocket(FakeDataChannel* incoming_channel,
             FakeDataChannel* outgoing_channel)
      : incoming_(incoming_channel),
        outgoing_(outgoing_channel) {
  }

  virtual ~FakeSocket() {
  }

  virtual int Read(IOBuffer* buf, int buf_len,
                   OldCompletionCallback* callback) {
    // Read random number of bytes.
    buf_len = rand() % buf_len + 1;
    return incoming_->Read(buf, buf_len, callback);
  }

  virtual int Write(IOBuffer* buf, int buf_len,
                    OldCompletionCallback* callback) {
    // Write random number of bytes.
    buf_len = rand() % buf_len + 1;
    return outgoing_->Write(buf, buf_len, callback);
  }

  virtual bool SetReceiveBufferSize(int32 size) {
    return true;
  }

  virtual bool SetSendBufferSize(int32 size) {
    return true;
  }

  virtual int Connect(OldCompletionCallback* callback) {
    return net::OK;
  }

  virtual void Disconnect() {}

  virtual bool IsConnected() const {
    return true;
  }

  virtual bool IsConnectedAndIdle() const {
    return true;
  }

  virtual int GetPeerAddress(AddressList* address) const {
    net::IPAddressNumber ip_address(4);
    *address = net::AddressList::CreateFromIPAddress(ip_address, 0 /*port*/);
    return net::OK;
  }

  virtual int GetLocalAddress(IPEndPoint* address) const {
    net::IPAddressNumber ip_address(4);
    *address = net::IPEndPoint(ip_address, 0);
    return net::OK;
  }

  virtual const BoundNetLog& NetLog() const {
    return net_log_;
  }

  virtual void SetSubresourceSpeculation() {}
  virtual void SetOmniboxSpeculation() {}

  virtual bool WasEverUsed() const {
    return true;
  }

  virtual bool UsingTCPFastOpen() const {
    return false;
  }

  virtual int64 NumBytesRead() const {
    return -1;
  }

  virtual base::TimeDelta GetConnectTimeMicros() const {
    return base::TimeDelta::FromMicroseconds(-1);
  }

 private:
  net::BoundNetLog net_log_;
  FakeDataChannel* incoming_;
  FakeDataChannel* outgoing_;

  DISALLOW_COPY_AND_ASSIGN(FakeSocket);
};

}  // namespace

// Verify the correctness of the test helper classes first.
TEST(FakeSocketTest, DataTransfer) {
  // Establish channels between two sockets.
  FakeDataChannel channel_1;
  FakeDataChannel channel_2;
  FakeSocket client(&channel_1, &channel_2);
  FakeSocket server(&channel_2, &channel_1);

  const char kTestData[] = "testing123";
  const int kTestDataSize = strlen(kTestData);
  const int kReadBufSize = 1024;
  scoped_refptr<net::IOBuffer> write_buf = new net::StringIOBuffer(kTestData);
  scoped_refptr<net::IOBuffer> read_buf = new net::IOBuffer(kReadBufSize);

  // Write then read.
  int written = server.Write(write_buf, kTestDataSize, NULL);
  EXPECT_GT(written, 0);
  EXPECT_LE(written, kTestDataSize);

  int read = client.Read(read_buf, kReadBufSize, NULL);
  EXPECT_GT(read, 0);
  EXPECT_LE(read, written);
  EXPECT_EQ(0, memcmp(kTestData, read_buf->data(), read));

  // Read then write.
  TestOldCompletionCallback callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            server.Read(read_buf, kReadBufSize, &callback));

  written = client.Write(write_buf, kTestDataSize, NULL);
  EXPECT_GT(written, 0);
  EXPECT_LE(written, kTestDataSize);

  read = callback.WaitForResult();
  EXPECT_GT(read, 0);
  EXPECT_LE(read, written);
  EXPECT_EQ(0, memcmp(kTestData, read_buf->data(), read));
}

class SSLServerSocketTest : public PlatformTest {
 public:
  SSLServerSocketTest()
      : socket_factory_(net::ClientSocketFactory::GetDefaultFactory()) {
  }

 protected:
  void Initialize() {
    FakeSocket* fake_client_socket = new FakeSocket(&channel_1_, &channel_2_);
    FakeSocket* fake_server_socket = new FakeSocket(&channel_2_, &channel_1_);

    FilePath certs_dir;
    PathService::Get(base::DIR_SOURCE_ROOT, &certs_dir);
    certs_dir = certs_dir.AppendASCII("net");
    certs_dir = certs_dir.AppendASCII("data");
    certs_dir = certs_dir.AppendASCII("ssl");
    certs_dir = certs_dir.AppendASCII("certificates");

    FilePath cert_path = certs_dir.AppendASCII("unittest.selfsigned.der");
    std::string cert_der;
    ASSERT_TRUE(file_util::ReadFileToString(cert_path, &cert_der));

    scoped_refptr<net::X509Certificate> cert =
        X509Certificate::CreateFromBytes(cert_der.data(), cert_der.size());

    FilePath key_path = certs_dir.AppendASCII("unittest.key.bin");
    std::string key_string;
    ASSERT_TRUE(file_util::ReadFileToString(key_path, &key_string));
    std::vector<uint8> key_vector(
        reinterpret_cast<const uint8*>(key_string.data()),
        reinterpret_cast<const uint8*>(key_string.data() +
                                       key_string.length()));

    scoped_ptr<crypto::RSAPrivateKey> private_key(
        crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_vector));

    net::SSLConfig ssl_config;
    ssl_config.cached_info_enabled = false;
    ssl_config.false_start_enabled = false;
    ssl_config.origin_bound_certs_enabled = false;
    ssl_config.ssl3_enabled = true;
    ssl_config.tls1_enabled = true;

    // Certificate provided by the host doesn't need authority.
    net::SSLConfig::CertAndStatus cert_and_status;
    cert_and_status.cert_status = CERT_STATUS_AUTHORITY_INVALID;
    cert_and_status.der_cert = cert_der;
    ssl_config.allowed_bad_certs.push_back(cert_and_status);

    net::HostPortPair host_and_pair("unittest", 0);
    net::SSLClientSocketContext context;
    context.cert_verifier = &cert_verifier_;
    client_socket_.reset(
        socket_factory_->CreateSSLClientSocket(
            fake_client_socket, host_and_pair, ssl_config, NULL, context));
    server_socket_.reset(net::CreateSSLServerSocket(fake_server_socket,
                                                    cert, private_key.get(),
                                                    net::SSLConfig()));
  }

  FakeDataChannel channel_1_;
  FakeDataChannel channel_2_;
  scoped_ptr<net::SSLClientSocket> client_socket_;
  scoped_ptr<net::SSLServerSocket> server_socket_;
  net::ClientSocketFactory* socket_factory_;
  net::CertVerifier cert_verifier_;
};

// SSLServerSocket is only implemented using NSS.
#if defined(USE_NSS) || defined(OS_WIN) || defined(OS_MACOSX)

// This test only executes creation of client and server sockets. This is to
// test that creation of sockets doesn't crash and have minimal code to run
// under valgrind in order to help debugging memory problems.
TEST_F(SSLServerSocketTest, Initialize) {
  Initialize();
}

// This test executes Connect() on SSLClientSocket and Handshake() on
// SSLServerSocket to make sure handshaking between the two sockets is
// completed successfully.
TEST_F(SSLServerSocketTest, Handshake) {
  Initialize();

  TestOldCompletionCallback connect_callback;
  TestOldCompletionCallback handshake_callback;

  int server_ret = server_socket_->Handshake(&handshake_callback);
  EXPECT_TRUE(server_ret == net::OK || server_ret == net::ERR_IO_PENDING);

  int client_ret = client_socket_->Connect(&connect_callback);
  EXPECT_TRUE(client_ret == net::OK || client_ret == net::ERR_IO_PENDING);

  if (client_ret == net::ERR_IO_PENDING) {
    EXPECT_EQ(net::OK, connect_callback.WaitForResult());
  }
  if (server_ret == net::ERR_IO_PENDING) {
    EXPECT_EQ(net::OK, handshake_callback.WaitForResult());
  }

  // Make sure the cert status is expected.
  SSLInfo ssl_info;
  client_socket_->GetSSLInfo(&ssl_info);
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, ssl_info.cert_status);
}

TEST_F(SSLServerSocketTest, DataTransfer) {
  Initialize();

  TestOldCompletionCallback connect_callback;
  TestOldCompletionCallback handshake_callback;

  // Establish connection.
  int client_ret = client_socket_->Connect(&connect_callback);
  ASSERT_TRUE(client_ret == net::OK || client_ret == net::ERR_IO_PENDING);

  int server_ret = server_socket_->Handshake(&handshake_callback);
  ASSERT_TRUE(server_ret == net::OK || server_ret == net::ERR_IO_PENDING);

  client_ret = connect_callback.GetResult(client_ret);
  ASSERT_EQ(net::OK, client_ret);
  server_ret = handshake_callback.GetResult(server_ret);
  ASSERT_EQ(net::OK, server_ret);

  const int kReadBufSize = 1024;
  scoped_refptr<net::StringIOBuffer> write_buf =
      new net::StringIOBuffer("testing123");
  scoped_refptr<net::DrainableIOBuffer> read_buf =
      new net::DrainableIOBuffer(new net::IOBuffer(kReadBufSize),
                                 kReadBufSize);

  // Write then read.
  TestOldCompletionCallback write_callback;
  TestOldCompletionCallback read_callback;
  server_ret = server_socket_->Write(write_buf, write_buf->size(),
                                     &write_callback);
  EXPECT_TRUE(server_ret > 0 || server_ret == net::ERR_IO_PENDING);
  client_ret = client_socket_->Read(read_buf, read_buf->BytesRemaining(),
                                    &read_callback);
  EXPECT_TRUE(client_ret > 0 || client_ret == net::ERR_IO_PENDING);

  server_ret = write_callback.GetResult(server_ret);
  EXPECT_GT(server_ret, 0);
  client_ret = read_callback.GetResult(client_ret);
  ASSERT_GT(client_ret, 0);

  read_buf->DidConsume(client_ret);
  while (read_buf->BytesConsumed() < write_buf->size()) {
    client_ret = client_socket_->Read(read_buf, read_buf->BytesRemaining(),
                                      &read_callback);
    EXPECT_TRUE(client_ret > 0 || client_ret == net::ERR_IO_PENDING);
    client_ret = read_callback.GetResult(client_ret);
    ASSERT_GT(client_ret, 0);
    read_buf->DidConsume(client_ret);
  }
  EXPECT_EQ(write_buf->size(), read_buf->BytesConsumed());
  read_buf->SetOffset(0);
  EXPECT_EQ(0, memcmp(write_buf->data(), read_buf->data(), write_buf->size()));

  // Read then write.
  write_buf = new net::StringIOBuffer("hello123");
  server_ret = server_socket_->Read(read_buf, read_buf->BytesRemaining(),
                                    &read_callback);
  EXPECT_TRUE(server_ret > 0 || server_ret == net::ERR_IO_PENDING);
  client_ret = client_socket_->Write(write_buf, write_buf->size(),
                                     &write_callback);
  EXPECT_TRUE(client_ret > 0 || client_ret == net::ERR_IO_PENDING);

  server_ret = read_callback.GetResult(server_ret);
  ASSERT_GT(server_ret, 0);
  client_ret = write_callback.GetResult(client_ret);
  EXPECT_GT(client_ret, 0);

  read_buf->DidConsume(server_ret);
  while (read_buf->BytesConsumed() < write_buf->size()) {
    server_ret = server_socket_->Read(read_buf, read_buf->BytesRemaining(),
                                      &read_callback);
    EXPECT_TRUE(server_ret > 0 || server_ret == net::ERR_IO_PENDING);
    server_ret = read_callback.GetResult(server_ret);
    ASSERT_GT(server_ret, 0);
    read_buf->DidConsume(server_ret);
  }
  EXPECT_EQ(write_buf->size(), read_buf->BytesConsumed());
  read_buf->SetOffset(0);
  EXPECT_EQ(0, memcmp(write_buf->data(), read_buf->data(), write_buf->size()));
}

// This test executes ExportKeyingMaterial() on the client and server sockets,
// after connecting them, and verifies that the results match.
// This test will fail if False Start is enabled (see crbug.com/90208).
TEST_F(SSLServerSocketTest, ExportKeyingMaterial) {
  Initialize();

  TestOldCompletionCallback connect_callback;
  TestOldCompletionCallback handshake_callback;

  int client_ret = client_socket_->Connect(&connect_callback);
  ASSERT_TRUE(client_ret == net::OK || client_ret == net::ERR_IO_PENDING);

  int server_ret = server_socket_->Handshake(&handshake_callback);
  ASSERT_TRUE(server_ret == net::OK || server_ret == net::ERR_IO_PENDING);

  if (client_ret == net::ERR_IO_PENDING) {
    ASSERT_EQ(net::OK, connect_callback.WaitForResult());
  }
  if (server_ret == net::ERR_IO_PENDING) {
    ASSERT_EQ(net::OK, handshake_callback.WaitForResult());
  }

  const int kKeyingMaterialSize = 32;
  const char* kKeyingLabel = "EXPERIMENTAL-server-socket-test";
  const char* kKeyingContext = "";
  unsigned char server_out[kKeyingMaterialSize];
  int rv = server_socket_->ExportKeyingMaterial(kKeyingLabel, kKeyingContext,
                                                server_out, sizeof(server_out));
  ASSERT_EQ(rv, net::OK);

  unsigned char client_out[kKeyingMaterialSize];
  rv = client_socket_->ExportKeyingMaterial(kKeyingLabel, kKeyingContext,
                                            client_out, sizeof(client_out));
  ASSERT_EQ(rv, net::OK);
  EXPECT_TRUE(memcmp(server_out, client_out, sizeof(server_out)) == 0);

  const char* kKeyingLabelBad = "EXPERIMENTAL-server-socket-test-bad";
  unsigned char client_bad[kKeyingMaterialSize];
  rv = client_socket_->ExportKeyingMaterial(kKeyingLabelBad, kKeyingContext,
                                            client_bad, sizeof(client_bad));
  ASSERT_EQ(rv, net::OK);
  EXPECT_TRUE(memcmp(server_out, client_bad, sizeof(server_out)) != 0);
}
#endif

}  // namespace net
