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
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixErrnoTest, AcceptInvalidSocket) {
  int invalid_socket_fd = -1;
  EXPECT_FALSE(accept(invalid_socket_fd, NULL, NULL) == 0);
  EXPECT_TRUE(errno == EBADF);
  SB_DLOG(INFO) << "Failed to accept invalid socket, errno = "
                << strerror(errno);
}
}  // namespace
}  // namespace nplb
}  // namespace starboard
