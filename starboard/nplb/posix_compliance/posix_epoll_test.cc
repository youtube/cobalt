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

constexpr int kMaxEvents = 10;
constexpr int kMaxBuf = 1024;

class PosixEpollTest : public testing::Test {
 public:
  void SetUp() override {
    // Create epoll object, arg doesn't matter as long as its  > 0
    epfd = epoll_create(1);
    ASSERT_GT(epfd, 0) << strerror(errno);

    // Create non-blocking socket for use in all tests
    socket_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    ASSERT_GE(socket_fd, 0) << strerror(errno);

    // Set epoll_event data
    epev.data.fd = socket_fd;
    epev.events = EPOLLIN | EPOLLOUT;

    // Get socket for use in tests similar to socket_helper.cc
    ASSERT_TRUE(
        PosixGetLocalAddressIPv4(reinterpret_cast<sockaddr*>(&address)) == 0 ||
        PosixGetLocalAddressIPv6(reinterpret_cast<sockaddr*>(&address)) == 0);
    address.sin6_port = htons(PosixGetPortNumberForTests());
  }

  void TearDown() override {
    EXPECT_EQ(close(socket_fd), 0) << strerror(errno);
    EXPECT_EQ(close(epfd), 0) << strerror(errno);
  }

 protected:
  int epfd;
  int socket_fd;
  epoll_event epev;
  sockaddr_in6 address = {};
  socklen_t add_len = sizeof(struct sockaddr_in);
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

TEST_F(PosixEpollTest, SunnyDayWaitTimeout) {
  struct epoll_event ev, events[kMaxEvents];
  int listen_sock, num_ready;

  // create listen socket, bind and listen on <port>
  listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_GE(listen_sock, 0);

  // set socket reusable
  const int on = 1;
  int result =
      setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  EXPECT_EQ(result, 0) << strerror(errno);

  // bind socket with local address
  result =
      bind(listen_sock, reinterpret_cast<struct sockaddr*>(&address), add_len);
  EXPECT_EQ(result, 0) << strerror(errno);

  result = listen(listen_sock, kMaxConn);
  EXPECT_EQ(result, 0) << strerror(errno);

  ev.events = EPOLLIN;
  ev.data.fd = listen_sock;
  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &ev), 0)
      << strerror(errno);

  // 5 second timeout for wait
  num_ready = epoll_wait(epfd, events, kMaxEvents, 5000);
  // expect nothing to be ready since we're not doing anything
  EXPECT_EQ(num_ready, 0);

  EXPECT_EQ(close(listen_sock), 0) << strerror(errno);
}

TEST_F(PosixEpollTest, SunnyDayWait) {
  struct sockaddr_in dest;

  char buffer[kMaxBuf];
  struct epoll_event events[kMaxEvents];
  int i, num_ready;

  // Add socket to epoll
  epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &epev);

  if (connect(socket_fd, (struct sockaddr*)&address, sizeof(address)) != 0) {
    EXPECT_EQ(errno, EINPROGRESS) << "Connection failed, not in progress";
  }

  // Wait for socket to connect
  num_ready = epoll_wait(epfd, events, kMaxEvents, 1000);
  for (i = 0; i < num_ready; i++) {
    if (events[i].events & EPOLLIN) {
      printf("Socket %d connected\n", events[i].data.fd);
    }
  }

  EXPECT_GT(num_ready, 0) << "No sockets connected after wait";
  int bytes_received = 0;

  char* send_buf = new char[kMaxBuf];
  char* receive_buf = new char[kMaxBuf];
  for (int i = 0; i < kMaxBuf; ++i) {
    send_buf[i] = static_cast<char>(i);
  }
  memset(receive_buf, 0, kMaxBuf);
  int bytes_sent = sendto(socket_fd, send_buf, kMaxBuf, kSendFlags, NULL, 0);

  // Wait to receive data from connected socket
  num_ready = epoll_wait(epfd, events, kMaxEvents, 1000);
  bool data_recieved = false;
  for (i = 0; i < num_ready; i++) {
    if (events[i].events & EPOLLIN) {
      SB_LOG(INFO) << "Socket " << events[i].data.fd << " receiving data";
      memset(buffer, 0, kMaxBuf);
      recvfrom(socket_fd, receive_buf, kMaxBuf, 0, 0, 0);
      SB_LOG(INFO) << "Received.";
      data_recieved = true;
    }
  }
  EXPECT_TRUE(data_recieved) << "No data read from socket";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
