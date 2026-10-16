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
#include <gtest/gtest.h>
#include <sched.h>
#include <unistd.h>

namespace nplb {
namespace {

TEST(PosixSchedTest, SchedGetAffinitySuccess) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  pid_t pid = 0;  // A pid of 0 refers to the calling process.

  int result = sched_getaffinity(pid, sizeof(mask), &mask);

  EXPECT_EQ(result, 0) << "sched_getaffinity failed: " << strerror(errno);

  EXPECT_GT(CPU_COUNT(&mask), 0)
      << "The affinity mask should have at least one CPU set.";
}

TEST(PosixSchedTest, SchedGetAffinityPidZeroIsSelf) {
  cpu_set_t mask_for_pid_zero;
  cpu_set_t mask_for_actual_pid;
  CPU_ZERO(&mask_for_pid_zero);
  CPU_ZERO(&mask_for_actual_pid);

  pid_t actual_pid = getpid();
  EXPECT_GT(actual_pid, 0);

  int result1 =
      sched_getaffinity(0, sizeof(mask_for_pid_zero), &mask_for_pid_zero);
  EXPECT_EQ(result1, 0) << "sched_getaffinity(0) failed: " << strerror(errno);

  int result2 = sched_getaffinity(actual_pid, sizeof(mask_for_actual_pid),
                                  &mask_for_actual_pid);
  EXPECT_EQ(result2, 0) << "sched_getaffinity(getpid()) failed: "
                        << strerror(errno);

  EXPECT_TRUE(CPU_EQUAL(&mask_for_pid_zero, &mask_for_actual_pid))
      << "Masks for pid 0 and actual pid should be the same.";
}

TEST(PosixSchedTest, SchedGetAffinityFailsWithInvalidPid) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  pid_t invalid_pid = -1;

  int result = sched_getaffinity(invalid_pid, sizeof(mask), &mask);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, ESRCH) << "Expected ESRCH for invalid PID, but got: "
                          << strerror(errno);
}

TEST(PosixSchedTest, SchedGetAffinityFailsWithInvalidCpuSetSize) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  pid_t pid = 0;
  size_t invalid_size = 1;

  if (sizeof(cpu_set_t) <= invalid_size) {
    GTEST_SKIP() << "Cannot test invalid size: sizeof(cpu_set_t) is too small.";
  }

  int result = sched_getaffinity(pid, invalid_size, &mask);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL) << "Expected EINVAL for small cpusetsize, but got: "
                           << strerror(errno);
}

}  // namespace
}  // namespace nplb
