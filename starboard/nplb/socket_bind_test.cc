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

// This is to use NULL in asserts, which otherwise complain about long
// vs. pointer type.
const void* kNull = NULL;

TEST(SbSocketBindTest, RainyDayNullSocket) {
  SbSocketAddress address = GetIpv4Unspecified(GetPortNumberForTests());
  EXPECT_EQ(kSbSocketErrorFailed, SbSocketBind(kSbSocketInvalid, &address));
}

TEST(SbSocketBindTest, RainyDayNullAddress) {
  SbSocket server_socket = CreateServerTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(server_socket));

  // Binding with a NULL address should fail.
  EXPECT_EQ(kSbSocketErrorFailed, SbSocketBind(server_socket, NULL));

  // Even though that failed, binding the same socket now with 0.0.0.0:2048
  // should work.
  SbSocketAddress address = GetIpv4Unspecified(GetPortNumberForTests());
  EXPECT_EQ(kSbSocketOk, SbSocketBind(server_socket, &address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

TEST(SbSocketBindTest, RainyDayNullNull) {
  EXPECT_EQ(kSbSocketErrorFailed, SbSocketBind(kSbSocketInvalid, NULL));
}

#if SB_HAS(IPV6)
TEST(SbSocketBindTest, RainyDayWrongAddressType) {
  SbSocket server_socket = CreateServerTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(server_socket));

  // Binding with the wrong address type should fail.
  SbSocketAddress address = GetIpv6Unspecified(GetPortNumberForTests());
  EXPECT_EQ(kSbSocketErrorFailed, SbSocketBind(server_socket, &address));

  // Even though that failed, binding the same socket now with 0.0.0.0:2048
  // should work.
  address = GetIpv4Unspecified(GetPortNumberForTests());
  EXPECT_EQ(kSbSocketOk, SbSocketBind(server_socket, &address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}
#endif

TEST(SbSocketBindTest, RainyDayBadInterface) {
  SbSocket server_socket = CreateServerTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(server_socket));

  // Binding with an interface that doesn't exist on this device should fail, so
  // let's find an address of a well-known public website that we shouldn't be
  // able to bind to.
  const char* kTestHostName = "www.yahoo.com";
  SbSocketResolution* resolution =
      SbSocketResolve(kTestHostName, kSbSocketResolveFilterIpv4);
  ASSERT_NE(kNull, resolution);
  EXPECT_LT(0, resolution->address_count);
  EXPECT_EQ(kSbSocketErrorFailed,
            SbSocketBind(server_socket, &resolution->addresses[0]));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
  SbSocketFreeResolution(resolution);
}

TEST(SbSocketBindTest, SunnyDayLocalInterface) {
  SbSocket server_socket = CreateServerTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(server_socket));

  SbSocketAddress address = {0};
  EXPECT_TRUE(SbSocketGetLocalInterfaceAddress(&address));
  EXPECT_EQ(kSbSocketOk, SbSocketBind(server_socket, &address));

  EXPECT_TRUE(SbSocketDestroy(server_socket));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
