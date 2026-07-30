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

#include "starboard/shared/modular/starboard_layer_posix_pthread_abi_wrappers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixSchedGetSchedulerTest, SchedGetSchedulerSuccess) {
  int policy = sched_getscheduler(0);
  EXPECT_GE(policy, 0);
  EXPECT_TRUE(policy == MUSL_SCHED_OTHER || policy == MUSL_SCHED_FIFO ||
              policy == MUSL_SCHED_RR);
}

TEST(PosixSchedGetSchedulerTest, SchedGetSchedulerFailsWithInvalidPid) {
  int result = sched_getscheduler(std::numeric_limits<pid_t>::max());
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, ESRCH);
}

}  // namespace
}  // namespace nplb
