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

#include <errno.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>

#include "starboard/common/thread.h"
#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

void ExpectGetPrioritySuccess(int priority_val, int errno_val) {
  EXPECT_FALSE((priority_val == -1) && (errno_val != 0))
      << "getpriority() failed unexpectedly. Errno: " << (errno_val) << " ("
      << strerror(errno_val) << ")";
}

TEST(PosixGetPriorityTests, SucceedsForCurrentProcess) {
  errno = 0;
  int priority = getpriority(PRIO_PROCESS, 0);
  int call_errno = errno;

  ExpectGetPrioritySuccess(priority, call_errno);
}

TEST(PosixGetPriorityTests, SucceedsForCurrentProcessGroup) {
  errno = 0;
  int priority = getpriority(PRIO_PGRP, 0);
  int call_errno = errno;

  ExpectGetPrioritySuccess(priority, call_errno);
}

TEST(PosixGetPriorityTests, SucceedsForCurrentUser) {
  errno = 0;
  int priority = getpriority(PRIO_USER, 0);
  int call_errno = errno;

  ExpectGetPrioritySuccess(priority, call_errno);
}

TEST(PosixGetPriorityTests, FailsForInvalidWhich) {
  const int invalid_which = -1;
  errno = 0;
  EXPECT_EQ(-1, getpriority(invalid_which, 0));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL, got: " << errno << " ("
                           << strerror(errno) << ")";
}

TEST(PosixGetPriorityTests, FailsForNonExistentProcess) {
  const pid_t non_existent_pid = -99999;
  errno = 0;
  EXPECT_EQ(-1, getpriority(PRIO_PROCESS, non_existent_pid));
  EXPECT_EQ(ESRCH, errno) << "Expected ESRCH, got: " << errno << " ("
                          << strerror(errno) << ")";
}

TEST(PosixGetPriorityTests, FailsForNonExistentProcessGroup) {
  const pid_t non_existent_pgid = -99999;
  errno = 0;
  EXPECT_EQ(-1, getpriority(PRIO_PGRP, non_existent_pgid));
  EXPECT_EQ(ESRCH, errno) << "Expected ESRCH, got: " << errno << " ("
                          << strerror(errno) << ")";
}

TEST(PosixGetPriorityTests, FailsForNonExistentUser) {
  const uid_t non_existent_uid = 99999;
  errno = 0;
  EXPECT_EQ(-1, getpriority(PRIO_USER, non_existent_uid));
  EXPECT_EQ(ESRCH, errno) << "Expected ESRCH, got: " << errno << " ("
                          << strerror(errno) << ")";
}

class PosixSetPriorityTests : public ::testing::Test {
 protected:
  void SetUp() override {
    if (geteuid() == 0) {
      GTEST_SKIP()
          << "Skipping this test due to being ran on root. This test suite"
          << "is only effective if we are not running on root.";
    }
  }
};

// Since we don't support getpgid() nor getuid(), we can only test the
// functionality for |PRIO_PROCESS|. Setting priorities for |PRIO_PGRP| and
// |PRIO_USER| are not tested.
//
// We have decided to not test the values outside the Linux standard range of
// [-20, 19], as we would be only able to test for values past the lowest
// priority, 19. This is problematic because:

// - The tests would become order dependent. If a test that set the priority to
// 19 ran before the PosixSetPriorityTests.SetProcessPrioritySuccessfully,
// PosixSetPriorityTests.SetProcessPrioritySuccessfully would fail, as the
// priority value can go no lower.
// - It's possible that if we set the process to the lowest priority, the system
// can ignore any events on this process for a while.
TEST_F(PosixSetPriorityTests, SetProcessPrioritySuccessfully) {
  errno = 0;
  const int current_priority = getpriority(PRIO_PROCESS, 0);
  int call_errno = errno;
  ExpectGetPrioritySuccess(current_priority, call_errno);
  const int target_priority = current_priority + 1;

  errno = 0;
  ASSERT_EQ(0, setpriority(PRIO_PROCESS, 0, target_priority))
      << "setpriority failed. Errno: " << errno << " (" << strerror(errno)
      << ")";

  errno = 0;
  int new_priority = getpriority(PRIO_PROCESS, 0);
  call_errno = errno;
  ExpectGetPrioritySuccess(new_priority, call_errno);

  EXPECT_EQ(target_priority, new_priority);
}

TEST_F(PosixSetPriorityTests, ErrorOnInvalidWhich) {
  const int invalid_which = -1;
  errno = 0;
  EXPECT_EQ(-1, setpriority(invalid_which, 0, 0));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid 'which', but got: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixSetPriorityTests, ErrorOnInvalidWhoForProcess) {
  const pid_t non_existent_pid = -99999;
  errno = 0;
  EXPECT_EQ(-1, setpriority(PRIO_PROCESS, non_existent_pid, 0));
  EXPECT_EQ(ESRCH, errno) << "Expected ESRCH for non-existent PID, but got: "
                          << errno << " (" << strerror(errno) << ")";
}

TEST_F(PosixSetPriorityTests, ErrorOnPermissionDenied) {
  errno = 0;
  const int current_priority = getpriority(PRIO_PROCESS, 0);
  int call_errno = errno;
  ExpectGetPrioritySuccess(current_priority, call_errno);

  // An unprivileged process may only lower its nice value down to
  // 20 - RLIMIT_NICE, so going below both that and the current value fails.
  struct rlimit nice_limit;
  ASSERT_EQ(0, getrlimit(RLIMIT_NICE, &nice_limit))
      << "getrlimit failed. Errno: " << errno << " (" << strerror(errno) << ")";
  const int ceiling =
      20 - static_cast<int>(std::min<rlim_t>(nice_limit.rlim_cur, 40));
  const int denied_priority = std::min(current_priority, ceiling) - 1;
  if (denied_priority < -20) {
    GTEST_SKIP() << "RLIMIT_NICE (" << nice_limit.rlim_cur
                 << ") permits every priority.";
  }

  errno = 0;
  EXPECT_EQ(-1, setpriority(PRIO_PROCESS, 0, denied_priority));
  EXPECT_EQ(EACCES, errno)
      << "Expected EACCES when increasing priority, but got: " << errno << " ("
      << strerror(errno) << ")";
}

TEST_F(PosixSetPriorityTests, SetProcessPriorityToStarboardNiceValues) {
  // Test only low priorities as the process can't go back to higher priorities.
  // NOTE: The priority is already lowered from the normal level by the
  //  SetProcessPrioritySuccessfully test.

  // Low Priority
  int low_nice =
      starboard::ThreadPriorityToNiceValue(starboard::ThreadPriority::kLow);
  ASSERT_EQ(0, setpriority(PRIO_PROCESS, 0, low_nice))
      << "setpriority failed for Low. Errno: " << errno << " ("
      << strerror(errno) << ")";
  EXPECT_EQ(low_nice, getpriority(PRIO_PROCESS, 0));

  // Lowest Priority
  int lowest_nice =
      starboard::ThreadPriorityToNiceValue(starboard::ThreadPriority::kLowest);
  ASSERT_EQ(0, setpriority(PRIO_PROCESS, 0, lowest_nice))
      << "setpriority failed for Lowest. Errno: " << errno << " ("
      << strerror(errno) << ")";
  EXPECT_EQ(lowest_nice, getpriority(PRIO_PROCESS, 0));
}

}  // namespace
}  // namespace nplb
