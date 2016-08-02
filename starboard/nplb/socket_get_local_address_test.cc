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
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbSocketGetLocalAddressTest, RainyDayInvalidSocket) {
  SbSocketAddress address = {0};
  EXPECT_FALSE(SbSocketGetLocalAddress(kSbSocketInvalid, &address));
}

TEST(SbSocketGetLocalAddressTest, RainyDayNullAddress) {
  SbSocket server_socket = CreateBoundTcpIpv4Socket(0);
  EXPECT_TRUE(SbSocketIsValid(server_socket));
  EXPECT_FALSE(SbSocketGetLocalAddress(server_socket, NULL));
  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST(SbSocketGetLocalAddressTest, RainyDayNullNull) {
  EXPECT_FALSE(SbSocketGetLocalAddress(kSbSocketInvalid, NULL));
}

TEST(SbSocketGetLocalAddressTest, SunnyDayUnbound) {
  SbSocket server_socket = CreateServerTcpIpv4Socket();
  SbSocketAddress address = {0};
  EXPECT_TRUE(SbSocketGetLocalAddress(server_socket, &address));
  EXPECT_EQ(kSbSocketAddressTypeIpv4, address.type);
  EXPECT_EQ(0, address.port);
  EXPECT_TRUE(IsUnspecified(&address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST(SbSocketGetLocalAddressTest, SunnyDayBoundUnspecified) {
  SbSocket server_socket = CreateBoundTcpIpv4Socket(0);
  EXPECT_TRUE(SbSocketIsValid(server_socket));

  SbSocketAddress address = {0};
  EXPECT_TRUE(SbSocketGetLocalAddress(server_socket, &address));
  EXPECT_EQ(kSbSocketAddressTypeIpv4, address.type);
  EXPECT_NE(0, address.port);

  // We bound to an unspecified address, so it should come back that way.
  EXPECT_TRUE(IsUnspecified(&address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST(SbSocketGetLocalAddressTest, SunnyDayBoundSpecified) {
  SbSocket server_socket = CreateServerTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(server_socket));

  {
    SbSocketAddress address = {0};
    EXPECT_TRUE(SbSocketGetLocalInterfaceAddress(&address));
    EXPECT_EQ(kSbSocketOk, SbSocketBind(server_socket, &address));
  }

  SbSocketAddress address = {0};
  EXPECT_TRUE(SbSocketGetLocalAddress(server_socket, &address));
  EXPECT_EQ(kSbSocketAddressTypeIpv4, address.type);
  EXPECT_NE(0, address.port);
  EXPECT_FALSE(IsUnspecified(&address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST(SbSocketGetLocalAddressTest, SunnyDayConnected) {
  const int kPort = GetPortNumberForTests();
  ConnectedTrio trio = CreateAndConnect(kPort, kSocketTimeout);
  if (!SbSocketIsValid(trio.server_socket)) {
    return;
  }

  SbSocketAddress address = {0};
  EXPECT_TRUE(SbSocketGetLocalAddress(trio.listen_socket, &address));
  EXPECT_TRUE(IsUnspecified(&address));
  EXPECT_EQ(kPort, address.port);

  SbSocketAddress address1 = {0};
  EXPECT_TRUE(SbSocketGetLocalAddress(trio.client_socket, &address1));
  EXPECT_FALSE(IsUnspecified(&address1));
  EXPECT_NE(kPort, address1.port);

  SbSocketAddress address2 = {0};
  EXPECT_TRUE(SbSocketGetLocalAddress(trio.server_socket, &address2));
  EXPECT_FALSE(IsUnspecified(&address2));
  EXPECT_EQ(kPort, address2.port);

  EXPECT_TRUE(SbSocketDestroy(trio.server_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.client_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.listen_socket));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
