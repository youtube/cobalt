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

#include <limits>

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

class PosixSchedSetSchedulerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    original_policy = sched_getscheduler(0);
    ASSERT_GE(original_policy, 0)
        << "sched_getscheduler failed: " << strerror(errno);
    ASSERT_GE(sched_getparam(0, &original_param), 0)
        << "sched_getparam failed: " << strerror(errno);
  }
  void TearDown() override {
    if ((sched_setscheduler(0, original_policy, &original_param)) != 0) {
      SB_LOG(ERROR) << "Failed to restore original scheduling policy: "
                    << strerror(errno);
    }
  }

  int original_policy;
  struct sched_param original_param;
};

// For the purposes of this test suite, we define a success as a call that
// either sets the scheduling policy (i.e. returns 0) or results in an EPERM
// error, as it's possible that the current test environment doesn't have
// sufficient permissions to set the scheduling policy. We are essentially
// testing that the functionality is present and at least doesn't crash.
//
// Additionally, SCHED_FIFO, SCHED_RR, and SCHED_BATCH require a priority to be
// set. In those tests, we try to use the maximum priority, even though
// processes generally do not have permission to increase their own priority, so
// a result of EPERM is expected. We would ideally want to test setting minimum
// priority, which would theoretically always succeed, but that would then
// prevent the test process from re-raising its priority during teardown.

TEST_F(PosixSchedSetSchedulerTest, SchedSetSchedulerSuccessForOther) {
  struct sched_param param;
  param.sched_priority = 0;
  // SCHED_OTHER usually requires priority 0.
  int result = sched_setscheduler(0, SCHED_OTHER, &param);

  if (result == -1 && errno == EPERM) {
    GTEST_SKIP() << "Insufficient permissions to set scheduling policy.";
  }

  EXPECT_GE(result, 0) << "sched_setscheduler failed: " << strerror(errno);
}

TEST_F(PosixSchedSetSchedulerTest, SchedSetSchedulerSuccessForFifo) {
  int min_priority = sched_get_priority_min(SCHED_FIFO);
  int max_priority = sched_get_priority_max(SCHED_FIFO);

  if (min_priority == -1 || max_priority == -1) {
    GTEST_SKIP() << "Could not get priority range for SCHED_FIFO";
  }

  struct sched_param param;
  param.sched_priority = max_priority;
  int result = sched_setscheduler(0, SCHED_FIFO, &param);
  if (result == -1 && errno == EPERM) {
    GTEST_SKIP() << "Insufficient permissions to set scheduling policy.";
  }

  EXPECT_GE(result, 0) << "sched_setscheduler failed: " << strerror(errno);
}

TEST_F(PosixSchedSetSchedulerTest, SchedSetSchedulerSuccessForRr) {
  int min_priority = sched_get_priority_min(SCHED_RR);
  int max_priority = sched_get_priority_max(SCHED_RR);

  if (min_priority == -1 || max_priority == -1) {
    GTEST_SKIP() << "Could not get priority range for SCHED_RR";
  }

  struct sched_param param;
  param.sched_priority = max_priority;
  int result = sched_setscheduler(0, SCHED_RR, &param);
  if (result == -1 && errno == EPERM) {
    GTEST_SKIP() << "Insufficient permissions to set scheduling policy.";
  }

  EXPECT_GE(result, 0) << "sched_setscheduler failed: " << strerror(errno);
}

TEST_F(PosixSchedSetSchedulerTest, SchedSetSchedulerSuccessForBatch) {
  int min_priority = sched_get_priority_min(SCHED_BATCH);
  int max_priority = sched_get_priority_max(SCHED_BATCH);

  if (min_priority == -1 || max_priority == -1) {
    GTEST_SKIP() << "Could not get priority range for SCHED_BATCH";
  }

  struct sched_param param;
  param.sched_priority = max_priority;
  int result = sched_setscheduler(0, SCHED_BATCH, &param);
  if (result == -1 && errno == EPERM) {
    GTEST_SKIP() << "Insufficient permissions to set scheduling policy.";
  }

  EXPECT_GE(result, 0) << "sched_setscheduler failed: " << strerror(errno);
}

TEST_F(PosixSchedSetSchedulerTest, SchedSetSchedulerSuccessForIdle) {
  struct sched_param param;
  param.sched_priority = 0;
  int result = sched_setscheduler(0, SCHED_IDLE, &param);
  if (result == -1 && errno == EPERM) {
    GTEST_SKIP() << "Insufficient permissions to set scheduling policy.";
  }

  EXPECT_GE(result, 0) << "sched_setscheduler failed: " << strerror(errno);
}

TEST_F(PosixSchedSetSchedulerTest, SchedSetSchedulerFailsWithInvalidPid) {
  struct sched_param param;
  param.sched_priority = 0;
  int result = sched_setscheduler(std::numeric_limits<pid_t>::max(),
                                  SCHED_OTHER, &param);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, ESRCH);
}

TEST_F(PosixSchedSetSchedulerTest, SchedSetSchedulerFailsWithInvalidPolicy) {
  struct sched_param param;
  param.sched_priority = 0;
  int result = sched_setscheduler(0, -1, &param);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixSchedSetSchedulerTest, SchedSetSchedulerFailsWithNullParam) {
  int result = sched_setscheduler(0, SCHED_OTHER, nullptr);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixSchedSetSchedulerTest, SchedSetSchedulerFailsWithInvalidPriority) {
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
