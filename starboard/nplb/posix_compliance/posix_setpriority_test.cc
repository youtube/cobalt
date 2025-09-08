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
#include <cerrno>
#include <cstring>

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
    // Save the original priority to restore it after each test.
    errno = 0;
    original_priority_ = getpriority(PRIO_PROCESS, 0);
    // getpriority can return -1 as a valid priority, so check errno.
    ASSERT_NE(-1, original_priority_)
        << "getpriority failed in SetUp. Errno: " << errno << " ("
        << strerror(errno) << ")";
  }

  void TearDown() override {
    // Restore the original priority.
    errno = 0;
    int result = setpriority(PRIO_PROCESS, 0, original_priority_);
    if (result == -1) {
      // It's possible the test left us with a priority we can't escape from
      // without root. Log a warning but don't fail the test suite.
      std::cerr << "Warning: Failed to restore original priority in TearDown. "
                << "Errno: " << errno << " (" << strerror(errno) << ")"
                << std::endl;
    }
  }

  int original_priority_;
};

// Tests setting the priority for the current process.
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

// Tests setting the priority for the current process group.
TEST_F(PosixSetPriorityTests, SetProcessGroupPrioritySuccessfully) {
  pid_t pgid = getpgrp();
  ASSERT_GE(pgid, 0) << "getpgrp() failed.";

  // Get the current priority of the process group.
  errno = 0;
  int original_pg_priority = getpriority(PRIO_PGRP, pgid);
  ASSERT_NE(-1, original_pg_priority)
      << "getpriority for process group failed. Errno: " << errno << " ("
      << strerror(errno) << ")";

  const int target_priority = original_pg_priority + 1;

  SB_LOG(INFO) << "John the targeted priority is " << target_priority;
  errno = 0;
  int result = setpriority(PRIO_PGRP, pgid, target_priority);
  int saved_errno = errno;

  // In some environments, a non-root user may not have permission to change
  // the priority of every process in the group, leading to ACCES. This is an
  // environmental constraint, not a failure of setpriority itself.
  if (result == -1 && saved_errno == EACCES) {
    GTEST_SKIP() << "Skipping test: Received ACCES when setting process group "
                    "priority. This is expected in some non-root environments.";
  }

  ASSERT_EQ(0, result) << "setpriority for process group failed. Errno: "
                       << saved_errno << " (" << strerror(saved_errno) << ")";

  errno = 0;
  int new_priority = getpriority(PRIO_PGRP, pgid);
  ASSERT_NE(-1, new_priority)
      << "getpriority for process group failed after setting. Errno: " << errno
      << " (" << strerror(errno) << ")";

  EXPECT_EQ(target_priority, new_priority);
}

// Tests setting the priority for all processes of the current user.
// Tests setting the priority for all processes of the current user.
TEST_F(PosixSetPriorityTests, SetUserPrioritySuccessfully) {
  uid_t uid = getuid();

  // Get current priority for the user's processes.
  errno = 0;
  int original_user_priority = getpriority(PRIO_USER, uid);
  ASSERT_NE(-1, original_user_priority)
      << "getpriority for user failed. Errno: " << errno << " ("
      << strerror(errno) << ")";

  const int target_priority = original_user_priority + 1;
  errno = 0;
  int result = setpriority(PRIO_USER, uid, target_priority);
  int saved_errno = errno;

  // Similar to the process group test, setting priority for all processes of
  // a user may fail with EACCES if the test runner doesn't have permission
  // to modify all of them (e.g., login shells).
  if (result == -1 && saved_errno == EACCES) {
    GTEST_SKIP() << "Skipping test: Received ACCES when setting user priority. "
                    "This is expected in some non-root environments.";
  }

  ASSERT_EQ(0, result) << "setpriority for user failed. Errno: " << saved_errno
                       << " (" << strerror(saved_errno) << ")";

  errno = 0;
  int new_priority = getpriority(PRIO_USER, uid);
  ASSERT_NE(-1, new_priority)
      << "getpriority for user failed after setting. Errno: " << errno << " ("
      << strerror(errno) << ")";

  EXPECT_EQ(target_priority, new_priority);
}

// Tests that setpriority fails with EINVAL for an invalid 'which' parameter.
TEST_F(PosixSetPriorityTests, ErrorOnInvalidWhich) {
  const int invalid_which = -1;  // An invalid value for 'which'.
  errno = 0;
  EXPECT_EQ(-1, setpriority(invalid_which, 0, 0));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid 'which', but got: "
                           << errno << " (" << strerror(errno) << ")";
}

// Tests that setpriority fails with ESRCH for a non-existent process ID.
TEST_F(PosixSetPriorityTests, ErrorOnInvalidWhoForProcess) {
  // Use a PID that is highly unlikely to exist.
  const pid_t non_existent_pid = -99999;
  errno = 0;
  EXPECT_EQ(-1, setpriority(PRIO_PROCESS, non_existent_pid, 0));
  EXPECT_EQ(ESRCH, errno) << "Expected ESRCH for non-existent PID, but got: "
                          << errno << " (" << strerror(errno) << ")";
}

// Tests that setpriority fails with EACCES when a non-root user tries to
// lower the nice value (increase priority).
TEST_F(PosixSetPriorityTests, ErrorOnPermissionDenied) {
  // This test is only meaningful if not run as root.
  if (geteuid() == 0) {
    GTEST_SKIP() << "This test cannot be run as root.";
  }

  // Attempt to set a higher priority (lower nice value).
  const int higher_priority = original_priority_ - 1;

  errno = 0;
  EXPECT_EQ(-1, setpriority(PRIO_PROCESS, 0, higher_priority));
  EXPECT_EQ(EACCES, errno)
      << "Expected EACCES when increasing priority, but got: " << errno << " ("
      << strerror(errno) << ")";
}

// Tests setting priority to the minimum and maximum allowed values.
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
    // If it fails, it should be due to permissions.
    EXPECT_EQ(EACCES, errno)
        << "Expected EACCES when setting to min priority as non-root, got: "
        << errno << " (" << strerror(errno) << ")";
  }
}

}  // namespace
}  // namespace starboard::nplb
