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

class PosixSocketCreateTest : public ::testing::TestWithParam<int> {
 public:
  int GetAddressType() { return GetParam(); }
  int GetSocketDomain() { return GetParam() == 0 ? AF_INET : AF_INET6; }
};

class PosixPairSocketCreateTest
    : public ::testing::TestWithParam<std::pair<int, int> > {
 public:
  int GetAddressType() { return GetParam().first; }
  int GetSocketDomain() { return GetParam().first == 0 ? AF_INET : AF_INET6; }
  int GetSocketType() {
    return GetParam().second == 0 ? SOCK_STREAM : SOCK_DGRAM;
  }
  int GetSocketProtocol() {
    return GetParam().second == 0 ? IPPROTO_TCP : IPPROTO_UDP;
  }
  int GetProtocol() { return GetParam().second; }
};

int PosixSocketCreate(int domain, int type, int protocol) {
  return ::socket(domain, type, protocol);
}

int PosixSocketClose(int socket_fd) {
  int result = 0;
  if (socket_fd >= 0) {
#if defined(_WIN32)
    result = _close(socket_fd);
#else
    result = close(socket_fd);
#endif
  }

  return result;
}

TEST_P(PosixPairSocketCreateTest, Create) {
  int socket_fd = PosixSocketCreate(GetSocketDomain(), GetSocketType(),
                                    GetSocketProtocol());
  EXPECT_TRUE(socket_fd > 0);
  EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
}

TEST_P(PosixSocketCreateTest, ATonOfTcp) {
  const int kATon = 4096;
  for (int i = 0; i < kATon; ++i) {
    int socket_fd =
        PosixSocketCreate(GetSocketDomain(), SOCK_STREAM, IPPROTO_TCP);

    EXPECT_TRUE(socket_fd > 0);
    EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
  }
}

TEST_P(PosixSocketCreateTest, ManyTcpAtOnce) {
  const int kMany = 128;
  int sockets_fd[kMany] = {0};
  for (int i = 0; i < kMany; ++i) {
    sockets_fd[i] =
        PosixSocketCreate(GetSocketDomain(), SOCK_STREAM, IPPROTO_TCP);
    ASSERT_TRUE(sockets_fd[i] > 0);
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(PosixSocketClose(sockets_fd[i]) == 0);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
   //
#endif  // SB_API_VERSION >= 16
