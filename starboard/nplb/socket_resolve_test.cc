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

#include <utility>

#include "starboard/log.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "starboard/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketResolveTest
    : public ::testing::TestWithParam<
          std::pair<SbSocketResolveFilter, SbSocketAddressType> > {
 public:
  SbSocketResolveFilter GetResolveFilter() { return GetParam().first; }
  SbSocketAddressType GetAddressType() { return GetParam().second; }
};

// This is to use NULL in asserts, which otherwise complain about long
// vs. pointer type.
const void* kNull = NULL;

// A random host name to use to test DNS resolution.
const char kTestHostName[] = "www.example.com";
const char kLocalhost[] = "localhost";

#define LOG_ADDRESS(i) "In returned address #" << (i)

TEST_F(SbSocketResolveTest, SunnyDay) {
  SbSocketResolution* resolution = SbSocketResolve(kTestHostName, 0);
  ASSERT_NE(kNull, resolution);
  EXPECT_LT(0, resolution->address_count);
  EXPECT_NE(kNull, resolution->addresses);
  for (int i = 0; i < resolution->address_count; ++i) {
    const SbSocketAddress& address = resolution->addresses[i];
    EXPECT_TRUE(address.type == kSbSocketAddressTypeIpv4 ||
                address.type == kSbSocketAddressTypeIpv6)
        << LOG_ADDRESS(i);
    EXPECT_FALSE(IsUnspecified(&address)) << LOG_ADDRESS(i);
  }

  SbSocketFreeResolution(resolution);
}

TEST_P(SbSocketResolveTest, Localhost) {
  SbSocketResolution* resolution =
      SbSocketResolve(kLocalhost, GetResolveFilter());
  ASSERT_NE(kNull, resolution);
  EXPECT_LT(0, resolution->address_count);
  EXPECT_NE(kNull, resolution->addresses);
  for (int i = 0; i < resolution->address_count; ++i) {
    const SbSocketAddress& address = resolution->addresses[i];
    EXPECT_TRUE(address.type == kSbSocketAddressTypeIpv4 ||
                address.type == kSbSocketAddressTypeIpv6)
        << LOG_ADDRESS(i);
    EXPECT_TRUE(IsLocalhost(&address)) << LOG_ADDRESS(i);
    EXPECT_FALSE(IsUnspecified(&address)) << LOG_ADDRESS(i);
  }

  SbSocketFreeResolution(resolution);
}

TEST_P(SbSocketResolveTest, SunnyDayFiltered) {
  SbSocketResolution* resolution =
      SbSocketResolve(kTestHostName, GetResolveFilter());
  ASSERT_NE(kNull, resolution);
  EXPECT_LT(0, resolution->address_count);
  EXPECT_NE(kNull, resolution->addresses);
  for (int i = 0; i < resolution->address_count; ++i) {
    const SbSocketAddress& address = resolution->addresses[i];
    EXPECT_EQ(GetAddressType(), address.type) << LOG_ADDRESS(i);
  }
  SbSocketFreeResolution(resolution);
}

TEST_F(SbSocketResolveTest, IgnoreExtraBits) {
  // Even with this extra bit set, the resolution should come out the same.
  SbSocketResolution* resolution1 = SbSocketResolve(kTestHostName, 1 << 14);
  SbSocketResolution* resolution2 = SbSocketResolve(kTestHostName, 0);
  ASSERT_NE(kNull, resolution1);
  ASSERT_NE(kNull, resolution2);
  EXPECT_LT(0, resolution2->address_count);
  EXPECT_EQ(resolution2->address_count, resolution1->address_count);
  EXPECT_NE(kNull, resolution1->addresses);
  EXPECT_NE(kNull, resolution2->addresses);
  SbSocketFreeResolution(resolution1);
  SbSocketFreeResolution(resolution2);
}

TEST_F(SbSocketResolveTest, RainyDayNullHostname) {
  SbSocketResolution* resolution = SbSocketResolve(NULL, 0);
  ASSERT_EQ(kNull, resolution);
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    SbSocketResolveTest,
    ::testing::Values(
        std::make_pair(kSbSocketResolveFilterIpv4, kSbSocketAddressTypeIpv4),
        std::make_pair(kSbSocketResolveFilterIpv6, kSbSocketAddressTypeIpv6)));
#else
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    SbSocketResolveTest,
    ::testing::Values(std::make_pair(kSbSocketResolveFilterIpv4,
                                     kSbSocketAddressTypeIpv4)));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
