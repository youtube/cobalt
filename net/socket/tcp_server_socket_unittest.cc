// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_server_socket.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {
const int kListenBacklog = 5;

class TCPServerSocketTest : public PlatformTest {
 protected:
  TCPServerSocketTest()
      : socket_(NULL, NetLog::Source()) {
  }

  void SetUpIPv4() {
    IPEndPoint address;
    ParseAddress("127.0.0.1", 0, &address);
    ASSERT_EQ(OK, socket_.Listen(address, kListenBacklog));
    ASSERT_EQ(OK, socket_.GetLocalAddress(&local_address_));
  }

  void SetUpIPv6(bool* success) {
    *success = false;
    IPEndPoint address;
    ParseAddress("::1", 0, &address);
    if (socket_.Listen(address, kListenBacklog) != 0) {
      LOG(ERROR) << "Failed to listen on ::1 - probably because IPv6 is "
          "disabled. Skipping the test";
      return;
    }
    ASSERT_EQ(OK, socket_.GetLocalAddress(&local_address_));
    *success = true;
  }

  void ParseAddress(std::string ip_str, int port, IPEndPoint* address) {
    IPAddressNumber ip_number;
    bool rv = ParseIPLiteralToNumber(ip_str, &ip_number);
    if (!rv)
      return;
    *address = IPEndPoint(ip_number, port);
  }

  static IPEndPoint GetPeerAddress(StreamSocket* socket) {
    AddressList address;
    EXPECT_EQ(OK, socket->GetPeerAddress(&address));
    IPEndPoint endpoint;
    EXPECT_TRUE(endpoint.FromSockAddr(
        address.head()->ai_addr, address.head()->ai_addrlen));
    return endpoint;
  }

  AddressList local_address_list() const {
    return AddressList::CreateFromIPAddress(
        local_address_.address(), local_address_.port());
  }

  TCPServerSocket socket_;
  IPEndPoint local_address_;
};

TEST_F(TCPServerSocketTest, Accept) {
  ASSERT_NO_FATAL_FAILURE(SetUpIPv4());

  TestOldCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(),
                                    NULL, NetLog::Source());
  connecting_socket.Connect(&connect_callback);

  TestCompletionCallback accept_callback;
  scoped_ptr<StreamSocket> accepted_socket;
  int result = socket_.Accept(&accepted_socket, accept_callback.callback());
  if (result == ERR_IO_PENDING)
    result = accept_callback.WaitForResult();
  ASSERT_EQ(OK, result);

  ASSERT_TRUE(accepted_socket.get() != NULL);

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());

  EXPECT_EQ(OK, connect_callback.WaitForResult());
}

// Test Accept() callback.
TEST_F(TCPServerSocketTest, AcceptAsync) {
  ASSERT_NO_FATAL_FAILURE(SetUpIPv4());

  TestCompletionCallback accept_callback;
  scoped_ptr<StreamSocket> accepted_socket;

  ASSERT_EQ(ERR_IO_PENDING,
            socket_.Accept(&accepted_socket, accept_callback.callback()));

  TestOldCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(),
                                    NULL, NetLog::Source());
  connecting_socket.Connect(&connect_callback);

  EXPECT_EQ(OK, connect_callback.WaitForResult());
  EXPECT_EQ(OK, accept_callback.WaitForResult());

  EXPECT_TRUE(accepted_socket != NULL);

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());
}

// Accept two connections simultaneously.
TEST_F(TCPServerSocketTest, Accept2Connections) {
  ASSERT_NO_FATAL_FAILURE(SetUpIPv4());

  TestCompletionCallback accept_callback;
  scoped_ptr<StreamSocket> accepted_socket;

  ASSERT_EQ(ERR_IO_PENDING,
            socket_.Accept(&accepted_socket, accept_callback.callback()));

  TestOldCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(),
                                    NULL, NetLog::Source());
  connecting_socket.Connect(&connect_callback);

  TestOldCompletionCallback connect_callback2;
  TCPClientSocket connecting_socket2(local_address_list(),
                                     NULL, NetLog::Source());
  connecting_socket2.Connect(&connect_callback2);

  EXPECT_EQ(OK, accept_callback.WaitForResult());

  TestCompletionCallback accept_callback2;
  scoped_ptr<StreamSocket> accepted_socket2;
  int result = socket_.Accept(&accepted_socket2, accept_callback2.callback());
  if (result == ERR_IO_PENDING)
    result = accept_callback2.WaitForResult();
  ASSERT_EQ(OK, result);

  EXPECT_EQ(OK, connect_callback.WaitForResult());

  EXPECT_TRUE(accepted_socket != NULL);
  EXPECT_TRUE(accepted_socket2 != NULL);
  EXPECT_NE(accepted_socket.get(), accepted_socket2.get());

  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());
  EXPECT_EQ(GetPeerAddress(accepted_socket2.get()).address(),
            local_address_.address());
}

TEST_F(TCPServerSocketTest, AcceptIPv6) {
  bool initialized = false;
  ASSERT_NO_FATAL_FAILURE(SetUpIPv6(&initialized));
  if (!initialized)
    return;

  TestOldCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(),
                                    NULL, NetLog::Source());
  connecting_socket.Connect(&connect_callback);

  TestCompletionCallback accept_callback;
  scoped_ptr<StreamSocket> accepted_socket;
  int result = socket_.Accept(&accepted_socket, accept_callback.callback());
  if (result == ERR_IO_PENDING)
    result = accept_callback.WaitForResult();
  ASSERT_EQ(OK, result);

  ASSERT_TRUE(accepted_socket.get() != NULL);

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(GetPeerAddress(accepted_socket.get()).address(),
            local_address_.address());

  EXPECT_EQ(OK, connect_callback.WaitForResult());
}

}  // namespace

}  // namespace net
