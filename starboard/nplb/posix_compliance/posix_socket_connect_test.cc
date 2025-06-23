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

// Here we are not trying to do anything fancy, just to really sanity check that
// this is hooked up to something.

#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixSocketConnectTest, RainyDayNullNull) {
  int invalid_socket = -1;
  EXPECT_FALSE(connect(invalid_socket, NULL, 0) == 0);
}

TEST(PosixSocketConnectTest, RainyDayNullSocket) {
  sockaddr_in address = {};

  address.sin_family = AF_INET;
  address.sin_port = 2048;

  int invalid_socket_fd = -1;
  EXPECT_FALSE(connect(invalid_socket_fd, reinterpret_cast<sockaddr*>(&address),
                       sizeof(sockaddr_in)) == 0);
}

TEST(PosixSocketConnectTest, RainyDayNullAddress) {
  // create socket
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd >= 0);

  EXPECT_FALSE(connect(socket_fd, NULL, 0) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}

TEST(PosixSocketConnectTest, SunnyDayConnectToServer) {
  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  int result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(PosixGetPortNumberForTests()), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);
  ASSERT_TRUE(result == 0);
  EXPECT_TRUE(close(listen_socket_fd) == 0);
  EXPECT_TRUE(close(client_socket_fd) == 0);
  EXPECT_TRUE(close(server_socket_fd) == 0);
}

TEST(PosixSocketConnectTest, SunnyDayConnectToServerAgain) {
  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  int result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(PosixGetPortNumberForTests()), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);
  ASSERT_TRUE(result == 0);
  EXPECT_TRUE(close(listen_socket_fd) == 0);
  EXPECT_TRUE(close(client_socket_fd) == 0);
  EXPECT_TRUE(close(server_socket_fd) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
