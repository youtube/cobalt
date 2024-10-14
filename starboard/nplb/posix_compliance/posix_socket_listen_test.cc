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

TEST(PosixSocketListenTest, RainyDayInvalid) {
  int invalid_socket_fd = -1;
  EXPECT_FALSE(listen(invalid_socket_fd, kMaxConn) == 0);
}

TEST(PosixSocketListenTest, SunnyDayUnbound) {
  int result = -1;
  // create socket
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd >= 0);

  // set socket reusable
  const int on = 1;
  result = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  EXPECT_TRUE(result == 0);
  if (result != 0) {
    close(socket_fd);
    return;
  }

  // bind socket with local address
  sockaddr_in6 address = {};
  EXPECT_TRUE(
      PosixGetLocalAddressIPv4(reinterpret_cast<sockaddr*>(&address)) == 0 ||
      PosixGetLocalAddressIPv6(reinterpret_cast<sockaddr*>(&address)) == 0);
  address.sin6_port = htons(GetPortNumberForTests());

  result =
      bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr));
  EXPECT_TRUE(result == 0);
  if (result != 0) {
    close(socket_fd);
    return;
  }

  EXPECT_TRUE(result = listen(socket_fd, kMaxConn) == 0);
  if (result != 0) {
    close(socket_fd);
    return;
  }

  // Listening on an unbound socket should listen to a system-assigned port on
  // all local interfaces.
  socklen_t socklen;
  sockaddr_in addr_in = {0};
  result =
      getsockname(socket_fd, reinterpret_cast<sockaddr*>(&addr_in), &socklen);
  if (result < 0) {
    close(socket_fd);
    return;
  }

  EXPECT_EQ(AF_INET, addr_in.sin_family);
  EXPECT_NE(0, addr_in.sin_port);
  EXPECT_TRUE(close(socket_fd) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
