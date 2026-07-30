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

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

class PosixGetrlimitSuccessTests : public ::testing::TestWithParam<int> {};

TEST_P(PosixGetrlimitSuccessTests, SucceedsForValidResource) {
  int resource = GetParam();
  struct rlimit rlim;

  errno = 0;
  int result = getrlimit(resource, &rlim);
  int call_errno = errno;

  EXPECT_EQ(0, result) << "getrlimit() failed unexpectedly. Errno: "
                       << call_errno << " (" << strerror(call_errno) << ")";
  EXPECT_EQ(0, call_errno);

  // Ensure that the current (soft) limit is less than or equal to the maximum
  // (hard) limit.
  EXPECT_LE(rlim.rlim_cur, rlim.rlim_max);
}

INSTANTIATE_TEST_SUITE_P(PosixGetrlimitTests,
                         PosixGetrlimitSuccessTests,
                         ::testing::Values(RLIMIT_AS,
                                           RLIMIT_CORE,
                                           RLIMIT_CPU,
                                           RLIMIT_DATA,
                                           RLIMIT_FSIZE,
                                           RLIMIT_NOFILE,
                                           RLIMIT_STACK
#if defined(RLIMIT_NPROC)
                                           ,
                                           RLIMIT_NPROC
#endif  // defined(RLIMIT_NPROC)
#if defined(RLIMIT_MEMLOCK)
                                           ,
                                           RLIMIT_MEMLOCK
#endif  // defined(RLIMIT_MEMLOCK)
#if defined(RLIMIT_RSS)
                                           ,
                                           RLIMIT_RSS
#endif  // defined(RLIMIT_RSS)
#if defined(RLIMIT_LOCKS)
                                           ,
                                           RLIMIT_LOCKS
#endif  // defined(RLIMIT_LOCKS)
#if defined(RLIMIT_SIGPENDING)
                                           ,
                                           RLIMIT_SIGPENDING
#endif  // defined(RLIMIT_SIGPENDING)
#if defined(RLIMIT_MSGQUEUE)
                                           ,
                                           RLIMIT_MSGQUEUE
#endif  // defined(RLIMIT_MSGQUEUE)
#if defined(RLIMIT_NICE)
                                           ,
                                           RLIMIT_NICE
#endif  // defined(RLIMIT_NICE)
#if defined(RLIMIT_RTPRIO)
                                           ,
                                           RLIMIT_RTPRIO
#endif  // defined(RLIMIT_RTPRIO)
#if defined(RLIMIT_RTTIME)
                                           ,
                                           RLIMIT_RTTIME
#endif  // defined(RLIMIT_RTTIME)
                                           ));

// Since we have no guarantees what privileges a process has when it runs these
// tests, we are unable to test the |EPERM| errno for getrlimit.
TEST(PosixGetrlimitTests, FailsForInvalidResource) {
  const int invalid_resource = -1;
  struct rlimit rlim;

  errno = 0;
  EXPECT_EQ(-1, getrlimit(invalid_resource, &rlim));
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for invalid resource, got: "
                           << errno << " (" << strerror(errno) << ")";
}

}  // namespace
}  // namespace nplb
