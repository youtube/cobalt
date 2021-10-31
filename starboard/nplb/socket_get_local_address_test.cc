// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <utility>

#include "starboard/common/socket.h"
#include "starboard/nplb/socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketGetLocalAddressTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

class PairSbSocketGetLocalAddressTest
    : public ::testing::TestWithParam<
          std::pair<SbSocketAddressType, SbSocketAddressType> > {
 public:
  SbSocketAddressType GetServerAddressType() { return GetParam().first; }
  SbSocketAddressType GetClientAddressType() { return GetParam().second; }
};

TEST_F(SbSocketGetLocalAddressTest, RainyDayInvalidSocket) {
  SbSocketAddress address = {0};
  EXPECT_FALSE(SbSocketGetLocalAddress(kSbSocketInvalid, &address));
}

TEST_P(SbSocketGetLocalAddressTest, RainyDayNullAddress) {
  SbSocket server_socket = CreateBoundTcpSocket(GetAddressType(), 0);
  ASSERT_TRUE(SbSocketIsValid(server_socket));
  EXPECT_FALSE(SbSocketGetLocalAddress(server_socket, NULL));
  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST_F(SbSocketGetLocalAddressTest, RainyDayNullNull) {
  EXPECT_FALSE(SbSocketGetLocalAddress(kSbSocketInvalid, NULL));
}

TEST_P(SbSocketGetLocalAddressTest, SunnyDayUnbound) {
  SbSocket server_socket = CreateServerTcpSocket(GetAddressType());
  SbSocketAddress address = {0};
  EXPECT_TRUE(SbSocketGetLocalAddress(server_socket, &address));
  EXPECT_EQ(GetAddressType(), address.type);
  EXPECT_EQ(0, address.port);
  EXPECT_TRUE(IsUnspecified(&address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST_P(SbSocketGetLocalAddressTest, SunnyDayBoundUnspecified) {
  SbSocket server_socket = CreateBoundTcpSocket(GetAddressType(), 0);
  ASSERT_TRUE(SbSocketIsValid(server_socket));

  SbSocketAddress address = {0};
  EXPECT_TRUE(SbSocketGetLocalAddress(server_socket, &address));
  EXPECT_EQ(GetAddressType(), address.type);
  EXPECT_NE(0, address.port);

  // We bound to an unspecified address, so it should come back that way.
  EXPECT_TRUE(IsUnspecified(&address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST_F(SbSocketGetLocalAddressTest, SunnyDayBoundSpecified) {
  SbSocketAddress interface_address = {0};
  EXPECT_TRUE(SbSocketGetInterfaceAddress(NULL, &interface_address, NULL));

  SbSocket server_socket = CreateServerTcpSocket(interface_address.type);
  ASSERT_TRUE(SbSocketIsValid(server_socket));

  EXPECT_EQ(kSbSocketOk, SbSocketBind(server_socket, &interface_address));

  SbSocketAddress local_address = {0};
  EXPECT_TRUE(SbSocketGetLocalAddress(server_socket, &local_address));
  EXPECT_NE(0, local_address.port);
  EXPECT_FALSE(IsUnspecified(&local_address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST_P(PairSbSocketGetLocalAddressTest, SunnyDayConnected) {
  const int kPort = GetPortNumberForTests();
  ConnectedTrio trio = CreateAndConnect(
      GetServerAddressType(), GetClientAddressType(), kPort, kSocketTimeout);
  ASSERT_TRUE(SbSocketIsValid(trio.server_socket));

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

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketGetLocalAddressTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketGetLocalAddressTest,
    ::testing::Values(
        std::make_pair(kSbSocketAddressTypeIpv4, kSbSocketAddressTypeIpv4),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv6),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv4)));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketGetLocalAddressTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketGetLocalAddressTest,
    ::testing::Values(std::make_pair(kSbSocketAddressTypeIpv4,
                                     kSbSocketAddressTypeIpv4)));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
