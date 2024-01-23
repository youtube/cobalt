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

// The accept SunnyDay case is tested as a subset of at least one other test
// case, so it is not included redundantly here.
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
#include "starboard/common/time.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/shared/posix/handle_eintr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixSocketAcceptTest : public ::testing::TestWithParam<int> {
 public:
  int GetAddressType() { return GetParam(); }
  int GetSocketDomain() { return GetParam() == 0 ? AF_INET : AF_INET6; }
};

int PosixSocketAccept(int socket_fd) {
  if (socket_fd < 0) {
    errno = EBADF;
    return -1;
  }

  int accept_socket_fd = HANDLE_EINTR(accept(socket_fd, NULL, NULL));
  if (accept_socket_fd < 0) {
    // If we didn't get a socket, it should be pending.
    if (!(errno == EINPROGRESS || errno == EAGAIN
#if EWOULDBLOCK != EAGAIN
          || errno == EWOULDBLOCK
#endif
          )) {
      return -1;
    }
  }

  // Adopt the newly accepted socket.
  return accept_socket_fd;
}

int PosixSocketAcceptBySpinning(int server_socket_fd, int64_t timeout) {
  int64_t start = CurrentMonotonicTime();
  while (true) {
    int accepted_socket_fd = PosixSocketAccept(server_socket_fd);
    if (accepted_socket_fd >= 0) {
      return accepted_socket_fd;
    }

    // If we didn't get a socket, it should be pending.
    if (!(errno == EINPROGRESS || errno == EAGAIN
#if EWOULDBLOCK != EAGAIN
          || errno == EWOULDBLOCK
#endif
          )) {
      return -1;
    }

    // Check if we have passed our timeout.
    if (CurrentMonotonicTime() - start >= timeout) {
      break;
    }

    // Just being polite.
    SbThreadYield();
  }

  return -1;
}

TEST_P(PosixSocketAcceptTest, RainyDayInvalidSocket) {
  int invalid_socket_fd = -1;
  EXPECT_FALSE(::accept(invalid_socket_fd, NULL, NULL) == 0);
}

TEST_P(PosixSocketAcceptTest, RainyDayNoConnection) {
  // Set up a socket to listen.
  int server_socket_fd = PosixCreateListeningTcpSocket(
      GetSocketDomain(), SOCK_STREAM, IPPROTO_TCP, GetPortNumberForTests());
  ASSERT_TRUE(server_socket_fd > 0);

  // Don't create a socket to connect to it.
  // Spin briefly to ensure that it won't accept.
  int64_t start = CurrentMonotonicTime();
  int accepted_socket_fd = 0;
  while (true) {
    accepted_socket_fd = HANDLE_EINTR(accept(server_socket_fd, NULL, NULL));

    // If we didn't get a socket, it should be pending.
    EXPECT_TRUE(errno == EINPROGRESS || errno == EAGAIN
#if EWOULDBLOCK != EAGAIN
                || errno == EWOULDBLOCK
#endif
    );

    // Check if we have passed our timeout.
    if (CurrentMonotonicTime() - start >= kSocketTimeout) {
      break;
    }

    // Just being polite.
    SbThreadYield();
  }

  PosixSocketClose(accepted_socket_fd);
  EXPECT_TRUE(PosixSocketClose(server_socket_fd) == 0);
}

TEST_P(PosixSocketAcceptTest, RainyDayNotBound) {
  // Set up a socket, but don't Bind or Listen.
  int socket_domain, socket_type, socket_protocol;
  socket_domain = GetSocketDomain();
  socket_type = SOCK_STREAM;
  socket_protocol = IPPROTO_TCP;

  int socket_fd = ::socket(socket_domain, socket_type, socket_protocol);
  ASSERT_TRUE(socket_fd > 0);

  // Set reuse address to true
  int result = PosixSocketSetReuseAddress(socket_fd, true);
  if (result != 0) {
    EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
    return;
  }

  // Accept should result in an error.
  EXPECT_FALSE(::accept(socket_fd, NULL, NULL) == 0);

  EXPECT_FALSE(errno == 0);
  EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
}

TEST_P(PosixSocketAcceptTest, RainyDayNotListening) {
  // Set up a socket, but don't Bind or Listen.
  int socket_domain, socket_type, socket_protocol;
  socket_domain = GetSocketDomain();
  socket_type = SOCK_STREAM;
  socket_protocol = IPPROTO_TCP;

  int socket_fd =
      PosixSocketCreate(socket_domain, socket_type, socket_protocol);
  ASSERT_TRUE(socket_fd > 0);

  // Accept should result in an error.
  EXPECT_FALSE(::accept(socket_fd, NULL, NULL) == 0);

  EXPECT_FALSE(errno == 0);
  EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
#endif  // SB_API_VERSION >= 16
