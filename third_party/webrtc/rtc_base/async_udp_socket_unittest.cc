/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/async_udp_socket.h"

#include <cstdint>
#include <memory>

#include "rtc_base/async_packet_socket.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::NotNull;

TEST(AsyncUDPSocketTest, SetSocketOptionIfEctChange) {
  const SocketAddress kAddr("22.22.22.22", 0);
  VirtualSocketServer socket_server;
  std::unique_ptr<AsyncUDPSocket> udp_socket =
      AsyncUDPSocket::Create(CreateTestEnvironment(), kAddr, socket_server);
  ASSERT_THAT(udp_socket, NotNull());

  int ect = 0;
  udp_socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  ASSERT_EQ(ect, 0);

  uint8_t buffer[] = "hello";
  AsyncSocketPacketOptions packet_options;
  packet_options.ecn_1 = false;
  udp_socket->SendTo(buffer, 5, kAddr, packet_options);
  udp_socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  EXPECT_EQ(ect, 0);

  packet_options.ecn_1 = true;
  udp_socket->SendTo(buffer, 5, kAddr, packet_options);
  udp_socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  EXPECT_EQ(ect, 1);

  packet_options.ecn_1 = false;
  udp_socket->SendTo(buffer, 5, kAddr, packet_options);
  udp_socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  EXPECT_EQ(ect, 0);
}

}  // namespace webrtc
