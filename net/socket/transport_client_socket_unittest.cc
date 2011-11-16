// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket.h"

#include "base/basictypes.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/listen_socket.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "net/socket/client_socket_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

const char kServerReply[] = "HTTP/1.1 404 Not Found";

enum ClientSocketTestTypes {
  TCP,
  SCTP
};

class TransportClientSocketTest
    : public ListenSocket::ListenSocketDelegate,
      public ::testing::TestWithParam<ClientSocketTestTypes> {
 public:
  TransportClientSocketTest()
      : listen_port_(0),
        net_log_(CapturingNetLog::kUnbounded),
        socket_factory_(ClientSocketFactory::GetDefaultFactory()),
        close_server_socket_on_next_send_(false) {
  }

  ~TransportClientSocketTest() {
  }

  // Implement ListenSocketDelegate methods
  virtual void DidAccept(ListenSocket* server, ListenSocket* connection) {
    connected_sock_ = connection;
  }
  virtual void DidRead(ListenSocket*, const char* str, int len) {
    // TODO(dkegel): this might not be long enough to tickle some bugs.
    connected_sock_->Send(kServerReply, arraysize(kServerReply) - 1,
                          false /* Don't append line feed */);
    if (close_server_socket_on_next_send_)
      CloseServerSocket();
  }
  virtual void DidClose(ListenSocket* sock) {}

  // Testcase hooks
  virtual void SetUp();

  void CloseServerSocket() {
    // delete the connected_sock_, which will close it.
    connected_sock_ = NULL;
  }

  void PauseServerReads() {
    connected_sock_->PauseReads();
  }

  void ResumeServerReads() {
    connected_sock_->ResumeReads();
  }

  int DrainClientSocket(IOBuffer* buf,
                        uint32 buf_len,
                        uint32 bytes_to_read,
                        TestOldCompletionCallback* callback);

  void SendClientRequest();

  void set_close_server_socket_on_next_send(bool close) {
    close_server_socket_on_next_send_ = close;
  }

 protected:
  int listen_port_;
  CapturingNetLog net_log_;
  ClientSocketFactory* const socket_factory_;
  scoped_ptr<StreamSocket> sock_;

 private:
  scoped_refptr<ListenSocket> listen_sock_;
  scoped_refptr<ListenSocket> connected_sock_;
  bool close_server_socket_on_next_send_;
};

void TransportClientSocketTest::SetUp() {
  ::testing::TestWithParam<ClientSocketTestTypes>::SetUp();

  // Find a free port to listen on
  ListenSocket *sock = NULL;
  int port;
  // Range of ports to listen on.  Shouldn't need to try many.
  const int kMinPort = 10100;
  const int kMaxPort = 10200;
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif
  for (port = kMinPort; port < kMaxPort; port++) {
    sock = ListenSocket::Listen("127.0.0.1", port, this);
    if (sock)
      break;
  }
  ASSERT_TRUE(sock != NULL);
  listen_sock_ = sock;
  listen_port_ = port;

  AddressList addr;
  scoped_ptr<HostResolver> resolver(
      CreateSystemHostResolver(HostResolver::kDefaultParallelism,
                               HostResolver::kDefaultRetryAttempts,
                               NULL));
  HostResolver::RequestInfo info(HostPortPair("localhost", listen_port_));
  TestCompletionCallback callback;
  int rv = resolver->Resolve(info, &addr, callback.callback(), NULL,
                             BoundNetLog());
  CHECK_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  CHECK_EQ(rv, OK);
  sock_.reset(
      socket_factory_->CreateTransportClientSocket(addr,
                                                   &net_log_,
                                                   NetLog::Source()));
}

int TransportClientSocketTest::DrainClientSocket(
    IOBuffer* buf, uint32 buf_len,
    uint32 bytes_to_read, TestOldCompletionCallback* callback) {
  int rv = OK;
  uint32 bytes_read = 0;

  while (bytes_read < bytes_to_read) {
    rv = sock_->Read(buf, buf_len, callback);
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      rv = callback->WaitForResult();

    EXPECT_GE(rv, 0);
    bytes_read += rv;
  }

  return static_cast<int>(bytes_read);
}

