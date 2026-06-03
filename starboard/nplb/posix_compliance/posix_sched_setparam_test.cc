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

class PosixSchedSetParamTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_GE(sched_getparam(0, &original_param), 0)
        << "sched_getparam failed: " << strerror(errno);
  }
  void TearDown() override {
    if (sched_setparam(0, &original_param) != 0) {
      SB_LOG(ERROR) << "Failed to restore original scheduling parameters: "
                    << strerror(errno);
    }
  }

  struct sched_param original_param;
};

TEST_F(PosixSchedSetParamTest, SchedSetParamSuccess) {
  int result = sched_setparam(0, &original_param);

  if (result == -1 && errno == EPERM) {
    GTEST_SKIP() << "Insufficient permissions to set scheduling parameters.";
  }

  EXPECT_EQ(result, 0) << "sched_setparam failed: " << strerror(errno);
}

TEST_F(PosixSchedSetParamTest, SchedSetParamFailsWithInvalidPid) {
  struct sched_param param;
  param.sched_priority = 0;
  int result = sched_setparam(std::numeric_limits<pid_t>::max(), &param);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, ESRCH);
}

TEST_F(PosixSchedSetParamTest, SchedSetParamFailsWithNullParam) {
  int result = sched_setparam(0, nullptr);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixSchedSetParamTest, SchedSetParamFailsWithInvalidPriority) {
  int current_policy = sched_getscheduler(0);
  ASSERT_GE(current_policy, 0)
      << "sched_getscheduler failed: " << strerror(errno);

  int min_priority = sched_get_priority_min(current_policy);
  int max_priority = sched_get_priority_max(current_policy);

  if (min_priority == -1 || max_priority == -1) {
    GTEST_SKIP() << "Could not get priority range for current policy";
  }

  struct sched_param param;
  param.sched_priority = min_priority - 1;
  int result = sched_setparam(0, &param);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);

  param.sched_priority = max_priority + 1;
  result = sched_setparam(0, &param);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

}  // namespace
}  // namespace nplb
