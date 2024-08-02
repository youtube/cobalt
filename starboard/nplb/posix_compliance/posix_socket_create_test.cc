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

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixSocketCreateTest, ATonOfTcp) {
  const int kATon = 4096;
  for (int i = 0; i < kATon; ++i) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    EXPECT_TRUE(socket_fd >= 0);
    EXPECT_TRUE(close(socket_fd) == 0);
  }
}

TEST(PosixSocketCreateTest, ManyTcpAtOnce) {
  const int kMany = 128;
  int sockets_fd[kMany] = {0};
  for (int i = 0; i < kMany; ++i) {
    sockets_fd[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_TRUE(sockets_fd[i] >= 0);
  }

  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(close(sockets_fd[i]) == 0);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
