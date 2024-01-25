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

#if SB_API_VERSION >= 16

#if defined(_WIN32)
// WinSock includes need to be in particular order
// clang-format off
#include <winsock2.h>
#include <mswsock.h>
#include <WS2tcpip.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif
#include <unistd.h>

#include "starboard/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixSocketSetOptionsTest : public ::testing::TestWithParam<int> {
 public:
  int GetSocketDomain() { return GetParam(); }
};

TEST_P(PosixSocketSetOptionsTest, TryThemAllTCP) {
#if defined(_WIN32)
  int socket_fd =
      WSASocketW(GetSocketDomain(), SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
#else
  int socket_fd = socket(GetSocketDomain(), SOCK_STREAM, IPPROTO_TCP);
#endif

  int true_val = 1;
  EXPECT_EQ(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &true_val,
                       sizeof(true_val)),
            0);

  int value = 16 * 1024;
  EXPECT_EQ(setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value)),
            0);
  EXPECT_EQ(setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value)),
            0);

  // Test TCP keepalive option.
  int period_seconds = static_cast<int>(30'000'000 / 1'000'000);
  EXPECT_EQ(setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &true_val,
                       sizeof(true_val)),
            0);
  #if defined(__APPLE__)
  // For reference:
  // https://stackoverflow.com/questions/15860127/how-to-configure-tcp-keepalive-under-mac-os-
  EXPECT_EQ(setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPALIVE, &period_seconds,
                       sizeof(period_seconds)),
            0);
  #else
  EXPECT_EQ(setsockopt(socket_fd, SOL_TCP, TCP_KEEPIDLE, &period_seconds,
                       sizeof(period_seconds)),
            0);
  EXPECT_EQ(setsockopt(socket_fd, SOL_TCP, TCP_KEEPINTVL, &period_seconds,
                       sizeof(period_seconds)),
            0);
  #endif

  EXPECT_EQ(setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &true_val,
                       sizeof(true_val)),
            0);

#if defined(_WIN32)
  closesocket(socket_fd);
#else
  close(socket_fd);
#endif
}

TEST_P(PosixSocketSetOptionsTest, TryThemAllUDP) {
#if defined(_WIN32)
  int socket_fd =
      WSASocketW(GetSocketDomain(), SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
#else
  int socket_fd = socket(GetSocketDomain(), SOCK_STREAM, IPPROTO_TCP);
#endif

  int true_val = 1;
  EXPECT_EQ(setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, &true_val,
                       sizeof(true_val)),
            0);
  EXPECT_EQ(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &true_val,
                       sizeof(true_val)),
            0);

  int value = 16 * 1024;
  EXPECT_EQ(setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value)),
            0);
  EXPECT_EQ(setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value)),
            0);

#if defined(_WIN32)
  closesocket(socket_fd);
#else
  close(socket_fd);
#endif
}

TEST_P(PosixSocketSetOptionsTest, RainyDayInvalidSocket) {
  int true_val = 1;
  EXPECT_EQ(
      setsockopt(-1, SOL_SOCKET, SO_REUSEADDR, &true_val, sizeof(true_val)),
      -1);

  int value = 16 * 1024;
  EXPECT_EQ(setsockopt(-1, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value)), -1);
  EXPECT_EQ(setsockopt(-1, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value)), -1);

  EXPECT_EQ(
      setsockopt(-1, SOL_SOCKET, SO_KEEPALIVE, &true_val, sizeof(true_val)),
      -1);

  EXPECT_EQ(
      setsockopt(-1, IPPROTO_TCP, TCP_NODELAY, &true_val, sizeof(true_val)),
      -1);
}

// TODO: Come up with some way to test the effects of these options.

#if SB_HAS(IPV6)
INSTANTIATE_TEST_SUITE_P(PosixSocketAddressTypes,
                         PosixSocketSetOptionsTest,
                         ::testing::Values(AF_INET, AF_INET6));
#else
INSTANTIATE_TEST_SUITE_P(PosixSocketAddressTypes,
                         PosixSocketSetOptionsTest,
                         ::testing::Values(AF_INET));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION >= 16
