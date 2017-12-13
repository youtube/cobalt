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

#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketConnectTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

TEST_P(SbSocketConnectTest, RainyDayNullSocket) {
  SbSocketAddress address = GetUnspecifiedAddress(GetAddressType(), 2048);
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketConnect(kSbSocketInvalid, &address));
}

TEST_P(SbSocketConnectTest, RainyDayNullAddress) {
  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketConnect(socket, NULL));
  EXPECT_TRUE(SbSocketDestroy(socket));
}

TEST_F(SbSocketConnectTest, RainyDayNullNull) {
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketConnect(kSbSocketInvalid, NULL));
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketConnectTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketConnectTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
