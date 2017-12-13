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

// The accept SunnyDay case is tested as a subset of at least one other test
// case, so it is not included redundantly here.

#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketAcceptTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

TEST_P(SbSocketAcceptTest, RainyDayNoConnection) {
  // Set up a socket to listen.
  SbSocket server_socket =
      CreateListeningTcpSocket(GetAddressType(), GetPortNumberForTests());
  ASSERT_TRUE(SbSocketIsValid(server_socket));

  // Don't create a socket to connect to it.

  // Spin briefly to ensure that it won't accept.
  SbSocket accepted_socket = AcceptBySpinning(server_socket, kSocketTimeout);
  EXPECT_FALSE(SbSocketIsValid(accepted_socket))
      << "Accepted with no one connecting.";

  SbSocketDestroy(accepted_socket);
  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST_P(SbSocketAcceptTest, RainyDayNotBound) {
  // Set up a socket, but don't Bind or Listen.
  SbSocket server_socket = CreateServerTcpSocket(GetAddressType());
  ASSERT_TRUE(SbSocketIsValid(server_socket));

  // Accept should result in an error.
  EXPECT_EQ(kSbSocketInvalid, SbSocketAccept(server_socket));
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketGetLastError(server_socket));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST_P(SbSocketAcceptTest, RainyDayNotListening) {
  // Set up a socket, but don't Bind or Listen.
  SbSocket server_socket =
      CreateBoundTcpSocket(GetAddressType(), GetPortNumberForTests());
  ASSERT_TRUE(SbSocketIsValid(server_socket));

  // Accept should result in an error.
  EXPECT_EQ(kSbSocketInvalid, SbSocketAccept(server_socket));
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketGetLastError(server_socket));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketAcceptTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketAcceptTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
