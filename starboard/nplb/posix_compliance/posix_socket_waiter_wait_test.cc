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
#include "starboard/common/semaphore.h"
#include "starboard/common/socket.h"
#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"
#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SbPosixSocketWaiterWaitTest);

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

void FailSocketWaiterCallback(SbSocketWaiter waiter,
                              int socket,
                              void* context,
                              int ready_interests) {
  ADD_FAILURE() << __FUNCTION__ << " was called.";
}

TEST(SbPosixSocketWaiterWaitTest, SunnyDay) {
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

  // This add should do nothing, since it is already registered. Testing here
  // because we can see if it fires a write to the proper callback, then it
  // properly ignored this second set of parameters.
  EXPECT_FALSE(SbPosixSocketWaiterAdd(waiter, *trio.client_socket_fd_ptr,
                                      &values, &FailSocketWaiterCallback,
                                      kSbSocketWaiterInterestRead, false));

  WaitShouldNotBlock(waiter);

  EXPECT_EQ(1, values.count);  // Check that the callback was called once.
  EXPECT_EQ(waiter, values.waiter);
  EXPECT_EQ(*trio.client_socket_fd_ptr, values.socket);
  EXPECT_EQ(&values, values.context);
  EXPECT_EQ(kSbSocketWaiterInterestWrite, values.ready_interests);

  // Close and reopen sockets, this bug is tracked at b/348992515
  EXPECT_TRUE(close(*trio.server_socket_fd_ptr) == 0);
  EXPECT_TRUE(close(*trio.client_socket_fd_ptr) == 0);
  EXPECT_TRUE(close(*trio.listen_socket_fd_ptr) == 0);
  result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(GetPortNumberForTests()), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);
  ASSERT_TRUE(result == 0);
  ASSERT_TRUE(server_socket_fd >= 0);

  // Try again to make sure writable sockets are still writable
  values.count = 0;
  EXPECT_TRUE(SbPosixSocketWaiterAdd(
      waiter, *trio.client_socket_fd_ptr, &values, &TestSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  WaitShouldNotBlock(waiter);

  EXPECT_EQ(1, values.count);  // Check that the callback was called once.
  EXPECT_EQ(waiter, values.waiter);
  EXPECT_EQ(*trio.client_socket_fd_ptr, values.socket);
  EXPECT_EQ(&values, values.context);
  EXPECT_EQ(kSbSocketWaiterInterestWrite, values.ready_interests);

  // The client socket should become ready to read after we write some data to
  // it.
  values.count = 0;
  EXPECT_TRUE(SbPosixSocketWaiterAdd(waiter, *trio.client_socket_fd_ptr,
                                     &values, &TestSocketWaiterCallback,
                                     kSbSocketWaiterInterestRead, false));

  // Now we can send something to trigger readability.
  char* send_buf = new char[kBufSize];
  int bytes_sent =
      send(*trio.server_socket_fd_ptr, send_buf, kBufSize, kSendFlags);
  delete[] send_buf;
  EXPECT_LT(0, bytes_sent);

  WaitShouldNotBlock(waiter);

  EXPECT_EQ(1, values.count);
  EXPECT_EQ(waiter, values.waiter);
  EXPECT_EQ(*trio.client_socket_fd_ptr, values.socket);
  EXPECT_EQ(&values, values.context);
  EXPECT_EQ(kSbSocketWaiterInterestRead, values.ready_interests);

  EXPECT_TRUE(close(*trio.server_socket_fd_ptr) == 0);
  EXPECT_TRUE(close(*trio.client_socket_fd_ptr) == 0);
  EXPECT_TRUE(close(*trio.listen_socket_fd_ptr) == 0);
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

struct AlreadyReadyContext {
  AlreadyReadyContext()
      : waiter(kSbSocketWaiterInvalid),
        a_write_result(false),
        a_read_result(false),
        b_result(false) {}
  ~AlreadyReadyContext() { EXPECT_TRUE(SbSocketWaiterDestroy(waiter)); }

  SbSocketWaiter waiter;
  struct trio_socket_fd trio;
  Semaphore wrote_a_signal;
  bool a_write_result;
  bool a_read_result;
  Semaphore write_b_signal;
  bool b_result;
  Semaphore wrote_b_signal;
};

const char kAData[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
const char kBData[] = "bb";

void* AlreadyReadyEntryPoint(void* param) {
  AlreadyReadyContext* context = reinterpret_cast<AlreadyReadyContext*>(param);

  context->a_write_result =
      PosixWriteBySpinning(*context->trio.server_socket_fd_ptr, kAData,
                           SB_ARRAY_SIZE_INT(kAData), kSocketTimeout);
  context->wrote_a_signal.Put();

  context->write_b_signal.Take();
  context->b_result =
      PosixWriteBySpinning(*context->trio.server_socket_fd_ptr, kBData,
                           SB_ARRAY_SIZE_INT(kBData), kSocketTimeout);
  context->wrote_b_signal.Put();
  return NULL;
}

void WakeUpSocketWaiterCallback(SbSocketWaiter waiter,
                                int socket,
                                void* param,
                                int ready_interests) {
  SbSocketWaiterWakeUp(waiter);
}

void AlreadyReadySocketWaiterCallback(SbSocketWaiter waiter,
                                      int socket,
                                      void* param,
                                      int ready_interests) {
  AlreadyReadyContext* context = reinterpret_cast<AlreadyReadyContext*>(param);
  // Read in the A data.
  char buffer[SB_ARRAY_SIZE_INT(kAData)];
  context->a_read_result =
      PosixReadBySpinning(*context->trio.client_socket_fd_ptr, buffer,
                          SB_ARRAY_SIZE_INT(buffer), kSocketTimeout);

  // Tell the thread to write the B data now.
  context->write_b_signal.Put();

  // Wait until it thinks it has finished.
  context->wrote_b_signal.Take();

  // Now add the second read callback, and hope that it gets called.
  EXPECT_TRUE(SbPosixSocketWaiterAdd(waiter, socket, context,
                                     &WakeUpSocketWaiterCallback,
                                     kSbSocketWaiterInterestRead, false));
}

// This test ensures that if the socket gets written to while it is not being
// waited on, and inside a callback, it will become ready immediately the next
// time it is waited on.
TEST(SbPosixSocketWaiterWaitTest, SunnyDayAlreadyReady) {
  AlreadyReadyContext context;
  context.waiter = SbSocketWaiterCreate();
  ASSERT_TRUE(SbSocketWaiterIsValid(context.waiter));

  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  int result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(GetPortNumberForTests()), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);
  ASSERT_TRUE(result == 0);
  ASSERT_TRUE(server_socket_fd >= 0);

  context.trio.listen_socket_fd_ptr = &listen_socket_fd;
  context.trio.client_socket_fd_ptr = &client_socket_fd;
  context.trio.server_socket_fd_ptr = &server_socket_fd;

  EXPECT_TRUE(SbPosixSocketWaiterAdd(
      context.waiter, *context.trio.client_socket_fd_ptr, &context,
      &AlreadyReadySocketWaiterCallback, kSbSocketWaiterInterestRead, false));

  pthread_t thread = 0;
  pthread_create(&thread, nullptr, AlreadyReadyEntryPoint, &context);
  EXPECT_TRUE(thread != 0);
  context.wrote_a_signal.Take();

  WaitShouldNotBlock(context.waiter);

  EXPECT_EQ(pthread_join(thread, NULL), 0);
  EXPECT_TRUE(close(listen_socket_fd) == 0);
  EXPECT_TRUE(close(client_socket_fd) == 0);
  EXPECT_TRUE(close(server_socket_fd) == 0);
}

TEST(SbPosixSocketWaiterWaitTest, RainyDayInvalidWaiter) {
  WaitShouldNotBlock(kSbSocketWaiterInvalid);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