void TransportClientSocketTest::SendClientRequest() {
  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer(
      new IOBuffer(arraysize(request_text) - 1));
  TestOldCompletionCallback callback;
  int rv;

  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);
  rv = sock_->Write(request_buffer, arraysize(request_text) - 1, &callback);
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(rv, static_cast<int>(arraysize(request_text) - 1));
}

// TODO(leighton):  Add SCTP to this list when it is ready.
INSTANTIATE_TEST_CASE_P(StreamSocket,
                        TransportClientSocketTest,
                        ::testing::Values(TCP));

TEST_P(TransportClientSocketTest, Connect) {
  TestOldCompletionCallback callback;
  EXPECT_FALSE(sock_->IsConnected());

  int rv = sock_->Connect(&callback);

  net::CapturingNetLog::EntryList net_log_entries;
  net_log_.GetEntries(&net_log_entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      net_log_entries, 0, net::NetLog::TYPE_SOCKET_ALIVE));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      net_log_entries, 1, net::NetLog::TYPE_TCP_CONNECT));
  if (rv != OK) {
    ASSERT_EQ(rv, ERR_IO_PENDING);
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
  }

  EXPECT_TRUE(sock_->IsConnected());
  net_log_.GetEntries(&net_log_entries);
  EXPECT_TRUE(net::LogContainsEndEvent(
      net_log_entries, -1, net::NetLog::TYPE_TCP_CONNECT));

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());
}

TEST_P(TransportClientSocketTest, IsConnected) {
  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  TestOldCompletionCallback callback;
  uint32 bytes_read;

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(sock_->IsConnectedAndIdle());
  int rv = sock_->Connect(&callback);
  if (rv != OK) {
    ASSERT_EQ(rv, ERR_IO_PENDING);
    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
  }
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_TRUE(sock_->IsConnectedAndIdle());

  // Send the request and wait for the server to respond.
  SendClientRequest();

  // Drain a single byte so we know we've received some data.
  bytes_read = DrainClientSocket(buf, 1, 1, &callback);
  ASSERT_EQ(bytes_read, 1u);

  // Socket should be considered connected, but not idle, due to
  // pending data.
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_FALSE(sock_->IsConnectedAndIdle());

  bytes_read = DrainClientSocket(buf, 4096, arraysize(kServerReply) - 2,
                                 &callback);
  ASSERT_EQ(bytes_read, arraysize(kServerReply) - 2);

  // After draining the data, the socket should be back to connected
  // and idle.
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_TRUE(sock_->IsConnectedAndIdle());

  // This time close the server socket immediately after the server response.
  set_close_server_socket_on_next_send(true);
  SendClientRequest();

  bytes_read = DrainClientSocket(buf, 1, 1, &callback);
  ASSERT_EQ(bytes_read, 1u);

  // As above because of data.
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_FALSE(sock_->IsConnectedAndIdle());

  bytes_read = DrainClientSocket(buf, 4096, arraysize(kServerReply) - 2,
                                 &callback);
  ASSERT_EQ(bytes_read, arraysize(kServerReply) - 2);

  // Once the data is drained, the socket should now be seen as
  // closed.
  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(sock_->IsConnectedAndIdle());
}

TEST_P(TransportClientSocketTest, Read) {
  TestOldCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != OK) {
    ASSERT_EQ(rv, ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
  }
  SendClientRequest();

  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  uint32 bytes_read = DrainClientSocket(buf, 4096, arraysize(kServerReply) - 1,
                                        &callback);
  ASSERT_EQ(bytes_read, arraysize(kServerReply) - 1);

  // All data has been read now.  Read once more to force an ERR_IO_PENDING, and
  // then close the server socket, and note the close.

  rv = sock_->Read(buf, 4096, &callback);
  ASSERT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(static_cast<int64>(std::string(kServerReply).size()),
            sock_->NumBytesRead());
  CloseServerSocket();
  EXPECT_EQ(0, callback.WaitForResult());
}

