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

TEST(SbSocketWaiterAddTest, SunnyDay) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SbSocket socket = CreateTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(socket));

  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterAddTest, SunnyDayPersistent) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SbSocket socket = CreateTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(socket));

  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, true));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterAddTest, SunnyDayMany) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  const int kMany = SB_FILE_MAX_OPEN;
  SbSocket sockets[kMany] = {0};
  for (int i = 0; i < kMany; ++i) {
    sockets[i] = SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp);
    EXPECT_TRUE(SbSocketIsValid(sockets[i]));
    EXPECT_TRUE(SbSocketWaiterAdd(
        waiter, sockets[i], NULL, &NoOpSocketWaiterCallback,
        kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(SbSocketDestroy(sockets[i]));
  }

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterAddTest, RainyDayAddToSameWaiter) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SbSocket socket = CreateTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(socket));

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

TEST(SbSocketWaiterAddTest, RainyDayAddToOtherWaiter) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  SbSocketWaiter waiter2 = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter2));

  SbSocket socket = CreateTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(socket));

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

TEST(SbSocketWaiterRemoveTest, RainyDayInvalidSocket) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  EXPECT_FALSE(SbSocketWaiterAdd(
      waiter, kSbSocketInvalid, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterRemoveTest, RainyDayInvalidWaiter) {
  SbSocket socket = CreateTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(socket));

  EXPECT_FALSE(SbSocketWaiterAdd(
      kSbSocketWaiterInvalid, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(SbSocketDestroy(socket));
}

TEST(SbSocketWaiterRemoveTest, RainyDayNoCallback) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
  SbSocket socket = CreateTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(socket));

  EXPECT_FALSE(SbSocketWaiterAdd(
      waiter, socket, NULL, NULL,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterRemoveTest, RainyDayNoInterest) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
  SbSocket socket = CreateTcpIpv4Socket();
  EXPECT_TRUE(SbSocketIsValid(socket));

  EXPECT_FALSE(SbSocketWaiterAdd(waiter, socket, NULL,
                                 &NoOpSocketWaiterCallback,
                                 kSbSocketWaiterInterestNone, false));

  EXPECT_TRUE(SbSocketDestroy(socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
