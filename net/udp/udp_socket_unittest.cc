// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/udp/udp_client_socket.h"
#include "net/udp/udp_server_socket.h"

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_test_suite.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

class UDPSocketTest : public PlatformTest {
 public:
  UDPSocketTest()
      : buffer_(new IOBufferWithSize(kMaxRead)) {
  }

  // Blocks until data is read from the socket.
  std::string RecvFromSocket(UDPServerSocket* socket) {
    TestCompletionCallback callback;

    int rv = socket->RecvFrom(buffer_, kMaxRead, &recv_from_address_,
                              &callback);
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      return "";  // error!
    return std::string(buffer_->data(), rv);
  }

  // Loop until |msg| has been written to the socket or until an
  // error occurs.
  // If |address| is specified, then it is used for the destination
  // to send to. Otherwise, will send to the last socket this server
  // received from.
  int SendToSocket(UDPServerSocket* socket, std::string msg) {
    return SendToSocket(socket, msg, recv_from_address_);
  }

  int SendToSocket(UDPServerSocket* socket,
                   std::string msg,
                   const IPEndPoint& address) {
    TestCompletionCallback callback;

    int length = msg.length();
    scoped_refptr<StringIOBuffer> io_buffer(new StringIOBuffer(msg));
    scoped_refptr<DrainableIOBuffer> buffer(
        new DrainableIOBuffer(io_buffer, length));

    int bytes_sent = 0;
    while (buffer->BytesRemaining()) {
      int rv = socket->SendTo(buffer, buffer->BytesRemaining(),
                              address, &callback);
      if (rv == ERR_IO_PENDING)
        rv = callback.WaitForResult();
      if (rv <= 0)
        return bytes_sent > 0 ? bytes_sent : rv;
      bytes_sent += rv;
      buffer->DidConsume(rv);
    }
    return bytes_sent;
  }

  std::string ReadSocket(UDPClientSocket* socket) {
    TestCompletionCallback callback;

    int rv = socket->Read(buffer_, kMaxRead, &callback);
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      return "";  // error!
    return std::string(buffer_->data(), rv);
  }

  // Loop until |msg| has been written to the socket or until an
  // error occurs.
  int WriteSocket(UDPClientSocket* socket, std::string msg) {
    TestCompletionCallback callback;

    int length = msg.length();
    scoped_refptr<StringIOBuffer> io_buffer(new StringIOBuffer(msg));
    scoped_refptr<DrainableIOBuffer> buffer(
        new DrainableIOBuffer(io_buffer, length));

    int bytes_sent = 0;
    while (buffer->BytesRemaining()) {
      int rv = socket->Write(buffer, buffer->BytesRemaining(), &callback);
      if (rv == ERR_IO_PENDING)
        rv = callback.WaitForResult();
      if (rv <= 0)
        return bytes_sent > 0 ? bytes_sent : rv;
      bytes_sent += rv;
      buffer->DidConsume(rv);
    }
    return bytes_sent;
  }

 private:
  static const int kMaxRead = 1024;
  scoped_refptr<IOBufferWithSize> buffer_;
  IPEndPoint recv_from_address_;
};

// Creates and address from an ip/port and returns it in |address|.
void CreateUDPAddress(std::string ip_str, int port, IPEndPoint* address) {
  IPAddressNumber ip_number;
  bool rv = ParseIPLiteralToNumber(ip_str, &ip_number);
  if (!rv)
    return;
  *address = IPEndPoint(ip_number, port);
}

