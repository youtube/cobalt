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

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// A helper function to create a pipe.
// Returns true on success, false on failure.
// pipe_fds[0] is the read end, pipe_fds[1] is the write end.
bool CreatePipe(int pipe_fds[2], int flags = 0) {
  if (flags == 0) {
    return pipe(pipe_fds) == 0;
  }
  return pipe2(pipe_fds, flags) == 0;
}

// Test fixture for epoll tests
class PosixEpollTest : public ::testing::Test {
 protected:
  static const int kInvalidFd = -123;  // An fd value that is likely invalid.
  static const int kMaxEvents = 10;  // Maximum number of events for epoll_wait.
  static const int kShortTimeoutMs =
      50;  // A short timeout for non-blocking checks.
  static const int kModerateTimeoutMs =
      100;  // A moderate timeout for waiting for events.
};

// --- Tests for epoll_create1 ---
// epoll_create1 is epoll_create without the obsolete size argument.
class PosixEpollCreate1Tests : public PosixEpollTest {};

TEST_F(PosixEpollCreate1Tests, CreateSuccessfully) {
  int epfd = epoll_create1(0);
  ASSERT_NE(epfd, -1) << "epoll_create1(0) failed: " << strerror(errno);
  if (epfd != -1) {
    ASSERT_EQ(close(epfd), 0);
  }
}

TEST_F(PosixEpollCreate1Tests, CreateSuccessfullyWithCloExec) {
  int epfd = epoll_create1(EPOLL_CLOEXEC);
  ASSERT_NE(epfd, -1) << "epoll_create1(EPOLL_CLOEXEC) failed: "
                      << strerror(errno);

  if (epfd != -1) {
    int flags = fcntl(epfd, F_GETFD);
    ASSERT_NE(flags, -1) << "fcntl(F_GETFD) failed: " << strerror(errno);
    ASSERT_TRUE(flags & FD_CLOEXEC) << "FD_CLOEXEC was not set.";
    ASSERT_EQ(close(epfd), 0);
  }
}

TEST_F(PosixEpollCreate1Tests, ErrorInvalidFlags) {
  // An arbitrary invalid flag value.
  // EPOLL_CLOEXEC is 0x80000. We pick another bit not used by epoll_create1.
  const int kInvalidFlag = 0x1;
  int epfd = epoll_create1(kInvalidFlag);
  ASSERT_EQ(epfd, -1);
  ASSERT_EQ(errno, EINVAL);
  // No fd to close as creation failed.
}

/* The following error scenarios are untested for epoll_create1():
1. EMFILE (per-process limit of open FDs) is difficult to test reliably in a
unit test framework without affecting the environment or other tests. It would
require opening FDs until the limit is reached.
2. ENFILE (system-wide limit of open FDs) is even harder to test than EMFILE -
it would require opening FDs till the system wide limit is reached.
3. ENOMEM (out of memory) is not reliably testable in a unit test as it's hard
to deterministically exhaust kernel memory.
*/

}  // namespace
}  // namespace nplb
}  // namespace starboard
