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

/* The following error scenarios are untested for epoll_ctl():
1. ENOMEM (out of memory for control operation) is not reliably testable.
2. ENOSPC (max_user_watches limit) is not reliably testable in a unit test.
*/

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "starboard/nplb/posix_compliance/posix_epoll_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixEpollCtlTests : public PosixEpollTest {
 protected:
  int epfd_ = -1;
  int pipe_fds_[2] = {-1, -1};  // pipe_fds_[0] for read, pipe_fds_[1] for write

  void SetUp() override {
    epfd_ = epoll_create1(0);
    ASSERT_NE(epfd_, -1) << "Failed to create epoll instance in SetUp: "
                         << strerror(errno);
    ASSERT_TRUE(CreatePipe(pipe_fds_, O_NONBLOCK))
        << "Failed to create pipe in SetUp: " << strerror(errno);
  }

  void TearDown() override {
    if (epfd_ >= 0) {
      close(epfd_);
      epfd_ = -1;
    }
    if (pipe_fds_[0] >= 0) {
      close(pipe_fds_[0]);
      pipe_fds_[0] = -1;
    }
    if (pipe_fds_[1] >= 0) {
      close(pipe_fds_[1]);
      pipe_fds_[1] = -1;
    }
  }
};

TEST_F(PosixEpollCtlTests, AddFdSuccessfully) {
  struct epoll_event event = {};
  event.events = EPOLLIN | EPOLLOUT;
  event.data.fd = pipe_fds_[0];

  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_ADD, pipe_fds_[0], &event), 0)
      << "EPOLL_CTL_ADD failed: " << strerror(errno);
}

TEST_F(PosixEpollCtlTests, ModifyFdSuccessfully) {
  struct epoll_event event = {};
  event.events = EPOLLIN;
  event.data.fd = pipe_fds_[0];

  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_ADD, pipe_fds_[0], &event), 0)
      << "EPOLL_CTL_ADD for MOD test failed: " << strerror(errno);

  event.events = EPOLLOUT;  // Modify to only EPOLLOUT
  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_MOD, pipe_fds_[0], &event), 0)
      << "EPOLL_CTL_MOD failed: " << strerror(errno);
}

TEST_F(PosixEpollCtlTests, DeleteFdSuccessfully) {
  struct epoll_event event = {};
  event.events = EPOLLIN;
  event.data.fd = pipe_fds_[0];

  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_ADD, pipe_fds_[0], &event), 0)
      << "EPOLL_CTL_ADD for DEL test failed: " << strerror(errno);

  // The 'event' parameter is ignored for EPOLL_CTL_DEL but must be non-NULL on
  // older kernels. Modern kernels (>= 2.6.9) allow NULL. For compliance, pass a
  // valid pointer.
  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_DEL, pipe_fds_[0], &event), 0)
      << "EPOLL_CTL_DEL failed: " << strerror(errno);
}

TEST_F(PosixEpollCtlTests, ErrorBadEpfd) {
  struct epoll_event event = {};
  event.events = EPOLLIN;
  event.data.fd = pipe_fds_[0];

  ASSERT_EQ(epoll_ctl(kInvalidFd, EPOLL_CTL_ADD, pipe_fds_[0], &event), -1);
  ASSERT_EQ(errno, EBADF);
}

TEST_F(PosixEpollCtlTests, ErrorBadFdOnAdd) {
  struct epoll_event event = {};
  event.events = EPOLLIN;
  event.data.fd = kInvalidFd;

  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_ADD, kInvalidFd, &event), -1);
  ASSERT_EQ(errno, EBADF);
}

TEST_F(PosixEpollCtlTests, ErrorBadFdOnMod) {
  struct epoll_event event = {};
  event.events = EPOLLIN;
  event.data.fd = kInvalidFd;  // This is the FD we are trying to MOD.

  // First, add a valid fd to have something in the epoll set.
  struct epoll_event valid_event = {};
  valid_event.events = EPOLLIN;
  valid_event.data.fd = pipe_fds_[0];
  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_ADD, pipe_fds_[0], &valid_event), 0);

  // Try to MOD using an fd that is itself invalid.
  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_MOD, kInvalidFd, &event), -1);
  ASSERT_EQ(errno, EBADF);
}

