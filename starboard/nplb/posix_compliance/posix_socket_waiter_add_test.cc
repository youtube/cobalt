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

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "starboard/common/socket.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbPosixSocketWaiterAddTest : public ::testing::TestWithParam<int> {
 public:
  int GetAddressType() { return GetParam(); }
};

// Type of SbPosixSocketWaiterCallback
void NoOpSocketWaiterCallback(SbSocketWaiter waiter,
                              int socket,
                              void* context,
                              int ready_interests) {}

TEST(SbPosixSocketWaiterAddTest, SunnyDay) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket >= 0);

  EXPECT_TRUE(SbPosixSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(close(socket) == 0);
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbPosixSocketWaiterAddTest, SunnyDayPersistent) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket >= 0);

  EXPECT_TRUE(SbPosixSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, true));

  EXPECT_TRUE(close(socket) == 0);
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbPosixSocketWaiterAddTest, SunnyDayMany) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  const int kMany = kSbFileMaxOpen;
  std::vector<int> sockets(kMany, 0);

  for (int i = 0; i < kMany; ++i) {
    sockets[i] = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_TRUE(sockets[i] >= 0);
    EXPECT_TRUE(SbPosixSocketWaiterAdd(
        waiter, sockets[i], NULL, &NoOpSocketWaiterCallback,
        kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(close(sockets[i]) == 0);
  }

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbPosixSocketWaiterAddTest, RainyDayAddToSameWaiter) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket >= 0);

  // First add should succeed.
  EXPECT_TRUE(SbPosixSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  // Second add should fail.
  EXPECT_FALSE(SbPosixSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  // Remove should succeed.
  EXPECT_TRUE(SbPosixSocketWaiterRemove(waiter, socket));

  // Add after remove should succeed.
  EXPECT_TRUE(SbPosixSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  // Second add after remove should fail.
  EXPECT_FALSE(SbPosixSocketWaiterAdd(
      waiter, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(close(socket) == 0);
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbPosixSocketWaiterAddTest, RainyDayInvalidSocket) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  EXPECT_FALSE(SbPosixSocketWaiterAdd(
      waiter, -1, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbPosixSocketWaiterAddTest, RainyDayInvalidWaiter) {
  int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket >= 0);

  EXPECT_FALSE(SbPosixSocketWaiterAdd(
      kSbSocketWaiterInvalid, socket, NULL, &NoOpSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(close(socket) == 0);
}

TEST(SbPosixSocketWaiterAddTest, RainyDayNoCallback) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));
  int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket >= 0);

  EXPECT_FALSE(SbPosixSocketWaiterAdd(
      waiter, socket, NULL, NULL,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  EXPECT_TRUE(close(socket) == 0);
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbPosixSocketWaiterAddTest, RainyDayNoInterest) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket >= 0);

  EXPECT_FALSE(SbPosixSocketWaiterAdd(waiter, socket, NULL,
                                      &NoOpSocketWaiterCallback,
                                      kSbSocketWaiterInterestNone, false));

  EXPECT_TRUE(close(socket) == 0);
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
