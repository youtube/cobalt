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

#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "starboard/common/socket.h"
#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixEpollTest : public testing::Test {
 public:
  void SetUp() override {
    epfd = epoll_create(1);
    EXPECT_GT(epfd, 0);

    socket_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_GE(socket_fd, 0);

    epev.events = EPOLLIN | EPOLLOUT;
  }

  void TearDown() override {
    EXPECT_EQ(close(socket_fd), 0);
    EXPECT_EQ(close(epfd), 0);
  }

 protected:
  int epfd;
  int socket_fd;
  epoll_event epev;
};

TEST_F(PosixEpollTest, SunnyDayAddRemove) {
  ASSERT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0);

  ASSERT_EQ(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0);
}

TEST_F(PosixEpollTest, SunnyDayMany) {
  const int kMany = kSbFileMaxOpen;
  std::vector<int> sockets(kMany, 0);

  for (int i = 0; i < kMany; ++i) {
    sockets[i] = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_GE(sockets[i], 0);
    EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, sockets[i], &epev), 0);
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_EQ(close(sockets[i]), 0);
  }
}

TEST_F(PosixEpollTest, RainyDayAddToSameWaiter) {
  // First remove should fail.
  ASSERT_NE(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0);

  // First add should succeed.
  ASSERT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0);

  // Second add should fail.
  ASSERT_NE(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0);

  // Remove should succeed.
  ASSERT_EQ(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0);

  // Remove after remove should fail.
  ASSERT_NE(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0);

  // Add after remove should succeed.
  ASSERT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0);

  // Second add after remove should fail.
  ASSERT_NE(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0);

  // Remove should succeed, to clear epoll queue for next test.
  ASSERT_EQ(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0);
}

TEST_F(PosixEpollTest, RainyDayInvalidSocket) {
  ASSERT_NE(epoll_ctl(epfd, EPOLL_CTL_ADD, -1, &epev), 0);
}

TEST_F(PosixEpollTest, RainyDayNoInterest) {
  ASSERT_NE(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, nullptr), 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
