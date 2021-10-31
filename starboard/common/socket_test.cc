// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(SocketTest, TestGetUnspecifiedAddress) {
  SbSocketAddress address =
      GetUnspecifiedAddress(kSbSocketAddressTypeIpv4, 1010);
  EXPECT_EQ(1010, address.port);
  EXPECT_EQ(kSbSocketAddressTypeIpv4, address.type);
}

TEST(SocketTest, TestGetLocalhostAddressIpv4) {
  SbSocketAddress address = {};
  bool result = GetLocalhostAddress(kSbSocketAddressTypeIpv4, 2020, &address);
  ASSERT_TRUE(result);
  EXPECT_EQ(2020, address.port);
  EXPECT_EQ(kSbSocketAddressTypeIpv4, address.type);
  EXPECT_EQ(127, address.address[0]);
  EXPECT_EQ(0, address.address[1]);
  EXPECT_EQ(0, address.address[2]);
  EXPECT_EQ(1, address.address[3]);
}

TEST(SocketTest, TestGetLocalhostAddressIpv6) {
  SbSocketAddress address = {};
  bool result = GetLocalhostAddress(kSbSocketAddressTypeIpv6, 3030, &address);
  ASSERT_TRUE(result);
  EXPECT_EQ(3030, address.port);
  EXPECT_EQ(kSbSocketAddressTypeIpv6, address.type);
  for (int i = 0; i < 15; i++) {
    EXPECT_EQ(0, address.address[i]);
  }
  EXPECT_EQ(1, address.address[15]);
}

TEST(SocketTest, TestGetLocalhostAddressInvalidType) {
  SbSocketAddress address = {};
  bool result =
      GetLocalhostAddress(static_cast<SbSocketAddressType>(2), 4040, &address);
  ASSERT_FALSE(result);
}
}  // namespace
}  // namespace starboard