TEST_F(UDPSocketTest, Connect) {
  const int kPort = 9999;
  std::string simple_message("hello world!");

  // Setup the server to listen.
  IPEndPoint bind_address;
  CreateUDPAddress("0.0.0.0", kPort, &bind_address);
  UDPServerSocket server(NULL, NetLog::Source());
  int rv = server.Listen(bind_address);
  EXPECT_EQ(OK, rv);

  // Setup the client.
  IPEndPoint server_address;
  CreateUDPAddress("127.0.0.1", kPort, &server_address);
  UDPClientSocket client(NULL, NetLog::Source());
  rv = client.Connect(server_address);
  EXPECT_EQ(OK, rv);

  // Client sends to the server.
  rv = WriteSocket(&client, simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // Server waits for message.
  std::string str = RecvFromSocket(&server);
  DCHECK(simple_message == str);

  // Server echoes reply.
  rv = SendToSocket(&server, simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // Client waits for response.
  str = ReadSocket(&client);
  DCHECK(simple_message == str);
}

// In this test, we verify that connect() on a socket will have the effect
// of filtering reads on this socket only to data read from the destination
// we connected to.
//
// The purpose of this test is that some documentation indicates that connect
// binds the client's sends to send to a particular server endpoint, but does
// not bind the client's reads to only be from that endpoint, and that we need
// to always use recvfrom() to disambiguate.
TEST_F(UDPSocketTest, VerifyConnectBindsAddr) {
  const int kPort1 = 9999;
  const int kPort2 = 10000;
  std::string simple_message("hello world!");
  std::string foreign_message("BAD MESSAGE TO GET!!");

  // Setup the first server to listen.
  IPEndPoint bind_address;
  CreateUDPAddress("0.0.0.0", kPort1, &bind_address);
  UDPServerSocket server1(NULL, NetLog::Source());
  int rv = server1.Listen(bind_address);
  EXPECT_EQ(OK, rv);

  // Setup the second server to listen.
  CreateUDPAddress("0.0.0.0", kPort2, &bind_address);
  UDPServerSocket server2(NULL, NetLog::Source());
  rv = server2.Listen(bind_address);
  EXPECT_EQ(OK, rv);

  // Setup the client, connected to server 1.
  IPEndPoint server_address;
  CreateUDPAddress("127.0.0.1", kPort1, &server_address);
  UDPClientSocket client(NULL, NetLog::Source());
  rv = client.Connect(server_address);
  EXPECT_EQ(OK, rv);

  // Client sends to server1.
  rv = WriteSocket(&client, simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // Server1 waits for message.
  std::string str = RecvFromSocket(&server1);
  DCHECK(simple_message == str);

  // Get the client's address.
  IPEndPoint client_address;
  rv = client.GetLocalAddress(&client_address);
  EXPECT_EQ(OK, rv);

  // Server2 sends reply.
  rv = SendToSocket(&server2, foreign_message,
                    client_address);
  EXPECT_EQ(foreign_message.length(), static_cast<size_t>(rv));

  // Server1 sends reply.
  rv = SendToSocket(&server1, simple_message,
                    client_address);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // Client waits for response.
  str = ReadSocket(&client);
  DCHECK(simple_message == str);
}

TEST_F(UDPSocketTest, ClientGetLocalPeerAddresses) {
  struct TestData {
    std::string remote_address;
    std::string local_address;
    bool is_ipv6;
  } tests[] = {
    { "127.0.00.1", "127.0.0.1", false },
    { "192.168.1.1", "127.0.0.1", false },
    { "::1", "::1", true },
    { "2001:db8:0::42", "::1", true },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); i++) {
    net::IPAddressNumber ip_number;
    net::ParseIPLiteralToNumber(tests[i].remote_address, &ip_number);
    net::IPEndPoint remote_address(ip_number, 80);
    net::ParseIPLiteralToNumber(tests[i].local_address, &ip_number);
    net::IPEndPoint local_address(ip_number, 80);

    UDPClientSocket client(NULL, NetLog::Source());
    int rv = client.Connect(remote_address);
    EXPECT_LE(ERR_IO_PENDING, rv);

    IPEndPoint fetched_local_address;
    rv = client.GetLocalAddress(&fetched_local_address);
    EXPECT_EQ(OK, rv);

    // TODO(mbelshe): figure out how to verify the IP and port.
    //                The port is dynamically generated by the udp stack.
    //                The IP is the real IP of the client, not necessarily
    //                loopback.
    //EXPECT_EQ(local_address.address(), fetched_local_address.address());

    IPEndPoint fetched_remote_address;
    rv = client.GetPeerAddress(&fetched_remote_address);
    EXPECT_EQ(OK, rv);

    EXPECT_EQ(remote_address, fetched_remote_address);
  }
}

TEST_F(UDPSocketTest, ServerGetLocalAddress) {
  // TODO(mbelshe): implement me
}

TEST_F(UDPSocketTest, ServerGetPeerAddress) {
  // TODO(mbelshe): implement me
}

}  // namespace

}  // namespace net

int main(int argc, char** argv) {
  // Record histograms, so we can get histograms data in tests.
  base::StatisticsRecorder recorder;
  NetTestSuite test_suite(argc, argv);

  return test_suite.Run();
}
