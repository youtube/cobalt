// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/udp/udp_listen_socket.h"

#include <netinet/in.h>

#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class MockDelegate : public UDPListenSocket::Delegate {
 public:
  MOCK_METHOD1(DidClose, void(UDPListenSocket*));
  MOCK_METHOD4(DidRead, void(UDPListenSocket*, const char*, int,
                             const IPEndPoint*));
};

using ::testing::_;
using ::testing::StrEq;

// Create a DGRAM socket and bound to random port.
SocketDescriptor GetSocketBoundToRandomPort() {
  SocketDescriptor s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  SockaddrStorage src_addr;
  struct sockaddr_in *in = (struct sockaddr_in *)src_addr.addr;
  in->sin_family = AF_INET;
  in->sin_port = htons(0);
  in->sin_addr.s_addr = INADDR_ANY;
  ::bind(s, src_addr.addr, src_addr.addr_len);
  SetNonBlocking(s);
  return s;
}

// Get the address from |s| and send data to that address
// using another DGRAM socket.
void SendDataToListeningSocket(SocketDescriptor s,
                               const char* data, int size) {
  // Get the watching address
  SockaddrStorage bind_addr;
  ::getsockname(s, bind_addr.addr, &bind_addr.addr_len);

  // Create another socket.
  SocketDescriptor cs = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  ::sendto(cs, data, size, 0, bind_addr.addr, bind_addr.addr_len);
}

const std::string& kSampleData = "Hello World.";

TEST(UDPListenSocketTest, DoRead) {
  SocketDescriptor s = GetSocketBoundToRandomPort();
  MockDelegate del;
  scoped_refptr<UDPListenSocket> sock = new UDPListenSocket(s, &del);

  // send the extra null character.
  SendDataToListeningSocket(s, kSampleData.c_str(), kSampleData.size() + 1);

  // Let poll wait enough time for the data to be caught in.
  usleep(200 * 1000);

  EXPECT_CALL(del, DidRead(sock.get(), StrEq(kSampleData.c_str()),
                           kSampleData.size() + 1, _));
  MessageLoop::current()->RunUntilIdle();
}

TEST(UDPListenSocketTest, DoReadReturnsNullAtEnd) {
  SocketDescriptor s = GetSocketBoundToRandomPort();
  MockDelegate del;
  scoped_refptr<UDPListenSocket> sock = new UDPListenSocket(s, &del);

  SendDataToListeningSocket(s, kSampleData.data(), kSampleData.size());

  // Let poll wait enough time for the data to be caught in.
  usleep(200 * 1000);

  // still expect kSampleData, since that has been padded with '\0'.
  EXPECT_CALL(del, DidRead(sock.get(), StrEq(kSampleData.c_str()),
                           kSampleData.size(), _));
  MessageLoop::current()->RunUntilIdle();
}

// Create 2 UDPListenSockets. Use one to send message to another.
TEST(UDPListenSocketTest, SendTo) {
  SocketDescriptor s1 = GetSocketBoundToRandomPort();
  MockDelegate del1;
  scoped_refptr<UDPListenSocket> sock1 = new UDPListenSocket(s1, &del1);

  SocketDescriptor s2 = GetSocketBoundToRandomPort();
  MockDelegate del2;
  scoped_refptr<UDPListenSocket> sock2 = new UDPListenSocket(s2, &del2);

  // Get the port of s1
  SockaddrStorage bind_addr;
  ::getsockname(s1, bind_addr.addr, &bind_addr.addr_len);
  IPEndPoint s1_addr;
  ignore_result(s1_addr.FromSockAddr(bind_addr.addr, bind_addr.addr_len));

  // Send message to s1 using s2
  sock2->SendTo(s1_addr, kSampleData);

  // Let poll wait enough time for the data to be caught in.
  usleep(200 * 1000);

  EXPECT_CALL(del1, DidRead(sock1.get(), StrEq(kSampleData.c_str()),
                           kSampleData.size(), _));
  MessageLoop::current()->RunUntilIdle();
}

}
