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
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "starboard/common/socket.h"
#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbPosixEpollTest : public ::testing::TestWithParam<int> {
 public:
  int GetAddressType() { return GetParam(); }
};

TEST(SbPosixEpollAddRemoveTest, SunnyDay) {
  int epfd = epoll_create(1);
  EXPECT_TRUE(epfd > 0);

  int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket >= 0);

  epoll_event epev;
  epev.events = EPOLLIN | EPOLLOUT;
  int add_val = epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &epev);
  EXPECT_TRUE(add_val == 0);

  EXPECT_TRUE(epoll_ctl(epfd, EPOLL_CTL_DEL, socket, &epev) == 0);

  EXPECT_TRUE(close(socket) == 0);
  EXPECT_TRUE(close(epfd) == 0);
}

TEST(SbPosixEpollAddTest, SunnyDayMany) {
  int epfd = epoll_create(1);
  EXPECT_TRUE(epfd > 0);

  const int kMany = kSbFileMaxOpen;
  std::vector<int> sockets(kMany, 0);

  epoll_event epev;
  epev.events = EPOLLIN | EPOLLOUT;

  for (int i = 0; i < kMany; ++i) {
    sockets[i] = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_TRUE(sockets[i] >= 0);
    EXPECT_TRUE(epoll_ctl(epfd, EPOLL_CTL_ADD, sockets[i], &epev) == 0);
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(close(sockets[i]) == 0);
  }

  EXPECT_TRUE(close(epfd) == 0);
}

TEST(SbPosixEpollAddTest, RainyDayAddToSameWaiter) {
  int epfd = epoll_create(1);
  EXPECT_TRUE(epfd > 0);

  int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket >= 0);
  epoll_event epev;
  epev.events = EPOLLIN | EPOLLOUT;

  // First add should succeed.
  EXPECT_TRUE(epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &epev) == 0);

  // Second add should fail.
  EXPECT_FALSE(epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &epev) == 0);

  // Remove should succeed.
  EXPECT_TRUE(epoll_ctl(epfd, EPOLL_CTL_DEL, socket, &epev) == 0);

  // Add after remove should succeed.
  EXPECT_TRUE(epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &epev) == 0);

  // Second add after remove should fail.
  EXPECT_FALSE(epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &epev) == 0);

  EXPECT_TRUE(close(socket) == 0);
  EXPECT_TRUE(close(epfd) == 0);
}

TEST(SbPosixEpollAddTest, RainyDayInvalidSocket) {
  int epfd = epoll_create(1);
  EXPECT_TRUE(epfd > 0);

  epoll_event epev;
  epev.events = EPOLLIN | EPOLLOUT;

  EXPECT_FALSE(epoll_ctl(epfd, EPOLL_CTL_ADD, -1, &epev) == 0);

  EXPECT_TRUE(close(epfd) == 0);
}

TEST(SbPosixEpollAddTest, RainyDayNoInterest) {
  int epfd = epoll_create(1);
  EXPECT_TRUE(epfd > 0);

  int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket >= 0);

  EXPECT_FALSE(epoll_ctl(epfd, EPOLL_CTL_ADD, socket, nullptr) == 0);

  EXPECT_TRUE(close(socket) == 0);
  EXPECT_TRUE(close(epfd) == 0);
}

TEST(SbPosixEpollAddTest, RainyDayRemoveSocket) {
  int epfd = epoll_create(1);
  EXPECT_TRUE(epfd > 0);

  int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket >= 0);
  epoll_event epev;
  epev.events = EPOLLIN | EPOLLOUT;

  // First remove should fail.
  EXPECT_FALSE(epoll_ctl(epfd, EPOLL_CTL_DEL, socket, &epev) == 0);

  // First add should succeed.
  EXPECT_TRUE(epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &epev) == 0);

  // Remove after add should succeed.
  EXPECT_TRUE(epoll_ctl(epfd, EPOLL_CTL_DEL, socket, &epev) == 0);

  // Remove after remove should succeed.
  EXPECT_FALSE(epoll_ctl(epfd, EPOLL_CTL_DEL, socket, &epev) == 0);

  // Add after second remove should succeed.
  EXPECT_TRUE(epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &epev) == 0);

  // Remove after add should succeed.
  EXPECT_TRUE(epoll_ctl(epfd, EPOLL_CTL_DEL, socket, &epev) == 0);

  EXPECT_TRUE(close(socket) == 0);
  EXPECT_TRUE(close(epfd) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
