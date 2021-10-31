// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/socket.h"
#include "starboard/nplb/socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const unsigned char kInvalidByte = 0xFE;

class SbSocketGetInterfaceAddressTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

TEST(SbSocketGetInterfaceAddressTest, SunnyDay) {
  SbSocketAddress invalid_address;
  SbSocketAddress address;

  // Initialize to something invalid.
  memset(&address, kInvalidByte, sizeof(address));
  memset(&invalid_address, kInvalidByte, sizeof(invalid_address));

  EXPECT_TRUE(SbSocketGetInterfaceAddress(NULL, &address, NULL));
  EXPECT_EQ(0, address.port);
  EXPECT_FALSE(IsUnspecified(&address));
  EXPECT_FALSE(IsLocalhost(&address));
  EXPECT_NE(0, memcmp(address.address, invalid_address.address,
                      SB_ARRAY_SIZE(address.address)));
}

TEST(SbSocketGetInterfaceAddressTest, RainyDayNull) {
  EXPECT_FALSE(SbSocketGetInterfaceAddress(NULL, NULL, NULL));
}

TEST(SbSocketGetInterfaceAddressTest, SunnyDayNullDestination) {
  SbSocketAddress netmask;
  SbSocketAddress source;

  memset(&netmask, kInvalidByte, sizeof(netmask));
  memset(&source, kInvalidByte, sizeof(source));

  // If destination address is NULL, then any IP address that is valid for
  // |destination| set to 0.0.0.0 (IPv4) or :: (IPv6) can be returned.

  EXPECT_TRUE(SbSocketGetInterfaceAddress(NULL, &source, NULL));
  EXPECT_TRUE(source.type == kSbSocketAddressTypeIpv4 ||
              source.type == kSbSocketAddressTypeIpv6);

  EXPECT_TRUE(SbSocketGetInterfaceAddress(NULL, &source, &netmask));
  // A netmask that starts with 0 is likely incorrect.
  EXPECT_TRUE(netmask.address[0] & 0x80);
  EXPECT_TRUE(source.type == kSbSocketAddressTypeIpv4 ||
              source.type == kSbSocketAddressTypeIpv6);
  EXPECT_TRUE(netmask.type == kSbSocketAddressTypeIpv4 ||
              netmask.type == kSbSocketAddressTypeIpv6);
}

TEST_P(SbSocketGetInterfaceAddressTest, SunnyDayDestination) {
  SbSocketAddress destination = {0};
  destination.type = GetAddressType();

  SbSocketAddress netmask;
  SbSocketAddress source;

  // Initialize to something invalid.
  memset(&netmask, kInvalidByte, sizeof(netmask));
  memset(&source, kInvalidByte, sizeof(source));

  EXPECT_TRUE(SbSocketGetInterfaceAddress(&destination, &source, NULL));
  EXPECT_EQ(GetAddressType(), source.type);
  EXPECT_TRUE(SbSocketGetInterfaceAddress(&destination, &source, &netmask));

  EXPECT_FALSE(IsLocalhost(&source));

  // A netmask that starts with 0 is likely incorrect.
  EXPECT_TRUE(netmask.address[0] & 0x80);
  EXPECT_EQ(GetAddressType(), source.type);
  EXPECT_EQ(GetAddressType(), netmask.type);
  EXPECT_EQ(0, source.port);
}

TEST_P(SbSocketGetInterfaceAddressTest, SunnyDaySourceForDestination) {
  const char kTestHostName[] = "www.example.com";

  SbSocketResolveFilter resolve_filter = kSbSocketResolveFilterNone;
  switch (GetAddressType()) {
    case kSbSocketAddressTypeIpv4:
      resolve_filter = kSbSocketResolveFilterIpv4;
      break;
    case kSbSocketAddressTypeIpv6:
      resolve_filter = kSbSocketResolveFilterIpv6;
      break;
    default:
      FAIL() << "Invalid address type " << GetAddressType();
  }
  SbSocketResolution* resolution =
      SbSocketResolve(kTestHostName, resolve_filter);

  // TODO: Switch to nullptr, when C++11 is available.
  ASSERT_NE(resolution, reinterpret_cast<SbSocketResolution*>(NULL));
  ASSERT_NE(resolution->address_count, 0);
  SbSocketAddress& destination_address = resolution->addresses[0];

  SbSocketAddress source;
  SbSocketAddress netmask;
  SbSocketAddress invalid_address;
  memset(&netmask, kInvalidByte, sizeof(netmask));
  memset(&source, kInvalidByte, sizeof(source));
  memset(&invalid_address, kInvalidByte, sizeof(source));
  SbSocketGetInterfaceAddress(&destination_address, &source, &netmask);

  EXPECT_EQ(GetAddressType(), source.type);
  EXPECT_NE(0, source.port);
  // A netmask that starts with 0 is likely incorrect.
  EXPECT_TRUE(netmask.address[0] & 0x80);
  EXPECT_EQ(GetAddressType(), netmask.type);
  EXPECT_NE(0, memcmp(source.address, invalid_address.address,
                      SB_ARRAY_SIZE(source.address)));
  EXPECT_NE(0, memcmp(netmask.address, invalid_address.address,
                      SB_ARRAY_SIZE(netmask.address)));

  SbSocketFreeResolution(resolution);
}

TEST_P(SbSocketGetInterfaceAddressTest, SunnyDaySourceNotLoopback) {
  SbSocketAddress destination = {0};
  destination.type = GetAddressType();

  // If the destination address is 0.0.0.0, and its |type| is
  // |kSbSocketAddressTypeIpv4|, then any IPv4 local interface that is up and
  // not a loopback interface is a valid return value.
  //
  // If the destination address is ::, and its |type| is
  // |kSbSocketAddressTypeIpv6| then any IPv6 local interface that is up and
  // not loopback or a link-local IP is a valid return value.  However, in the
  // case of IPv6, the address with the biggest scope must be returned.  E.g., a
  // globally scoped and routable IP is preferred over a unique local address
  // (ULA). Also, the IP address that is returned must be permanent.

  SbSocketAddress netmask;
  SbSocketAddress source;
  SbSocketAddress invalid_address;

  // Initialize to something invalid.
  memset(&netmask, kInvalidByte, sizeof(netmask));
  memset(&source, kInvalidByte, sizeof(source));
  memset(&invalid_address, kInvalidByte, sizeof(invalid_address));

  EXPECT_TRUE(SbSocketGetInterfaceAddress(&destination, &source, NULL));
  EXPECT_EQ(GetAddressType(), source.type);
  EXPECT_TRUE(SbSocketGetInterfaceAddress(&destination, &source, &netmask));
  EXPECT_FALSE(IsLocalhost(&source));
  EXPECT_FALSE(IsUnspecified(&source));

  EXPECT_NE(0, memcmp(netmask.address, invalid_address.address,
                      SB_ARRAY_SIZE(netmask.address)));
  EXPECT_NE(0, memcmp(source.address, invalid_address.address,
                      SB_ARRAY_SIZE(source.address)));
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketGetInterfaceAddressTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketGetInterfaceAddressTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
#endif  // SB_HAS(IPV6)

}  // namespace
}  // namespace nplb
}  // namespace starboard
