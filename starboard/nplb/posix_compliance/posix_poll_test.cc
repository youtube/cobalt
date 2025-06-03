// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

/*
  Test cases that are not feasible to implement in a unit testing environment:

  - Indefinite Wait: Testing an indefinite wait (-1 timeout) would cause the
    test to hang. While it could be implemented with a separate thread to
    signal the file descriptor, it adds complexity and potential flakiness.

  - nfds Exceeds Limit: Reliably testing the behavior when `nfds` exceeds the
    process's file descriptor limit is difficult to set up in a portable and
    non-disruptive way, which would trigger an `EINVAL` error.

  - EFAULT: Testing with an invalid `fds` pointer that is outside the
    process's address space is not reliably portable.

  - ENOMEM: Reliably simulating an out-of-memory condition is not feasible
    in a standard unit test environment.

  - EINTR: Reliably testing for interruption by a signal (EINTR) is prone to
    race conditions and can result in flaky tests, where the signal may arrive
    before or after the system call, instead of during.
*/

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Helper function to create a pipe for testing I/O events.
void CreatePipe(int* fds) {
  ASSERT_NE(pipe(fds), -1);
  // Ensure the pipe is non-blocking for reliable testing.
  fcntl(fds[0], F_SETFL, O_NONBLOCK);
  fcntl(fds[1], F_SETFL, O_NONBLOCK);
}

TEST(PosixPollTest, NoFdsReturnsImmediately) {
  // poll with no file descriptors should return 0 immediately.
  EXPECT_EQ(poll(NULL, 0, 100), 0);
}

TEST(PosixPollTest, TimeoutWithNoEvents) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[0];
  poll_fds[0].events = POLLIN;

  EXPECT_EQ(poll(poll_fds, 1, 50), 0);  // 50ms timeout

  close(fds[0]);
  close(fds[1]);
}

TEST(PosixPollTest, NegativeFdIsIgnored) {
  // A negative file descriptor should be ignored by poll, and the call should
  // time out, returning 0. The revents field for the invalid fd should be 0.
  struct pollfd poll_fds[1];
  poll_fds[0].fd = -1;
  poll_fds[0].events = POLLIN;
  poll_fds[0].revents = 1;  // Set to a non-zero value to ensure it's cleared.

  EXPECT_EQ(poll(poll_fds, 1, 0), 0);
  EXPECT_EQ(poll_fds[0].revents, 0);
}

TEST(PosixPollTest, PollInEvent) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[0];
  poll_fds[0].events = POLLIN;

  // Write data to the pipe to trigger a POLLIN event.
  char buffer[1] = {'a'};
  int res = write(fds[1], buffer, sizeof(buffer));
  EXPECT_NE(res, -1);

  int result = poll(poll_fds, 1, 1000);  // 1 second timeout

  EXPECT_EQ(result, 1);
  EXPECT_EQ(poll_fds[0].revents & POLLIN, POLLIN);

  close(fds[0]);
  close(fds[1]);
}

TEST(PosixPollTest, PollOutEvent) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[1];
  poll_fds[0].events = POLLOUT;

  int result = poll(poll_fds, 1, 1000);

  EXPECT_EQ(result, 1);
  EXPECT_EQ(poll_fds[0].revents & POLLOUT, POLLOUT);

  close(fds[0]);
  close(fds[1]);
}

TEST(PosixPollTest, PollErrEvent) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[1];
  poll_fds[0].events = POLLOUT;

  // Close the read end to cause an error on the write end.
  close(fds[0]);

  int result = poll(poll_fds, 1, 1000);

  EXPECT_EQ(result, 1);
  EXPECT_EQ(poll_fds[0].revents & POLLERR, POLLERR);

  close(fds[1]);
}

TEST(PosixPollTest, PollHupEvent) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[0];
  poll_fds[0].events = POLLIN;

  // Close the write end to cause a hang-up on the read end.
  close(fds[1]);

  int result = poll(poll_fds, 1, 1000);

  EXPECT_EQ(result, 1);
  EXPECT_TRUE(poll_fds[0].revents & POLLHUP);

  close(fds[0]);
}

TEST(PosixPollTest, ZeroTimeoutReturnsImmediately) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[0];
  poll_fds[0].events = POLLIN;

  EXPECT_EQ(poll(poll_fds, 1, 0), 0);

  close(fds[0]);
  close(fds[1]);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
