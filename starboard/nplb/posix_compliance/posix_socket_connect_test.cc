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
#if SB_API_VERSION >= 16

#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/common/time.h"
#include "starboard/nplb/socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  int result = -1;
  // create socket
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd >= 0);

  EXPECT_FALSE(connect(socket_fd, NULL, 0) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
#endif  // SB_API_VERSION >= 16