TEST_P(TransportClientSocketTest, Read_SmallChunks) {
  TestOldCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != OK) {
    ASSERT_EQ(rv, ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
  }
  SendClientRequest();

  scoped_refptr<IOBuffer> buf(new IOBuffer(1));
  uint32 bytes_read = 0;
  while (bytes_read < arraysize(kServerReply) - 1) {
    rv = sock_->Read(buf, 1, &callback);
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();

    ASSERT_EQ(1, rv);
    bytes_read += rv;
  }

  // All data has been read now.  Read once more to force an ERR_IO_PENDING, and
  // then close the server socket, and note the close.

  rv = sock_->Read(buf, 1, &callback);
  EXPECT_EQ(static_cast<int64>(std::string(kServerReply).size()),
            sock_->NumBytesRead());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  CloseServerSocket();
  EXPECT_EQ(0, callback.WaitForResult());
}

TEST_P(TransportClientSocketTest, Read_Interrupted) {
  TestOldCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != OK) {
    ASSERT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
  }
  SendClientRequest();

  // Do a partial read and then exit.  This test should not crash!
  scoped_refptr<IOBuffer> buf(new IOBuffer(16));
  rv = sock_->Read(buf, 16, &callback);
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);
  EXPECT_EQ(0, sock_->NumBytesRead());

  if (rv == ERR_IO_PENDING) {
    rv = callback.WaitForResult();
    EXPECT_EQ(16, sock_->NumBytesRead());
  }

  EXPECT_NE(0, rv);
}

TEST_P(TransportClientSocketTest, DISABLED_FullDuplex_ReadFirst) {
  TestOldCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != OK) {
    ASSERT_EQ(rv, ERR_IO_PENDING);

    rv = callback.WaitForResult();
    EXPECT_EQ(rv, OK);
  }

  // Read first.  There's no data, so it should return ERR_IO_PENDING.
  const int kBufLen = 4096;
  scoped_refptr<IOBuffer> buf(new IOBuffer(kBufLen));
  rv = sock_->Read(buf, kBufLen, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  PauseServerReads();
  const int kWriteBufLen = 64 * 1024;
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kWriteBufLen));
  char* request_data = request_buffer->data();
  memset(request_data, 'A', kWriteBufLen);
  TestOldCompletionCallback write_callback;

  while (true) {
    rv = sock_->Write(request_buffer, kWriteBufLen, &write_callback);
    ASSERT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING) {
      ResumeServerReads();
      rv = write_callback.WaitForResult();
      break;
    }
  }

  // At this point, both read and write have returned ERR_IO_PENDING, and the
  // write callback has executed.  We wait for the read callback to run now to
  // make sure that the socket can handle full duplex communications.

  rv = callback.WaitForResult();
  EXPECT_GE(rv, 0);
}

TEST_P(TransportClientSocketTest, DISABLED_FullDuplex_WriteFirst) {
  TestOldCompletionCallback callback;
  int rv = sock_->Connect(&callback);
  if (rv != OK) {
    ASSERT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);
  }

  PauseServerReads();
  const int kWriteBufLen = 64 * 1024;
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kWriteBufLen));
  char* request_data = request_buffer->data();
  memset(request_data, 'A', kWriteBufLen);
  TestOldCompletionCallback write_callback;

  while (true) {
    rv = sock_->Write(request_buffer, kWriteBufLen, &write_callback);
    ASSERT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      break;
  }

  // Now we have the Write() blocked on ERR_IO_PENDING.  It's time to force the
  // Read() to block on ERR_IO_PENDING too.

  const int kBufLen = 4096;
  scoped_refptr<IOBuffer> buf(new IOBuffer(kBufLen));
  while (true) {
    rv = sock_->Read(buf, kBufLen, &callback);
    ASSERT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);
    if (rv == ERR_IO_PENDING)
      break;
  }

  // At this point, both read and write have returned ERR_IO_PENDING.  Now we
  // run the write and read callbacks to make sure they can handle full duplex
  // communications.

  ResumeServerReads();
  rv = write_callback.WaitForResult();
  EXPECT_GE(rv, 0);

  // It's possible the read is blocked because it's already read all the data.
  // Close the server socket, so there will at least be a 0-byte read.
  CloseServerSocket();

  rv = callback.WaitForResult();
  EXPECT_GE(rv, 0);
}

}  // namespace

}  // namespace net
