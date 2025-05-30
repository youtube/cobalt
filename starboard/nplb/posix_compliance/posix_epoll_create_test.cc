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
This contains tests for epoll_create() and epoll_create1(). epoll_create takes
a size argument, which is disregarded as long as it is greater than zero.
epoll_create1() takes a flags argument. When flags are 0, epoll_create1() is
the same as epoll_create().
The following error scenarios are untested for epoll_create1():
1. EMFILE (per-process limit of open FDs) is difficult to test reliably in a
unit test framework without affecting the environment or other tests. It would
require opening FDs until the limit is reached.
2. ENFILE (system-wide limit of open FDs) is even harder to test than EMFILE -
it would require opening FDs till the system wide limit is reached.
3. ENOMEM (out of memory) is not reliably testable in a unit test as it's hard
to deterministically exhaust kernel memory.
These are also untested for epoll_create() for the same reasons.
*/

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "starboard/nplb/posix_compliance/posix_epoll_test_helper.cc"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixEpollCreateTests, CreatesValidFileDescriptorOnSuccess) {
  int epoll_fd = epoll_create(1);

  ASSERT_GE(epoll_fd, 0) << "epoll_create(1) failed: " << strerror(errno);
  ASSERT_EQ(close(epoll_fd), 0) << "close failed: " << strerror(errno);
}

TEST(PosixEpollCreateTests, FailsWithEINVALWhenSizeIsZero) {
  int epoll_fd = epoll_create(0);

  ASSERT_EQ(epoll_fd, -1);
  ASSERT_EQ(errno, EINVAL);
}

TEST(PosixEpollCreateTests, FailsWithEINVALWhenSizeIsNegative) {
  int epoll_fd = epoll_create(-1);

  ASSERT_EQ(epoll_fd, -1);
  ASSERT_EQ(errno, EINVAL);
}

TEST(PosixEpollCreate1Tests, CreateSuccessfully) {
  int epfd = epoll_create1(0);
  ASSERT_NE(epfd, -1) << "epoll_create1(0) failed: " << strerror(errno);
  ASSERT_EQ(close(epfd), 0) << "close failed: " << strerror(errno);
}

TEST(PosixEpollCreate1Tests, CreateSuccessfullyWithCloExec) {
  int epfd = epoll_create1(EPOLL_CLOEXEC);
  ASSERT_NE(epfd, -1) << "epoll_create1(EPOLL_CLOEXEC) failed: "
                      << strerror(errno);

  int flags = fcntl(epfd, F_GETFD);
  ASSERT_NE(flags, -1) << "fcntl(F_GETFD) failed: " << strerror(errno);
  ASSERT_TRUE(flags & FD_CLOEXEC) << "FD_CLOEXEC was not set.";
  ASSERT_EQ(close(epfd), 0) << "close failed: " << strerror(errno);
}

TEST(PosixEpollCreate1Tests, ErrorInvalidFlags) {
  // An arbitrary invalid flag value.
  // EPOLL_CLOEXEC is 0x80000. We pick another bit not used by epoll_create1.
  const int kInvalidFlag = 0x1;
  int epfd = epoll_create1(kInvalidFlag);
  ASSERT_EQ(epfd, -1);
  ASSERT_EQ(errno, EINVAL);
  // No fd to close as creation failed.
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
