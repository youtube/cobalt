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

#include "starboard/socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbSocketCreateTest, TcpIpv4) {
  SbSocket socket =
      SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp);
  EXPECT_TRUE(SbSocketIsValid(socket));
  EXPECT_TRUE(SbSocketDestroy(socket));
}

TEST(SbSocketCreateTest, TcpIpv6) {
  // It is allowed for a platform not to support IPv6 sockets, but we use this
  // test to at least exercise the code path.
  SbSocket socket =
      SbSocketCreate(kSbSocketAddressTypeIpv6, kSbSocketProtocolTcp);
  if (SbSocketIsValid(socket)) {
    EXPECT_TRUE(SbSocketDestroy(socket));
  }
}

TEST(SbSocketCreateTest, UdpIpv4) {
  // It is allowed for a platform not to support UDP sockets, but we use this
  // test to at least exercise the code path.
  SbSocket socket =
      SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolUdp);
  if (SbSocketIsValid(socket)) {
    EXPECT_TRUE(SbSocketDestroy(socket));
  }
}

TEST(SbSocketCreateTest, UdpIpv6) {
  // It is allowed for a platform not to support UDP and/or IPv6 sockets, but we
  // use this test to at least exercise the code path.
  SbSocket socket =
      SbSocketCreate(kSbSocketAddressTypeIpv6, kSbSocketProtocolUdp);
  if (SbSocketIsValid(socket)) {
    EXPECT_TRUE(SbSocketDestroy(socket));
  }
}

TEST(SbSocketCreateTest, ATonOfTcpIpv4) {
  const int kATon = 4096;
  for (int i = 0; i < kATon; ++i) {
    SbSocket socket =
        SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp);
    EXPECT_TRUE(SbSocketIsValid(socket));
    EXPECT_TRUE(SbSocketDestroy(socket));
  }
}

TEST(SbSocketCreateTest, ManyTcpIpv4AtOnce) {
  const int kMany = 128;
  SbSocket sockets[kMany] = {0};
  for (int i = 0; i < kMany; ++i) {
    sockets[i] = SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp);
    EXPECT_TRUE(SbSocketIsValid(sockets[i]));
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(SbSocketDestroy(sockets[i]));
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
