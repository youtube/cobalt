// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

SbSocket CreateMulticastSocket(const SbSocketAddress& address) {
  SbSocket socket =
      SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolUdp);
  EXPECT_TRUE(SbSocketIsValid(socket));
  EXPECT_TRUE(SbSocketSetReuseAddress(socket, true));

  // It will choose a port for me.
  SbSocketAddress bind_address =
      GetUnspecifiedAddress(kSbSocketAddressTypeIpv4, 0);
  EXPECT_EQ(kSbSocketOk, SbSocketBind(socket, &bind_address));

  EXPECT_TRUE(SbSocketJoinMulticastGroup(socket, &address));
  return socket;
}

SbSocket CreateSendSocket() {
  SbSocket socket =
      SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolUdp);
  EXPECT_TRUE(SbSocketIsValid(socket));
  EXPECT_TRUE(SbSocketSetReuseAddress(socket, true));
  return socket;
}

TEST(SbSocketJoinMulticastGroupTest, SunnyDay) {
  // "224.0.2.0" is an unassigned ad-hoc multicast address. Hopefully no one is
  // spamming it on the local network.
  // http://www.iana.org/assignments/multicast-addresses/multicast-addresses.xhtml
  SbSocketAddress address = GetUnspecifiedAddress(kSbSocketAddressTypeIpv4, 0);
  address.address[0] = 224;
  address.address[2] = 2;

  SbSocket send_socket = CreateSendSocket();
  SbSocket receive_socket = CreateMulticastSocket(address);

  // Get the bound port.
  SbSocketAddress local_address = {0};
  SbSocketGetLocalAddress(receive_socket, &local_address);
  address.port = local_address.port;

  const char kBuf[] = "01234567890123456789";
  char buf[SB_ARRAY_SIZE(kBuf)] = {0};
  while (true) {
    int sent =
        SbSocketSendTo(send_socket, kBuf, SB_ARRAY_SIZE_INT(kBuf), &address);
    if (sent < 0) {
      SbSocketError error = SbSocketGetLastError(send_socket);
      if (error == kSbSocketPending) {
        continue;
      }

      FAIL() << "Failed to send multicast packet: " << error;
      return;
    }
    EXPECT_EQ(SB_ARRAY_SIZE_INT(kBuf), sent);
    break;
  }

  SbSocketAddress receive_address;
  int loop_counts = 10000;
  while (true) {
    // Breaks the case where the test will hang in a loop when
    // SbSocketReceiveFrom() always returns kSbSocketPending.
    ASSERT_GE(loop_counts--, 0) << "Multicast timed out.";
    int received = SbSocketReceiveFrom(
        receive_socket, buf, SB_ARRAY_SIZE_INT(buf), &receive_address);
    if (received < 0 &&
        SbSocketGetLastError(receive_socket) == kSbSocketPending) {
      continue;
    }
    EXPECT_EQ(SB_ARRAY_SIZE_INT(kBuf), received);
    break;
  }

  for (int i = 0; i < SB_ARRAY_SIZE_INT(kBuf); ++i) {
    EXPECT_EQ(kBuf[i], buf[i]) << "position " << i;
  }

  EXPECT_TRUE(SbSocketDestroy(send_socket));
  EXPECT_TRUE(SbSocketDestroy(receive_socket));
}

TEST(SbSocketJoinMulticastGroupTest, RainyDayInvalidSocket) {
  SbSocketAddress address = GetUnspecifiedAddress(kSbSocketAddressTypeIpv4, 0);
  EXPECT_FALSE(SbSocketJoinMulticastGroup(kSbSocketInvalid, &address));
}

TEST(SbSocketJoinMulticastGroupTest, RainyDayInvalidAddress) {
  SbSocket socket =
      SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolUdp);
  ASSERT_TRUE(SbSocketIsValid(socket));
  EXPECT_FALSE(SbSocketJoinMulticastGroup(socket, NULL));
  EXPECT_TRUE(SbSocketDestroy(socket));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
