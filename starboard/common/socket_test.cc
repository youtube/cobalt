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

#include <arpa/inet.h>

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(SocketTest, TestGetUnspecifiedAddress) {
  struct sockaddr_storage address = GetUnspecifiedAddress(AF_INET, 1010);
  struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(&address);
  EXPECT_EQ(AF_INET, addr->sin_family);
  EXPECT_EQ(htons(1010), addr->sin_port);
}

TEST(SocketTest, TestGetLocalhostAddressIpv4) {
  struct sockaddr_storage address = {};
  bool result = GetLocalhostAddress(AF_INET, 2020, &address);
  ASSERT_TRUE(result);
  struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(&address);
  EXPECT_EQ(AF_INET, addr->sin_family);
  EXPECT_EQ(htons(2020), addr->sin_port);
  char ip_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &addr->sin_addr, ip_str, INET_ADDRSTRLEN);
  EXPECT_STREQ("127.0.0.1", ip_str);
}

TEST(SocketTest, TestGetLocalhostAddressIpv6) {
  struct sockaddr_storage address = {};
  bool result = GetLocalhostAddress(AF_INET6, 3030, &address);
  ASSERT_TRUE(result);
  struct sockaddr_in6* addr = reinterpret_cast<struct sockaddr_in6*>(&address);
  EXPECT_EQ(AF_INET6, addr->sin6_family);
  EXPECT_EQ(htons(3030), addr->sin6_port);
  char ip_str[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &addr->sin6_addr, ip_str, INET6_ADDRSTRLEN);
  EXPECT_STREQ("::1", ip_str);
}

TEST(SocketTest, TestGetLocalhostAddressInvalidType) {
  struct sockaddr_storage address = {};
  bool result = GetLocalhostAddress(2, 4040, &address);
  ASSERT_FALSE(result);
}
}  // namespace
}  // namespace starboard
