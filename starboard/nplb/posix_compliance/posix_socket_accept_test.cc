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

TEST(PosixSocketAcceptTest, RainyDayInvalidSocket) {
  int invalid_socket_fd = -1;
  EXPECT_FALSE(accept(invalid_socket_fd, NULL, NULL) == 0);
}

TEST(PosixSocketAcceptTest, RainyDayNoConnection) {
  // Set up a socket to listen.
  int socket_listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  int result = -1;
  ASSERT_TRUE(socket_listen_fd >= 0);

  struct sockaddr_in address = {0};
  EXPECT_TRUE(result =
                  bind(socket_listen_fd, reinterpret_cast<sockaddr*>(&address),
                       sizeof(sockaddr) == 0));
  if (result != 0) {
    close(socket_listen_fd);
    return;
  }

  EXPECT_TRUE(result = listen(socket_listen_fd, kMaxConn) == 0);
  if (result != 0) {
    close(socket_listen_fd);
    return;
  }

  // Don't create a socket to connect to it.
  // Spin briefly to ensure that it won't accept.
  int64_t start = CurrentMonotonicTime();
  int accepted_socket_fd = 0;
  while (true) {
    accepted_socket_fd = accept(socket_listen_fd, NULL, NULL);

    // If we didn't get a socket, it should be pending.
#if EWOULDBLOCK != EAGAIN
    EXPECT_TRUE(errno == EINPROGRESS || errno == EAGAIN ||
                errno == EWOULDBLOCK);
#else
    EXPECT_TRUE(errno == EINPROGRESS || errno == EAGAIN);
#endif

    // Check if we have passed our timeout.
    if (CurrentMonotonicTime() - start >= kSocketTimeout) {
      break;
    }

    // Just being polite.
    SbThreadYield();
  }
  EXPECT_FALSE(accepted_socket_fd >= 0);

  close(accepted_socket_fd);
  EXPECT_TRUE(close(socket_listen_fd) == 0);
}

TEST(PosixSocketAcceptTest, RainyDayNotBound) {
  // Set up a socket, but don't Bind or Listen.
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  int result = -1;
  ASSERT_TRUE(socket_fd >= 0);

  // Accept should result in an error.
  EXPECT_FALSE(accept(socket_fd, NULL, NULL) == 0);

  EXPECT_TRUE(close(socket_fd) == 0);
}

TEST(PosixSocketAcceptTest, RainyDayNotListening) {
  // Set up a socket, bind but don't Listen.
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  int result = -1;
  ASSERT_TRUE(socket_fd >= 0);

  struct sockaddr_in address = {0};
  EXPECT_TRUE(result = bind(socket_fd, reinterpret_cast<sockaddr*>(&address),
                            sizeof(sockaddr) == 0));
  if (result != 0) {
    close(socket_fd);
    return;
  }

  // Accept should result in an error.
  EXPECT_FALSE(accept(socket_fd, NULL, NULL) == 0);

  EXPECT_TRUE(close(socket_fd) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
#endif  // SB_API_VERSION >= 16
