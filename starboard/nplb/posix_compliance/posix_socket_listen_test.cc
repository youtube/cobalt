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

// SbSocketListen is Sunny Day tested in several other tests, so those won't be
// included here.

#if SB_API_VERSION >= 16

#if defined(_WIN32)
#include <errno.h>
#include <io.h>
#include <winsock2.h>
#else
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "starboard/common/socket.h"
#include "starboard/nplb/socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixSocketListenTest : public ::testing::TestWithParam<int> {
 public:
  int GetAddressType() { return GetParam(); }
  int GetSocketDomain() { return GetParam() == 0 ? AF_INET : AF_INET6; }
};

int PosixSocketListen(int socket_fd) {
  if (socket_fd < 0) {
    return -1;
  }
  return ::listen(socket_fd, kMaxConn);
}

int PosixCreateListeningTcpSocket(int domain,
                                  int type,
                                  int protocol,
                                  int port) {
  int socket_fd = PosixSocketCreate(domain, type, protocol);
  if (socket_fd < 0) {
    return -1;
  }

  struct sockaddr_in address = {0};
  address.sin_family = domain;
  address.sin_port = port;

  int result = PosixSocketBind(socket_fd, reinterpret_cast<sockaddr*>(&address),
                               sizeof(sockaddr));
  if (result != 0) {
    PosixSocketClose(socket_fd);
    return -1;
  }

  result = PosixSocketListen(socket_fd);
  if (result != 0) {
    PosixSocketClose(socket_fd);
    return -1;
  }

  return socket_fd;
}

TEST_F(PosixSocketListenTest, RainyDayInvalid) {
  int invalid_socket_fd = -1;
  EXPECT_FALSE(::listen(invalid_socket_fd, kMaxConn) == 0);
}

TEST_P(PosixSocketListenTest, SunnyDayUnbound) {
  int socket_fd =
      PosixSocketCreate(GetSocketDomain(), SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd >= 0);

  EXPECT_TRUE(PosixSocketListen(socket_fd) == 0);

  // Listening on an unbound socket should listen to a system-assigned port on
  // all local interfaces.
  struct sockaddr_in address = {0};
  socklen_t socklen;
  int result = 0;
  result =
      getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &socklen);
  if (result < 0) {
    EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
    return;
  }

  EXPECT_EQ(GetSocketDomain(), address.sin_family);
  EXPECT_NE(0, address.sin_port);
  EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
#endif  // SB_API_VERSION >= 16
