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

#include <utility>

#include "starboard/common/log.h"
#include "starboard/common/socket.h"
#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SbPosixSocketWaiterWaitTimedTest);

struct CallbackValues {
  int count;
  SbSocketWaiter waiter;
  int socket;
  void* context;
  int ready_interests;
};

void TestSocketWaiterCallback(SbSocketWaiter waiter,
                              int socket,
                              void* context,
                              int ready_interests) {
  CallbackValues* values = reinterpret_cast<CallbackValues*>(context);
  if (values) {
    ++values->count;
    values->waiter = waiter;
    values->socket = socket;
    values->context = context;
    values->ready_interests = ready_interests;
  }
  SbSocketWaiterWakeUp(waiter);
}

TEST(SbPosixSocketWaiterWaitTimedTest, SunnyDay) {
  const int kBufSize = 1024;

  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  int result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(GetPortNumberForTests()), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);
  ASSERT_TRUE(result == 0);
  ASSERT_TRUE(server_socket_fd >= 0);

  struct trio_socket_fd trio = {&listen_socket_fd, &client_socket_fd,
                                &server_socket_fd};

  // The client socket should be ready to write right away, but not read until
  // it gets some data.
  CallbackValues values = {0};
  EXPECT_TRUE(SbPosixSocketWaiterAdd(
      waiter, *trio.client_socket_fd_ptr, &values, &TestSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  TimedWaitShouldNotBlock(waiter, kSocketTimeout);

  // Even though we waited for no time, we should have gotten this callback.
  EXPECT_EQ(1, values.count);  // Check that the callback was called once.
  EXPECT_EQ(waiter, values.waiter);
  EXPECT_EQ(*trio.client_socket_fd_ptr, values.socket);
  EXPECT_EQ(&values, values.context);
  EXPECT_EQ(kSbSocketWaiterInterestWrite, values.ready_interests);

  // While we haven't written anything, we should block indefinitely, and
  // receive no read callbacks.
  values.count = 0;
  EXPECT_TRUE(SbPosixSocketWaiterAdd(waiter, *trio.client_socket_fd_ptr,
                                     &values, &TestSocketWaiterCallback,
                                     kSbSocketWaiterInterestRead, false));

  TimedWaitShouldBlock(waiter, kSocketTimeout);

  EXPECT_EQ(0, values.count);

  // The client socket should become ready to read after we write some data to
  // it.
  char* send_buf = new char[kBufSize];
  int bytes_sent =
      send(*trio.server_socket_fd_ptr, send_buf, kBufSize, kSendFlags);
  delete[] send_buf;
  EXPECT_LT(0, bytes_sent);

  TimedWaitShouldNotBlock(waiter, kSocketTimeout);

  EXPECT_EQ(1, values.count);
  EXPECT_EQ(waiter, values.waiter);
  EXPECT_EQ(*trio.client_socket_fd_ptr, values.socket);
  EXPECT_EQ(&values, values.context);
  EXPECT_EQ(kSbSocketWaiterInterestRead, values.ready_interests);

  EXPECT_TRUE(close(listen_socket_fd) == 0);
  EXPECT_TRUE(close(client_socket_fd) == 0);
  EXPECT_TRUE(close(server_socket_fd) == 0);
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST(SbSocketWaiterWaitTimedTest, RainyDayInvalidWaiter) {
  TimedWaitShouldNotBlock(kSbSocketWaiterInvalid, kSocketTimeout);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
