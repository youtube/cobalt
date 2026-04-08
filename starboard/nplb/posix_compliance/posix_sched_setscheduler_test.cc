// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
#include <sched.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixSchedSetSchedulerTest, SchedSetSchedulerSuccess) {
  struct sched_param param;
  param.sched_priority = 0;
  // SCHED_OTHER usually requires priority 0.
  int result = sched_setscheduler(0, SCHED_OTHER, &param);

  // On some systems, even setting SCHED_OTHER might require privileges if
  // it's considered a change, but usually setting it to what it already is
  // or to SCHED_OTHER for a normal process is fine.
  if (result == -1 && errno == EPERM) {
    GTEST_SKIP() << "Insufficient permissions to set scheduling policy.";
  }

  EXPECT_EQ(result, 0) << "sched_setscheduler failed: " << strerror(errno);
}

TEST(PosixSchedSetSchedulerTest, SchedSetSchedulerFailsWithInvalidPid) {
  struct sched_param param;
  param.sched_priority = 0;
  int result = sched_setscheduler(-1, SCHED_OTHER, &param);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST(PosixSchedSetSchedulerTest, SchedSetSchedulerFailsWithInvalidPolicy) {
  struct sched_param param;
  param.sched_priority = 0;
  int result = sched_setscheduler(0, -1, &param);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST(PosixSchedSetSchedulerTest, SchedSetSchedulerFailsWithNullParam) {
  int result = sched_setscheduler(0, SCHED_OTHER, nullptr);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST(PosixSchedSetSchedulerTest, SchedSetSchedulerFailsWithInvalidPriority) {
  int min_priority = sched_get_priority_min(SCHED_FIFO);
  int max_priority = sched_get_priority_max(SCHED_FIFO);

  if (min_priority == -1 || max_priority == -1) {
    GTEST_SKIP() << "Could not get priority range for SCHED_FIFO";
  }

  struct sched_param param;
  param.sched_priority = min_priority - 1;
  int result = sched_setscheduler(0, SCHED_FIFO, &param);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);

  param.sched_priority = max_priority + 1;
  result = sched_setscheduler(0, SCHED_FIFO, &param);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

}  // namespace
}  // namespace nplb
