// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketCreateTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

class PairSbSocketCreateTest
    : public ::testing::TestWithParam<
          std::pair<SbSocketAddressType, SbSocketProtocol> > {
 public:
  SbSocketAddressType GetAddressType() { return GetParam().first; }
  SbSocketProtocol GetProtocol() { return GetParam().second; }
};

TEST_P(PairSbSocketCreateTest, Create) {
  SbSocket socket = SbSocketCreate(GetAddressType(), GetProtocol());
#if !SB_HAS(IPV6)
  // It is allowed for a platform not to support IPv6 sockets, but we use this
  // test to at least exercise the code path.
  if (kSbSocketAddressTypeIpv6 == GetAddressType()) {
    if (SbSocketIsValid(socket)) {
      EXPECT_TRUE(SbSocketDestroy(socket));
    }
    return;
  }
#endif
  if (kSbSocketProtocolUdp == GetProtocol()) {
    // It is allowed for a platform not to support UDP sockets, but we use this
    // test to at least exercise the code path.
    if (SbSocketIsValid(socket)) {
      EXPECT_TRUE(SbSocketDestroy(socket));
    }
    return;
  }
  ASSERT_TRUE(SbSocketIsValid(socket));
  EXPECT_TRUE(SbSocketDestroy(socket));
}

TEST_P(SbSocketCreateTest, ATonOfTcp) {
  const int kATon = 4096;
  for (int i = 0; i < kATon; ++i) {
    SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
    ASSERT_TRUE(SbSocketIsValid(socket));
    EXPECT_TRUE(SbSocketDestroy(socket));
  }
}

TEST_P(SbSocketCreateTest, ManyTcpAtOnce) {
  const int kMany = 128;
  SbSocket sockets[kMany] = {0};
  for (int i = 0; i < kMany; ++i) {
    sockets[i] = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
    ASSERT_TRUE(SbSocketIsValid(sockets[i]));
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(SbSocketDestroy(sockets[i]));
  }
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketCreateTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketCreateTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
#endif

INSTANTIATE_TEST_CASE_P(
    SbSocketTypes,
    PairSbSocketCreateTest,
    ::testing::Values(
        std::make_pair(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp),
        std::make_pair(kSbSocketAddressTypeIpv4, kSbSocketProtocolUdp),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketProtocolTcp),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketProtocolUdp)));

}  // namespace
}  // namespace nplb
}  // namespace starboard
