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

#include <sys/resource.h>
#include <unistd.h>

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::nplb {
namespace {

// NOTE: Running these tests as a non-root user is crucial for permission
// checks. The test for EACCES specifically relies on the inability to raise
// priority (lower the nice value).

class PosixSetPriorityTests : public ::testing::Test {
 protected:
  void SetUp() override {
    if (geteuid() == 0) {
      GTEST_SKIP()
          << "Skipping this test due to being ran on root. This test suite"
          << "is only effective if we are not running on root.";
    }
    errno = 0;
    original_priority_ = getpriority(PRIO_PROCESS, 0);
    SB_LOG(INFO) << "The original priority is: " << original_priority_;
    // getpriority can return -1 as a valid priority, so check errno.
    ASSERT_EQ(0, errno) << "getpriority failed in SetUp. Errno: " << errno
                        << " (" << strerror(errno) << ")";
  }

  void TearDown() override {
    errno = 0;
    int result = setpriority(PRIO_PROCESS, 0, original_priority_);
    if (result == -1) {
      // It's possible the test left us with a priority we can't escape from
      // without root. Log a warning but don't fail the test suite.
      SB_LOG(WARNING)
          << "Warning: Failed to restore original priority in TearDown. "
          << "Errno: " << errno << " (" << strerror(errno) << ")";
    }
  }

  int original_priority_;
};

// Since we don't support getuid() nor getpgid(), we can only test the
// functionality for |PRIO_PROCESS|. |PRIO_PGRP| and |PRIO_USER| are not tested.
TEST_F(PosixSetPriorityTests, SetProcessPrioritySuccessfully) {
  const int target_priority = original_priority_ + 1;

  errno = 0;
  ASSERT_EQ(0, setpriority(PRIO_PROCESS, 0, target_priority))
      << "setpriority failed. Errno: " << errno << " (" << strerror(errno)
      << ")";

  errno = 0;
  int new_priority = getpriority(PRIO_PROCESS, 0);
  ASSERT_NE(-1, new_priority)
      << "getpriority failed after setting. Errno: " << errno << " ("
      << strerror(errno) << ")";

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
  const int higher_priority = original_priority_ - 1;

  errno = 0;
  EXPECT_EQ(-1, setpriority(PRIO_PROCESS, 0, higher_priority));
  EXPECT_EQ(EACCES, errno)
      << "Expected EACCES when increasing priority, but got: " << errno << " ("
      << strerror(errno) << ")";
}

TEST_F(PosixSetPriorityTests, SetToMinimumAndMaximumPriority) {
  // POSIX defines the range as -20 (highest) to 19 (lowest).
  const int max_priority = 19;
  const int min_priority = -20;

  // Test setting to the maximum (lowest priority).
  errno = 0;
  ASSERT_EQ(0, setpriority(PRIO_PROCESS, 0, max_priority))
      << "Failed to set to max priority. Errno: " << errno << " ("
      << strerror(errno) << ")";
  errno = 0;
  EXPECT_EQ(max_priority, getpriority(PRIO_PROCESS, 0));

  // Test setting to the minimum (highest priority). This may fail if not root.
  errno = 0;
  if (setpriority(PRIO_PROCESS, 0, min_priority) == 0) {
    errno = 0;
    EXPECT_EQ(min_priority, getpriority(PRIO_PROCESS, 0));
  } else {
    EXPECT_EQ(EACCES, errno)
        << "Expected EACCES when setting to min priority as non-root, got: "
        << errno << " (" << strerror(errno) << ")";
  }
}

}  // namespace
}  // namespace starboard::nplb
