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

TEST(PosixSchedGetParamTest, SchedGetParamSuccess) {
  struct sched_param param;
  int result = sched_getparam(0, &param);

  EXPECT_EQ(result, 0) << "sched_getparam failed: " << strerror(errno);
}

TEST(PosixSchedGetParamTest, SchedGetParamFailsWithInvalidPid) {
  struct sched_param param;
  int result = sched_getparam(-1, &param);

  EXPECT_EQ(result, -1);
  EXPECT_TRUE(errno == EINVAL || errno == ESRCH) << "errno was " << errno;
}

TEST(PosixSchedGetParamTest, SchedGetParamFailsWithNullParam) {
  int result = sched_getparam(0, nullptr);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

}  // namespace
}  // namespace nplb
