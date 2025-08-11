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

  - Indefinite Wait: Testing an indefinite wait (NULL timeout) would cause the
    test to hang. While it could be implemented with a separate thread to
    signal the file descriptor, it adds complexity and potential flakiness.

  - nfds Exceeds Limit: Reliably testing the behavior when `nfds` exceeds the
    process's file descriptor limit is difficult to set up in a portable and
    non-disruptive way, which would trigger an `EINVAL` error.

  - EFAULT: Testing with an invalid `fd_set` pointer that is outside the
    process's address space is not reliably portable.

  - ENOMEM: Reliably simulating an out-of-memory condition is not feasible
    in a standard unit test environment.

  - EINTR: Reliably testing for interruption by a signal (EINTR) is prone to
    race conditions and can result in flaky tests, where the signal may arrive
    before or after the system call, instead of during.
*/

#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
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

TEST(PosixSelectTest, NoFdsReturnsImmediately) {
  // select with nfds = 0 should return 0 immediately.
  struct timeval timeout = {0, 100000};  // 100ms
  EXPECT_EQ(select(0, NULL, NULL, NULL, &timeout), 0);
}

TEST(PosixSelectTest, TimeoutWithNoEvents) {
  int fds[2];
  CreatePipe(fds);
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(fds[0], &read_fds);

  struct timeval timeout = {0, 50000};  // 50ms
  EXPECT_EQ(select(fds[0] + 1, &read_fds, NULL, NULL, &timeout), 0);

  close(fds[0]);
  close(fds[1]);
}

TEST(PosixSelectTest, InvalidNfds) {
  // nfds being a negative value should result in an error.
  EXPECT_EQ(select(-1, NULL, NULL, NULL, NULL), -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST(PosixSelectTest, ReadEvent) {
  int fds[2];
  CreatePipe(fds);
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(fds[0], &read_fds);

  // Write data to the pipe to trigger a read event.
  char buffer[1] = {'a'};
  int res = write(fds[1], buffer, sizeof(buffer));
  EXPECT_NE(res, -1);

  struct timeval timeout = {1, 0};  // 1 second
  int result = select(fds[0] + 1, &read_fds, NULL, NULL, &timeout);

  EXPECT_EQ(result, 1);
  EXPECT_TRUE(FD_ISSET(fds[0], &read_fds));

  close(fds[0]);
  close(fds[1]);
}

TEST(PosixSelectTest, WriteEvent) {
  int fds[2];
  CreatePipe(fds);
  fd_set write_fds;
  FD_ZERO(&write_fds);
  FD_SET(fds[1], &write_fds);

  struct timeval timeout = {1, 0};
  int result = select(fds[1] + 1, NULL, &write_fds, NULL, &timeout);

  EXPECT_EQ(result, 1);
  EXPECT_TRUE(FD_ISSET(fds[1], &write_fds));

  close(fds[0]);
  close(fds[1]);
}

TEST(PosixSelectTest, ZeroTimeoutReturnsImmediately) {
  int fds[2];
  CreatePipe(fds);
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(fds[0], &read_fds);

  struct timeval timeout = {0, 0};
  EXPECT_EQ(select(fds[0] + 1, &read_fds, NULL, NULL, &timeout), 0);

  close(fds[0]);
  close(fds[1]);
}

TEST(PosixSelectTest, InvalidTimeout) {
  // A timeout with a negative value is invalid.
  struct timeval timeout = {-1, 0};
  EXPECT_EQ(select(0, NULL, NULL, NULL, &timeout), -1);
  EXPECT_EQ(errno, EINVAL);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
