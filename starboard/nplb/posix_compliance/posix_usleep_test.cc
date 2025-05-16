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
#include <signal.h>    // For EINTR definition (though not tested directly)
#include <sys/time.h>  // For gettimeofday()
#include <time.h>      // For timespec (though not directly used by usleep)
#include <unistd.h>    // For usleep()
#include <cstring>     // For strerror

#include "starboard/configuration_constants.h"  // Included for consistency
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {  // Anonymous namespace for test-local definitions

// Small duration for sleeps that should be short or return immediately.
const useconds_t kShortSleepUs = 1000;  // 1 millisecond (1,000 microseconds)
// A slightly longer duration for testing actual sleep.
const useconds_t kTestSleepUs = 50000;  // 50 milliseconds (50,000 microseconds)

// Helper function to calculate the difference between two timeval structs in
// microseconds.
long timeval_diff_us(const struct timeval* start, const struct timeval* end) {
  if (!start || !end) {
    return 0;
  }
  long seconds_diff = end->tv_sec - start->tv_sec;
  long useconds_diff = end->tv_usec - start->tv_usec;
  return (seconds_diff * 1000000L) + useconds_diff;
}

// Note on error conditions not tested:
// - EINTR: Not tested here because Starboard (as per the original example's
//   context) does not implement full signal handling that would typically lead
//   to EINTR for sleep functions. If EINTR were to occur, usleep() would
//   return -1 and set errno to EINTR.
// - EFAULT: usleep() takes its argument by value, so EFAULT related to
//   invalid pointers for the duration argument is not applicable.

// Test suite for POSIX usleep() function.
// The tests are named using the TEST macro, with PosixUsleepTests
// as the test suite name.
// Test suite for POSIX usleep() function.
// The tests are named using the TEST macro, with PosixUsleepTests
// as the test suite name.
TEST(PosixUsleepTests, SuccessfulSleep) {
  errno = 0;  // Clear errno before the call

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  int ret = usleep(kTestSleepUs);

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  EXPECT_EQ(0, ret) << "Expected successful sleep. Return: " << ret
                    << ", errno: " << errno << " (" << strerror(errno) << ")";

  if (ret == 0) {
    long elapsed_us = timeval_diff_us(&start_time, &end_time);
    // Check that the elapsed time is at least the requested sleep time.
    // Due to system scheduling, it might sleep slightly longer.
    EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
        << "Sleep duration was too short. Requested: " << kTestSleepUs
        << "us, Elapsed: " << elapsed_us << "us.";
  }
}

TEST(PosixUsleepTests, ZeroDurationSleep) {
  errno = 0;            // Clear errno before the call
  int ret = usleep(0);  // Request: 0 microseconds
  EXPECT_EQ(0, ret)
      << "Expected immediate return for zero duration sleep. Return: " << ret
      << ", errno: " << errno << " (" << strerror(errno) << ")";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
