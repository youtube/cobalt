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
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketWaiterRemoveTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

void NoOpSocketWaiterCallback(SbSocketWaiter waiter,
                              SbSocket socket,
                              void* context,
                              int ready_interests) {}

TEST_F(SbSocketWaiterRemoveTest, RainyDayInvalidSocket) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  EXPECT_FALSE(SbSocketWaiterRemove(waiter, kSbSocketInvalid));

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST_P(SbSocketWaiterRemoveTest, RainyDayInvalidWaiter) {
  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));

  EXPECT_FALSE(SbSocketWaiterRemove(kSbSocketWaiterInvalid, socket));

  EXPECT_TRUE(SbSocketDestroy(socket));
}

TEST_P(SbSocketWaiterRemoveTest, RainyDayNotAdded) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));

  EXPECT_FALSE(SbSocketWaiterRemove(waiter, socket));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST_P(SbSocketWaiterRemoveTest, RainyDayAlreadyRemoved) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
  SbSocket socket = SbSocketCreate(GetAddressType(), kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));

  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));
  EXPECT_TRUE(SbSocketWaiterRemove(waiter, socket));
  EXPECT_FALSE(SbSocketWaiterRemove(waiter, socket));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketWaiterRemoveTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketWaiterRemoveTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