TEST_F(PosixEpollCtlTests, ErrorAlreadyExistsOnAdd) {
  struct epoll_event event = {};
  event.events = EPOLLIN;
  event.data.fd = pipe_fds_[0];

  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_ADD, pipe_fds_[0], &event), 0)
      << "First EPOLL_CTL_ADD failed: " << strerror(errno);
  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_ADD, pipe_fds_[0], &event), -1);
  ASSERT_EQ(errno, EEXIST);
}

TEST_F(PosixEpollCtlTests, ErrorEpfdIsNotEpollFd) {
  struct epoll_event event = {};
  event.events = EPOLLIN;
  event.data.fd = pipe_fds_[0];

  // Use one end of the pipe as a non-epoll fd for epfd argument
  ASSERT_EQ(epoll_ctl(pipe_fds_[1], EPOLL_CTL_ADD, pipe_fds_[0], &event), -1);
  ASSERT_EQ(errno, EINVAL);
}

TEST_F(PosixEpollCtlTests, ErrorFdIsSameAsEpfd) {
  struct epoll_event event = {};
  event.events = EPOLLIN;
  event.data.fd = epfd_;  // Trying to add epfd to itself

  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_ADD, epfd_, &event), -1);
  ASSERT_EQ(errno, EINVAL);
}

TEST_F(PosixEpollCtlTests, ErrorInvalidOperation) {
  struct epoll_event event = {};
  event.events = EPOLLIN;
  event.data.fd = pipe_fds_[0];
  // An arbitrary integer that isn't a valid EPOLL_CTL operation.
  const int kInvalidOp = 99;

  ASSERT_EQ(epoll_ctl(epfd_, kInvalidOp, pipe_fds_[0], &event), -1);
  ASSERT_EQ(errno, EINVAL);
}

TEST_F(PosixEpollCtlTests, ErrorLoopBetweenTwoEpollInstances) {
  int epfd2 = epoll_create1(0);
  ASSERT_NE(epfd2, -1)
      << "Failed to create second epoll instance for ELOOP test: "
      << strerror(errno);

  // Try A watches B, B watches A
  struct epoll_event ev_a_watches_b = {};
  ev_a_watches_b.events = EPOLLIN;
  ev_a_watches_b.data.fd = epfd2;  // epfd_ will watch epfd2
  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_ADD, epfd2, &ev_a_watches_b), 0)
      << "epoll_ctl add epfd2 to epfd_ failed: " << strerror(errno);

  struct epoll_event ev_b_watches_a = {};
  ev_b_watches_a.events = EPOLLIN;
  ev_b_watches_a.data.fd = epfd_;  // epfd2 tries to watch epfd_
  ASSERT_EQ(epoll_ctl(epfd2, EPOLL_CTL_ADD, epfd_, &ev_b_watches_a), -1);
  ASSERT_EQ(errno, ELOOP);

  // Clean up: remove epfd2 from epfd_
  epoll_ctl(epfd_, EPOLL_CTL_DEL, epfd2, &ev_a_watches_b);
  if (epfd2 >= 0) {
    close(epfd2);
  }
}

TEST_F(PosixEpollCtlTests, ErrorFdNotRegisteredOnMod) {
  struct epoll_event event = {};
  event.events = EPOLLIN;
  event.data.fd = pipe_fds_[0];  // This fd is valid but not yet added

  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_MOD, pipe_fds_[0], &event), -1);
  ASSERT_EQ(errno, ENOENT);
}

TEST_F(PosixEpollCtlTests, ErrorFdNotRegisteredOnDel) {
  struct epoll_event event = {};  // Dummy event for DEL
  event.events = EPOLLIN;
  event.data.fd = pipe_fds_[0];  // This fd is valid but not yet added

  ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_DEL, pipe_fds_[0], &event), -1);
  ASSERT_EQ(errno, ENOENT);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
