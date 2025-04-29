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
    // Create epoll object, arg doesn't matter as long as it is > 0.
    epfd = epoll_create(1);
    ASSERT_GT(epfd, 0) << strerror(errno);

    // Create non-blocking socket for use in all tests.
    socket_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    ASSERT_GE(socket_fd, 0) << strerror(errno);

    // Set epoll_event data.
    epev.data.fd = socket_fd;
    epev.events = EPOLLIN | EPOLLOUT;

    // Get socket for use in tests similar to socket_helper.cc.
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
  // TODO(b/412696447): Correct member variable style.
  int timeout = 1000;
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
    EXPECT_GE(sockets[i], 0) << strerror(errno);
    EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, sockets[i], &epev), 0)
        << strerror(errno);
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_EQ(close(sockets[i]), 0) << strerror(errno);
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
  // Create listen socket, bind and listen on <port>.
  int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_GE(listen_sock, 0);

  // Set socket to be reusable.
  const int on = 1;
  int result =
      setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  EXPECT_EQ(result, 0) << strerror(errno);

  // Bind socket with local address.
  result =
      bind(listen_sock, reinterpret_cast<struct sockaddr*>(&address), add_len);
  EXPECT_EQ(result, 0) << strerror(errno);

  result = listen(listen_sock, kMaxConn);
  EXPECT_EQ(result, 0) << strerror(errno);

  struct epoll_event ev, events[kMaxEvents];
  ev.events = EPOLLIN;
  ev.data.fd = listen_sock;
  EXPECT_EQ(epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &ev), 0)
      << strerror(errno);

  // 1 second timeout for wait.
  int num_ready = epoll_wait(epfd, events, kMaxEvents, timeout);
  // Expect nothing to be ready since we're not doing anything.
  EXPECT_EQ(num_ready, 0);

  EXPECT_EQ(close(listen_sock), 0) << strerror(errno);
}

TEST_F(PosixEpollTest, ReceiveWithWait) {
  const int kBufSize = 256 * 1024;
  const int kSockBufSize = kBufSize / 8;
  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  int result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(PosixGetPortNumberForTests()), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);

  // Let's set the buffers small to create partial reads and writes.
  PosixSocketSetReceiveBufferSize(client_socket_fd, kSockBufSize);
  PosixSocketSetReceiveBufferSize(server_socket_fd, kSockBufSize);
  PosixSocketSetSendBufferSize(client_socket_fd, kSockBufSize);
  PosixSocketSetSendBufferSize(server_socket_fd, kSockBufSize);

  // Create the buffers and fill the send buffer with a pattern, the receive
  // buffer with zeros.
  char* send_buf = new char[kBufSize];
  char* receive_buf = new char[kBufSize];
  for (int i = 0; i < kBufSize; ++i) {
    send_buf[i] = static_cast<char>(i);
    receive_buf[i] = 0;
  }

  // Send from server to client.
  int send_total = 0;
  int receive_total = 0;
  epoll_ctl(epfd, EPOLL_CTL_ADD, server_socket_fd, &epev);
  epoll_ctl(epfd, EPOLL_CTL_ADD, client_socket_fd, &epev);
  int num_ready = 0;
  struct epoll_event events[kMaxEvents];
  num_ready = epoll_wait(epfd, events, kMaxEvents, timeout);

  for (int i = 0; i < num_ready; i++) {
    if (events[i].events & EPOLLIN && events[i].data.fd == server_socket_fd) {
      while (send_total < kBufSize) {
        int bytes_sent = send(server_socket_fd, send_buf + send_total,
                              kBufSize - send_total, kSendFlags);
        if (bytes_sent < 0) {
          EXPECT_EQ(errno, EINPROGRESS) << "Send failed, not in progress";
          bytes_sent = 0;
        }

        send_total += bytes_sent;
      }
    }
  }

  num_ready = epoll_wait(epfd, events, kMaxEvents, timeout);
  for (int i = 0; i < num_ready; i++) {
    if (events[i].events & EPOLLOUT && events[i].data.fd == client_socket_fd) {
      while (receive_total < kBufSize) {
        int bytes_received = recv(client_socket_fd, receive_buf + receive_total,
                                  kBufSize - receive_total, 0);

        if (bytes_received < 0) {
          EXPECT_EQ(errno, EINPROGRESS) << "Receive failed, not in progress";
          bytes_received = 0;
        }

        receive_total += bytes_received;
      }
    }
  }

  EXPECT_GE(receive_total, 0);
  EXPECT_GE(send_total, 0);

  EXPECT_TRUE(close(server_socket_fd) == 0);
  EXPECT_TRUE(close(client_socket_fd) == 0);
  EXPECT_TRUE(close(listen_socket_fd) == 0);
  delete[] send_buf;
  delete[] receive_buf;
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
