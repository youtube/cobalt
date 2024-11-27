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

#include "starboard/common/log.h"
#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixErrnoTest, CreateInvalidSocket) {
  EXPECT_FALSE(socket(-1, SOCK_STREAM, IPPROTO_TCP) == 0);
  EXPECT_TRUE(errno == EAFNOSUPPORT);
  SB_DLOG(INFO) << "Failed to create invalid socket, errno = "
                << strerror(errno);
}

TEST(PosixErrnoTest, AcceptInvalidSocket) {
  int invalid_socket_fd = -1;
  EXPECT_FALSE(accept(invalid_socket_fd, NULL, NULL) == 0);
  EXPECT_TRUE(errno == EBADF);
  SB_DLOG(INFO) << "Failed to accept invalid socket, errno = "
                << strerror(errno);
}

TEST(PosixErrnoTest, ConnectUnavailableAddress) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd > 0);

  sockaddr_in6 address = {};
  EXPECT_TRUE(
      PosixGetLocalAddressIPv4(reinterpret_cast<sockaddr*>(&address)) == 0 ||
      PosixGetLocalAddressIPv6(reinterpret_cast<sockaddr*>(&address)) == 0);

  // Attempt to connect to an address where we expect connection to be refused
  connect(socket_fd, (struct sockaddr*)&address, sizeof(address));

  EXPECT_TRUE(errno == ECONNREFUSED || errno == EADDRNOTAVAIL ||
              errno == EINPROGRESS || errno == EINVAL);
  SB_DLOG(INFO) << "Failed to connect to unavailable address, errno = "
                << strerror(errno);

  close(socket_fd);
}
}  // namespace
}  // namespace nplb
}  // namespace starboard
