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

#include "starboard/common/log.h"
#include "starboard/common/socket.h"
#include "starboard/nplb/socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketIsConnectedTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

class PairSbSocketIsConnectedTest
    : public ::testing::TestWithParam<
          std::pair<SbSocketAddressType, SbSocketAddressType> > {
 public:
  SbSocketAddressType GetServerAddressType() { return GetParam().first; }
  SbSocketAddressType GetClientAddressType() { return GetParam().second; }
};

TEST_F(SbSocketIsConnectedTest, RainyDayInvalidSocket) {
  EXPECT_FALSE(SbSocketIsConnected(kSbSocketInvalid));
}

TEST_P(PairSbSocketIsConnectedTest, SunnyDay) {
  ConnectedTrio trio =
      CreateAndConnect(GetServerAddressType(), GetClientAddressType(),
                       GetPortNumberForTests(), kSocketTimeout);
  ASSERT_TRUE(SbSocketIsValid(trio.server_socket));
  EXPECT_FALSE(SbSocketIsConnected(trio.listen_socket));
  EXPECT_TRUE(SbSocketIsConnected(trio.server_socket));
  EXPECT_TRUE(SbSocketIsConnected(trio.client_socket));

  // This is just to contrast the behavior with SbSocketIsConnectedAndIdle.
  char buf[512] = {0};
  SbSocketSendTo(trio.server_socket, buf, sizeof(buf), NULL);
  EXPECT_TRUE(SbSocketIsConnected(trio.client_socket));
  SbSocketSendTo(trio.client_socket, buf, sizeof(buf), NULL);
  EXPECT_TRUE(SbSocketIsConnected(trio.server_socket));

  EXPECT_TRUE(SbSocketDestroy(trio.server_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.client_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.listen_socket));
}

TEST_P(SbSocketIsConnectedTest, SunnyDayNotConnected) {
  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));
  EXPECT_FALSE(SbSocketIsConnected(socket));
  EXPECT_TRUE(SbSocketDestroy(socket));
}

TEST_P(SbSocketIsConnectedTest, SunnyDayListeningNotConnected) {
  SbSocket server_socket =
      CreateListeningTcpSocket(GetAddressType(), GetPortNumberForTests());
  ASSERT_TRUE(SbSocketIsValid(server_socket));
  EXPECT_FALSE(SbSocketIsConnected(server_socket));
  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketIsConnectedTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketIsConnectedTest,
    ::testing::Values(
        std::make_pair(kSbSocketAddressTypeIpv4, kSbSocketAddressTypeIpv4),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv6),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv4)));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketIsConnectedTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketIsConnectedTest,
    ::testing::Values(std::make_pair(kSbSocketAddressTypeIpv4,
                                     kSbSocketAddressTypeIpv4)));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
