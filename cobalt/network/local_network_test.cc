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

#include "cobalt/network/local_network.h"

#include "base/logging.h"
#include "cobalt/network/socket_address_parser.h"
#include "starboard/common/socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace cobalt {
namespace network {

template <size_t N>
SbSocketAddress ParseSocketAddress(const char (&address)[N]) {
  const char* spec = address;
  url::Component component = url::MakeRange(0, N - 1);
  SbSocketAddress out_socket_address;
  memset(&out_socket_address, 0, sizeof(SbSocketAddress));
  CHECK(ParseSocketAddress(spec, component, &out_socket_address));
  return out_socket_address;
}

TEST(IsPrivateRange, v4) {
  EXPECT_TRUE(IsIPInPrivateRange(ParseSocketAddress("10.0.0.1")));
  EXPECT_TRUE(IsIPInPrivateRange(ParseSocketAddress("172.16.1.1")));
  EXPECT_TRUE(IsIPInPrivateRange(ParseSocketAddress("192.168.1.1")));
  EXPECT_FALSE(IsIPInPrivateRange(ParseSocketAddress("127.0.0.1")));
  EXPECT_FALSE(IsIPInPrivateRange(ParseSocketAddress("143.195.170.1")));
  EXPECT_FALSE(IsIPInPrivateRange(ParseSocketAddress("0.0.0.0")));
  EXPECT_FALSE(IsIPInPrivateRange(ParseSocketAddress("239.255.255.255")));
}

#if SB_HAS(IPV6)

TEST(IsPrivateRange, v6) {
  EXPECT_TRUE(IsIPInPrivateRange(ParseSocketAddress("[fd00::]")));
  EXPECT_TRUE(IsIPInPrivateRange(ParseSocketAddress("[fd00:1:2:3:4:5::]")));
  EXPECT_FALSE(IsIPInPrivateRange(ParseSocketAddress("[fe80::]")));
  EXPECT_FALSE(IsIPInPrivateRange(
      ParseSocketAddress("[2606:2800:220:1:248:1893:25c8:1946]")));
}

#endif  // SB_HAS(IPV6)

}  // namespace network
}  // namespace cobalt
