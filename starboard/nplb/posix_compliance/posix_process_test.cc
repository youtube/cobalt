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
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>

namespace nplb {
namespace {

TEST(PosixProcessTest, GetPid) {
  pid_t pid = getpid();
  EXPECT_GT(pid, 0) << "getpid failed: " << strerror(errno);
}

TEST(PosixProcessTest, GetEuid) {
  uid_t euid = geteuid();
  EXPECT_GE(euid, 0U) << "geteuid failed: " << strerror(errno);
}

TEST(PosixProcessTest, SchedGetPriorityMax) {
  int max_priority = sched_get_priority_max(SCHED_FIFO);
  EXPECT_NE(max_priority, -1)
      << "sched_get_priority_max failed: " << strerror(errno);
}

TEST(PosixProcessTest, SchedGetPriorityMin) {
  int min_priority = sched_get_priority_min(SCHED_FIFO);
  EXPECT_NE(min_priority, -1)
      << "sched_get_priority_min failed: " << strerror(errno);
}

}  // namespace
}  // namespace nplb
