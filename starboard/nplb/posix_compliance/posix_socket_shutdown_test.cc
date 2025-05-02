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

// Here we are not trying to do anything fancy, just to really sanity check that
// this is hooked up to something.

#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"

namespace starboard {
namespace nplb {
namespace {

// Shutdown disables further receive operations.
TEST(PosixSocketShutdownTest, ShutdownRD) {
  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  const int result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(PosixGetPortNumberForTests()), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);
  ASSERT_EQ(result, 0);
  ASSERT_EQ(shutdown(listen_socket_fd, SHUT_RD), 0);
  ASSERT_EQ(shutdown(client_socket_fd, SHUT_RD), 0);
  ASSERT_EQ(shutdown(server_socket_fd, SHUT_RD), 0);
  ASSERT_EQ(close(listen_socket_fd), 0);
  ASSERT_EQ(close(client_socket_fd), 0);
  ASSERT_EQ(close(server_socket_fd), 0);
}

// Shutdown disables further send operations.
TEST(PosixSocketShutdownTest, ShutdownWR) {
  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  const int result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(PosixGetPortNumberForTests()), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);
  ASSERT_EQ(result, 0);
  ASSERT_EQ(shutdown(listen_socket_fd, SHUT_WR), 0);
  ASSERT_EQ(shutdown(client_socket_fd, SHUT_WR), 0);
  ASSERT_EQ(shutdown(server_socket_fd, SHUT_WR), 0);
  ASSERT_EQ(close(listen_socket_fd), 0);
  ASSERT_EQ(close(client_socket_fd), 0);
  ASSERT_EQ(close(server_socket_fd), 0);
}

// Shutdown disables further send and receive operations.
TEST(PosixSocketShutdownTest, ShutdownRDWR) {
  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  const int result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(PosixGetPortNumberForTests()), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);
  ASSERT_EQ(result, 0);
  ASSERT_EQ(shutdown(listen_socket_fd, SHUT_RDWR), 0);
  ASSERT_EQ(shutdown(client_socket_fd, SHUT_RDWR), 0);
  ASSERT_EQ(shutdown(server_socket_fd, SHUT_RDWR), 0);
  ASSERT_EQ(close(listen_socket_fd), 0);
  ASSERT_EQ(close(client_socket_fd), 0);
  ASSERT_EQ(close(server_socket_fd), 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
