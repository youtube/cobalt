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

#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void NoOpSocketWaiterCallback(SbSocketWaiter waiter,
                              SbSocket socket,
                              void* context,
                              int ready_interests) {}

TEST(SbSocketWaiterRemoveTest, RainyDayInvalidSocket) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  EXPECT_FALSE(SbSocketWaiterRemove(waiter, kSbSocketInvalid));

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterRemoveTest, RainyDayInvalidWaiter) {
  SbSocket socket = CreateTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(socket));

  EXPECT_FALSE(SbSocketWaiterRemove(kSbSocketWaiterInvalid, socket));

  EXPECT_TRUE(SbSocketDestroy(socket));
}

TEST(SbSocketWaiterRemoveTest, RainyDayNotAdded) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
  SbSocket socket = CreateTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(socket));

  EXPECT_FALSE(SbSocketWaiterRemove(waiter, socket));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterRemoveTest, RainyDayAlreadyRemoved) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
  SbSocket socket = CreateTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(socket));

  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));
  EXPECT_TRUE(SbSocketWaiterRemove(waiter, socket));
  EXPECT_FALSE(SbSocketWaiterRemove(waiter, socket));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
