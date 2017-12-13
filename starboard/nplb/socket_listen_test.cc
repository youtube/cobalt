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

// SbSocketListen is Sunny Day tested in several other tests, so those won't be
// included here.

#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketListenTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

TEST_F(SbSocketListenTest, RainyDayInvalid) {
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketListen(kSbSocketInvalid));
}

TEST_P(SbSocketListenTest, SunnyDayUnbound) {
  SbSocket server_socket = CreateServerTcpSocket(GetAddressType());
  ASSERT_TRUE(SbSocketIsValid(server_socket));

  EXPECT_EQ(kSbSocketOk, SbSocketListen(server_socket));

  // Listening on an unbound socket should listen to a system-assigned port on
  // all local interfaces.
  SbSocketAddress address = {0};
  EXPECT_TRUE(SbSocketGetLocalAddress(server_socket, &address));
  EXPECT_EQ(GetAddressType(), address.type);
  EXPECT_NE(0, address.port);
  EXPECT_TRUE(IsUnspecified(&address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketListenTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketListenTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
