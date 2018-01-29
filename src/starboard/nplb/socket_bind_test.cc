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

// The basic Sunny Day test is a subset of other Sunny Day tests, so it is not
// repeated here.

#include <utility>

#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketBindTest
    : public ::testing::TestWithParam<
          std::pair<SbSocketAddressType, SbSocketResolveFilter> > {
 public:
  SbSocketAddressType GetAddressType() { return GetParam().first; }
  SbSocketResolveFilter GetFilterType() { return GetParam().second; }
};

#if SB_HAS(IPV6)
class PairSbSocketBindTest
    : public ::testing::TestWithParam<
          std::pair<SbSocketAddressType, SbSocketAddressType> > {
 public:
  SbSocketAddressType GetServerAddressType() { return GetParam().first; }
  SbSocketAddressType GetClientAddressType() { return GetParam().second; }
};
#endif

// This is to use NULL in asserts, which otherwise complain about long
// vs. pointer type.
const void* kNull = NULL;

TEST_P(SbSocketBindTest, RainyDayNullSocket) {
  SbSocketAddress address =
      GetUnspecifiedAddress(GetAddressType(), GetPortNumberForTests());
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketBind(kSbSocketInvalid, &address));
}

TEST_P(SbSocketBindTest, RainyDayNullAddress) {
  SbSocket server_socket = CreateServerTcpSocket(GetAddressType());
  ASSERT_TRUE(SbSocketIsValid(server_socket));

  // Binding with a NULL address should fail.
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketBind(server_socket, NULL));

  // Even though that failed, binding the same socket now with 0.0.0.0:2048
  // should work.
  SbSocketAddress address =
      GetUnspecifiedAddress(GetAddressType(), GetPortNumberForTests());
  EXPECT_EQ(kSbSocketOk, SbSocketBind(server_socket, &address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST_F(SbSocketBindTest, RainyDayNullNull) {
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketBind(kSbSocketInvalid, NULL));
}

#if SB_HAS(IPV6)
TEST_P(PairSbSocketBindTest, RainyDayWrongAddressType) {
  SbSocket server_socket = CreateServerTcpSocket(GetServerAddressType());
  ASSERT_TRUE(SbSocketIsValid(server_socket));

  // Binding with the wrong address type should fail.
  SbSocketAddress address =
      GetUnspecifiedAddress(GetClientAddressType(), GetPortNumberForTests());
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketBind(server_socket, &address));

  // Even though that failed, binding the same socket now with the server
  // address type should work.
  address =
      GetUnspecifiedAddress(GetServerAddressType(), GetPortNumberForTests());
  EXPECT_EQ(kSbSocketOk, SbSocketBind(server_socket, &address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}
#endif

TEST_P(SbSocketBindTest, RainyDayBadInterface) {
  SbSocket server_socket = CreateServerTcpSocket(GetAddressType());
  ASSERT_TRUE(SbSocketIsValid(server_socket));

  // Binding with an interface that doesn't exist on this device should fail, so
  // let's find an address of a well-known public website that we shouldn't be
  // able to bind to.
  const char* kTestHostName = "www.yahoo.com";
  SbSocketResolution* resolution =
      SbSocketResolve(kTestHostName, GetFilterType());
  ASSERT_NE(kNull, resolution);
  EXPECT_LT(0, resolution->address_count);
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(
      SbSocketBind(server_socket, &resolution->addresses[0]));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
  SbSocketFreeResolution(resolution);
}

TEST_F(SbSocketBindTest, SunnyDayLocalInterface) {
  SbSocketAddress address = {0};
  EXPECT_TRUE(SbSocketGetInterfaceAddress(NULL, &address, NULL));
  SbSocket server_socket = CreateServerTcpSocket(address.type);
  ASSERT_TRUE(SbSocketIsValid(server_socket));

  EXPECT_EQ(kSbSocketOk, SbSocketBind(server_socket, &address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    SbSocketBindTest,
    ::testing::Values(
        std::make_pair(kSbSocketAddressTypeIpv4, kSbSocketResolveFilterIpv4),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketResolveFilterIpv6)));
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketBindTest,
    ::testing::Values(
        std::make_pair(kSbSocketAddressTypeIpv4, kSbSocketAddressTypeIpv6),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv4)));
#else
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    SbSocketBindTest,
    ::testing::Values(std::make_pair(kSbSocketAddressTypeIpv4,
                                     kSbSocketResolveFilterIpv4)));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
