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

  - Indefinite Wait: Testing an indefinite wait would cause the test to hang.
    While it could be implemented with a separate thread to signal the file
    descriptor, it adds complexity and potential flakiness.

  - nfds Exceeds Limit: Reliably testing the behavior when `nfds` exceeds the
    process's file descriptor limit is difficult to set up in a portable and
    non-disruptive way.

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
#include <time.h>
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

TEST(PosixPpollTest, NoFdsReturnsImmediately) {
  // ppoll with no file descriptors should return 0 immediately.
  struct timespec timeout = {0, 100000000};  // 100ms
  EXPECT_EQ(ppoll(NULL, 0, &timeout, NULL), 0);
}

TEST(PosixPpollTest, TimeoutWithNoEvents) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[0];
  poll_fds[0].events = POLLIN;

  struct timespec timeout = {0, 50000000};  // 50ms
  EXPECT_EQ(ppoll(poll_fds, 1, &timeout, NULL), 0);

  close(fds[0]);
  close(fds[1]);
}

TEST(PosixPpollTest, InvalidFd) {
  struct pollfd poll_fds[1];
  poll_fds[0].fd = -1;
  poll_fds[0].events = POLLIN;
  poll_fds[0].revents = 1;  // Set to a non-zero value to ensure it's cleared.

  struct timespec timeout = {0, 0};
  EXPECT_EQ(ppoll(poll_fds, 1, &timeout, NULL), 0);
  EXPECT_EQ(poll_fds[0].revents, 0);
}

TEST(PosixPpollTest, PollInEvent) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[0];
  poll_fds[0].events = POLLIN;

  // Write data to the pipe to trigger a POLLIN event.
  char buffer[1] = {'a'};
  int res = write(fds[1], buffer, sizeof(buffer));
  EXPECT_NE(res, -1);

  struct timespec timeout = {1, 0};  // 1 second
  int result = ppoll(poll_fds, 1, &timeout, NULL);

  EXPECT_EQ(result, 1);
  EXPECT_EQ(poll_fds[0].revents & POLLIN, POLLIN);

  close(fds[0]);
  close(fds[1]);
}

TEST(PosixPpollTest, PollOutEvent) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[1];
  poll_fds[0].events = POLLOUT;

  struct timespec timeout = {1, 0};
  int result = ppoll(poll_fds, 1, &timeout, NULL);

  EXPECT_EQ(result, 1);
  EXPECT_EQ(poll_fds[0].revents & POLLOUT, POLLOUT);

  close(fds[0]);
  close(fds[1]);
}

TEST(PosixPpollTest, PollErrEvent) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[1];
  poll_fds[0].events = POLLOUT;

  // Close the read end to cause an error on the write end.
  close(fds[0]);

  struct timespec timeout = {1, 0};
  int result = ppoll(poll_fds, 1, &timeout, NULL);

  EXPECT_EQ(result, 1);
  EXPECT_EQ(poll_fds[0].revents & POLLERR, POLLERR);

  close(fds[1]);
}

TEST(PosixPpollTest, PollHupEvent) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[0];
  poll_fds[0].events = POLLIN;

  // Close the write end to cause a hang-up on the read end.
  close(fds[1]);

  struct timespec timeout = {1, 0};
  int result = ppoll(poll_fds, 1, &timeout, NULL);

  EXPECT_EQ(result, 1);
  EXPECT_TRUE(poll_fds[0].revents & POLLHUP);

  close(fds[0]);
}

TEST(PosixPpollTest, ZeroTimeoutReturnsImmediately) {
  int fds[2];
  CreatePipe(fds);

  struct pollfd poll_fds[1];
  poll_fds[0].fd = fds[0];
  poll_fds[0].events = POLLIN;

  struct timespec timeout = {0, 0};
  EXPECT_EQ(ppoll(poll_fds, 1, &timeout, NULL), 0);

  close(fds[0]);
  close(fds[1]);
}

TEST(PosixPpollTest, InvalidTimeout) {
  struct pollfd poll_fds[1];
  poll_fds[0].fd = 0;  // stdin
  poll_fds[0].events = POLLIN;

  struct timespec timeout = {0, 1000000000};  // 1 second, invalid nsec
  EXPECT_EQ(ppoll(poll_fds, 1, &timeout, NULL), -1);
  EXPECT_EQ(errno, EINVAL);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
