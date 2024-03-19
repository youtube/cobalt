// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "starboard/common/log.h"
#include "starboard/nplb/socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixErrnoTest, AcceptInvalidSocket) {
  int invalid_socket_fd = -1;
  EXPECT_FALSE(accept(invalid_socket_fd, NULL, NULL) == 0);
  EXPECT_TRUE(errno == EBADF);
  SB_DLOG(INFO) << "Failed to accept invalid socket, errno = "
                << strerror(errno);
}

TEST(PosixErrnoTest, BindPrivilegedPort) {
  int socket_domain = AF_INET;
  int socket_type = SOCK_STREAM;
  int socket_protocol = IPPROTO_TCP;

  int socket_fd = socket(socket_domain, socket_type, socket_protocol);
  ASSERT_TRUE(socket_fd > 0);

  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(80);  // Try to bind to a privileged port
  address.sin_addr.s_addr = htonl(INADDR_ANY);

  int bind_result =
      bind(socket_fd, reinterpret_cast<struct sockaddr*>(&address),
           sizeof(struct sockaddr));
  EXPECT_TRUE(bind_result == -1);
  EXPECT_TRUE(errno == EACCES);
  SB_DLOG(INFO) << "Failed to bind, errno = " << strerror(errno);

  EXPECT_TRUE(close(socket_fd) == 0);
}

TEST(PosixErrnoTest, ConnectAddressRefused) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_TRUE(socket_fd > 0);

  sockaddr_in6 address = {};
#if SB_HAS(IPV6)
  EXPECT_TRUE(PosixGetLocalAddressiIPv4(
                  reinterpret_cast<struct sockaddr*>(&address)) == 0 ||
              PosixGetLocalAddressiIPv6(&address) == 0);
#else
  EXPECT_TRUE(
      PosixGetLocalAddressiIPv4(reinterpret_cast<sockaddr*>(&address)) == 0);
#endif

  // Attempt to connect to an address where we expect connection to be refused
  connect(socket_fd, (struct sockaddr*)&address, sizeof(address));

  EXPECT_TRUE(errno == ECONNREFUSED);
  SB_DLOG(INFO) << "Failed to connect, errno = " << strerror(errno);

  close(socket_fd);
}

TEST(PosixErrnoTest, SendBeforeConnect) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_TRUE(socket_fd > 0);

  char buffer[1024] = "test";
  ssize_t result = send(socket_fd, buffer, sizeof(buffer), 0);

  EXPECT_TRUE(result == -1 && errno == ENOTCONN);
  SB_DLOG(INFO) << "Failed to send, errno = " << strerror(errno);

  close(socket_fd);
}

TEST(PosixErrnoTest, ConnectToInvalidAddress) {
  int socket_domain = AF_INET;
  int socket_type = SOCK_STREAM;
  int socket_protocol = IPPROTO_TCP;

  int socket_fd = socket(socket_domain, socket_type, socket_protocol);
  ASSERT_TRUE(socket_fd > 0);

  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(12345);  // Try to connect to an invalid port
  address.sin_addr.s_addr = (((((250 << 8) | 43) << 8) | 244) << 8) | 18;

  int connect_result =
      connect(socket_fd, reinterpret_cast<struct sockaddr*>(&address),
              sizeof(struct sockaddr));
  EXPECT_TRUE(connect_result == -1);
  EXPECT_TRUE(errno == EADDRNOTAVAIL);
  SB_DLOG(INFO) << "Failed to connect, errno = " << strerror(errno);

  EXPECT_TRUE(close(socket_fd) == 0);
}
}  // namespace
}  // namespace nplb
}  // namespace starboard
