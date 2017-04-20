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

class SbSocketGetLocalInterfaceAddressTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

TEST_F(SbSocketGetLocalInterfaceAddressTest, SunnyDay) {
  SbSocketAddress address;
  // Initialize to something invalid.
  SbMemorySet(&address, 0xFE, sizeof(address));
#if SB_API_VERSION < 4
  EXPECT_TRUE(SbSocketGetLocalInterfaceAddress(&address));
#else
  EXPECT_TRUE(SbSocketGetInterfaceAddress(NULL, &address, NULL));
#endif  // SB_API_VERSION < 4
  EXPECT_EQ(0, address.port);
  EXPECT_FALSE(IsUnspecified(&address));
  EXPECT_FALSE(IsLocalhost(&address));
}

TEST_F(SbSocketGetLocalInterfaceAddressTest, RainyDayNull) {
#if SB_API_VERSION < 4
  EXPECT_FALSE(SbSocketGetLocalInterfaceAddress(NULL));
#else
  EXPECT_FALSE(SbSocketGetInterfaceAddress(NULL, NULL, NULL));
#endif  // SB_API_VERSION < 4
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketGetLocalInterfaceAddressTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketGetLocalInterfaceAddressTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
