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
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "starboard/common/socket.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixEpollTest : public testing::Test {
 public:
  void SetUp() override {
    epfd = epoll_create(1);
    ASSERT_GT(epfd, 0) << strerror(errno);

    socket_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_GE(socket_fd, 0) << strerror(errno);

    epev.data.fd = socket_fd;
    epev.events = EPOLLIN | EPOLLOUT;
  }

  void TearDown() override {
    EXPECT_EQ(close(socket_fd), 0) << strerror(errno);
    EXPECT_EQ(close(epfd), 0) << strerror(errno);
  }

 protected:
  int epfd;
  int socket_fd;
  epoll_event epev;
};

TEST_F(PosixEpollTest, SunnyDayAddRemove) {
  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0)
      << strerror(errno);

  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0)
      << strerror(errno);
}

TEST_F(PosixEpollTest, SunnyDayMany) {
  const int kMany = kSbFileMaxOpen;
  std::vector<int> sockets(kMany, 0);

  for (int i = 0; i < kMany; ++i) {
    sockets[i] = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_GE(sockets[i], 0);
    EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, sockets[i], &epev), 0)
        << strerror(errno);
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_EQ(close(sockets[i]), 0);
  }
}

TEST_F(PosixEpollTest, RainyDayRemoveFromEmptyFails) {
  // First remove should fail.
  EXPECT_LT(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0);
}

TEST_F(PosixEpollTest, RainyDayDoubleAddFails) {
  // First add should succeed.
  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0)
      << strerror(errno);

  // Second add should fail.
  EXPECT_LT(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0);

  // Remove should succeed, to clear epoll queue for next test.
  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0)
      << strerror(errno);
}

TEST_F(PosixEpollTest, RainyDayDoubleRemoveFails) {
  // First add should succeed.
  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0)
      << strerror(errno);

  // Remove should succeed.
  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0)
      << strerror(errno);

  // Remove after remove should fail.
  EXPECT_LT(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0);
}

TEST_F(PosixEpollTest, RainyDayAddRemoveRepeated) {
  // First remove should fail.
  EXPECT_LT(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0);

  // First add should succeed.
  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0)
      << strerror(errno);

  // Second add should fail.
  EXPECT_LT(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0);

  // Remove should succeed.
  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0)
      << strerror(errno);

  // Remove after remove should fail.
  EXPECT_LT(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0);

  // Add after remove should succeed.
  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0)
      << strerror(errno);

  // Second add after remove should fail.
  EXPECT_LT(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev), 0);

  // Remove should succeed, to clear epoll queue for next test.
  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, &epev), 0)
      << strerror(errno);
}

TEST_F(PosixEpollTest, RainyDayInvalidSocket) {
  EXPECT_LT(epoll_ctl(epfd, EPOLL_CTL_ADD, -1, &epev), 0);
}

TEST_F(PosixEpollTest, RainyDayNoInterest) {
  EXPECT_LT(epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, nullptr), 0);
}

TEST_F(PosixEpollTest, SunnyDayWait) {
#define MAX_EVENTS 10
  struct epoll_event ev, events[MAX_EVENTS];
  int listen_sock, conn_sock, nfds;

  // create listen socket, bind and listen on <port>
  listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_GE(listen_sock, 0);

  // set socket reusable
  const int on = 1;
  int result =
      setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  EXPECT_EQ(result, 0);

  // bind socket with local address
  sockaddr_in6 address = {};
  EXPECT_TRUE(
      PosixGetLocalAddressIPv4(reinterpret_cast<sockaddr*>(&address)) == 0 ||
      PosixGetLocalAddressIPv6(reinterpret_cast<sockaddr*>(&address)) == 0);
  address.sin6_port = htons(PosixGetPortNumberForTests());

  socklen_t add_len = sizeof(struct sockaddr_in);
  result =
      bind(listen_sock, reinterpret_cast<struct sockaddr*>(&address), add_len);
  EXPECT_EQ(result, 0);

  result = listen(listen_sock, kMaxConn);
  EXPECT_EQ(result, 0);

  ev.events = EPOLLIN;
  ev.data.fd = listen_sock;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
    perror("epoll_ctl: listen_sock");
    exit(EXIT_FAILURE);
  }

  // placeholder value in for loop to avoid infinite loop
  for (int i = 0; i < 5; i++) {
    nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }

    for (int n = 0; n < nfds; ++n) {
      if (events[n].data.fd == listen_sock) {
        conn_sock =
            accept(listen_sock, reinterpret_cast<struct sockaddr*>(&address),
                   &add_len);
        if (conn_sock == -1) {
          perror("accept");
          exit(EXIT_FAILURE);
        }
        // set non-blocking
        fcntl(conn_sock, F_SETFL, O_NONBLOCK);
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = conn_sock;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
          perror("epoll_ctl: conn_sock");
          exit(EXIT_FAILURE);
        }
      } else {
        // do something
        break;
      }
    }
    break;
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
