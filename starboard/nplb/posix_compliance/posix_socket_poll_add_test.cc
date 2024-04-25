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

#include <poll.h>
#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixSocketPollAddTest, SunnyDay) {
  struct pollfd fds = {0};
  fds.fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(fds.fd > 0);

  nfds_t nfds = 1;
  int timeout = 0;
  EXPECT_TRUE(poll(&fds, nfds, timeout) >= 0);

  EXPECT_TRUE(close(fds.fd) == 0);
}

TEST(PosixSocketPollAddTest, SunnyDayMany) {
  const int kMany = kSbFileMaxOpen;

  struct pollfd* pfds = (struct pollfd*)malloc(sizeof(struct pollfd)*kMany);
  memset(pfds, 0, sizeof(struct pollfd)*kMany);

  for (int i = 0; i < kMany; ++i) {
    pfds[i].fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_TRUE(pfds[i].fd > 0);
  }
    
  nfds_t nfds = kMany;
  int timeout = 0;
  EXPECT_TRUE(poll(pfds, nfds, timeout) >= 0);

  for (int i = 0; i < kMany; ++i) {
    EXPECT_TRUE(pfds[i].fd);
  }

  free(pfds);
}

TEST(PosixSocketPollAddTest, SunnyDayInvalidSocket) {
  struct pollfd fds = {0};
  // poll() allows negative fd
  fds.fd = -1;

  nfds_t nfds = 1;
  int timeout = 0;
  EXPECT_TRUE(poll(&fds, nfds, timeout) >= 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
