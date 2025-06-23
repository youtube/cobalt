// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_EPOLL_TEST_HELPER_H_
#define STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_EPOLL_TEST_HELPER_H_

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

// A helper function to create a pipe.
// Returns true on success, false on failure.
// pipe_fds[0] is the read end, pipe_fds[1] is the write end.
bool CreatePipe(int pipe_fds[2], int flags = 0);

// Test fixture for epoll tests
class PosixEpollTest : public ::testing::Test {
 protected:
  static const int kInvalidFd = -1;  // An fd value that is likely invalid.
  static const int kMaxEvents = 10;  // Maximum number of events for epoll_wait.
  static const int kShortTimeoutMs = 50;
  static const int kModerateTimeoutMs = 100;
};

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_POSIX_COMPLIANCE_POSIX_EPOLL_TEST_HELPER_H_
