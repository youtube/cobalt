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
#include "starboard/configuration_constants.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketWaiterAddTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

void NoOpSocketWaiterCallback(SbSocketWaiter waiter,
                              SbSocket socket,
                              void* context,
                              int ready_interests) {}

TEST_P(SbSocketWaiterAddTest, SunnyDay) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));

  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST_P(SbSocketWaiterAddTest, SunnyDayPersistent) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));

  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, true));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST_P(SbSocketWaiterAddTest, SunnyDayMany) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  const int kMany = kSbFileMaxOpen;
  std::vector<SbSocket> sockets(kMany, 0);
  for (int i = 0; i < kMany; ++i) {
    sockets[i] = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
    ASSERT_TRUE(SbSocketIsValid(sockets[i]));
    EXPECT_TRUE(SbSocketWaiterAdd(
        waiter, sockets[i], NULL, &NoOpSocketWaiterCallback,
        kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(SbSocketDestroy(sockets[i]));
  }

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST_P(SbSocketWaiterAddTest, RainyDayAddToSameWaiter) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));

  // First add should succeed.
  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  // Second add should fail.
  EXPECT_FALSE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  // Remove should succeed.
  EXPECT_TRUE(SbSocketWaiterRemove(waiter, socket));

  // Add after remove should succeed.
  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  // Second add after remove should fail.
  EXPECT_FALSE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST_P(SbSocketWaiterAddTest, RainyDayAddToOtherWaiter) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SbSocketWaiter waiter2 = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter2));

  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));

  // The first add should succeed.
  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  // Adding to the other waiter should fail.
  EXPECT_FALSE(SbSocketWaiterAdd(
      waiter2, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  // Remove should succeed.
  EXPECT_TRUE(SbSocketWaiterRemove(waiter, socket));

  // Adding to other waiter after removing should succeed.
  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter2, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  // Adding back to first waiter after adding to the other waiter should fail.
  EXPECT_FALSE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter2));
}

TEST_F(SbSocketWaiterAddTest, RainyDayInvalidSocket) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  EXPECT_FALSE(SbSocketWaiterAdd(
      waiter, kSbSocketInvalid, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST_P(SbSocketWaiterAddTest, RainyDayInvalidWaiter) {
  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));

  EXPECT_FALSE(SbSocketWaiterAdd(
      kSbSocketWaiterInvalid, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(SbSocketDestroy(socket));
}

TEST_P(SbSocketWaiterAddTest, RainyDayNoCallback) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));

  EXPECT_FALSE(SbSocketWaiterAdd(
      waiter, socket, NULL, NULL,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST_P(SbSocketWaiterAddTest, RainyDayNoInterest) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));

  EXPECT_FALSE(SbSocketWaiterAdd(waiter, socket, NULL,
                                 &NoOpSocketWaiterCallback,
                                 kSbSocketWaiterInterestNone, false));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketWaiterAddTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketWaiterAddTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
