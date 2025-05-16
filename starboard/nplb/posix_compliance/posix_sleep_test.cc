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
#include <unistd.h>    // For sleep()
#include <cstring>     // For strerror

#include "starboard/configuration_constants.h"  // Included for consistency
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {  // Anonymous namespace for test-local definitions

// Duration for testing actual sleep.
const unsigned int kTestSleepSecs = 1;  // 1 second
// A very short duration in microseconds, for checking if ZeroDurationSleep is
// fast.
const long kShortDurationThresholdUs = 100000;  // 100 milliseconds

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
//   to EINTR for sleep functions. If EINTR were to occur, sleep() would
//   return the number of unslept seconds.

// Test suite for POSIX sleep() function.
// The tests are named using the TEST macro, with PosixSleepTests
// as the test suite name.
TEST(PosixSleepTests, SuccessfulSleep) {
  errno = 0;  // Clear errno before the call

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  // The sleep() function returns 0 on success, or the number of unslept
  // seconds if interrupted by a signal.
  unsigned int ret = sleep(kTestSleepSecs);

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  EXPECT_EQ(0, ret) << "Expected successful sleep (return 0). Return: " << ret
                    << ", errno: " << errno << " (" << strerror(errno) << ")";

  if (ret == 0) {
    long elapsed_us = timeval_diff_us(&start_time, &end_time);
    long requested_us = static_cast<long>(kTestSleepSecs) * 1000000L;
    // Check that the elapsed time is at least the requested sleep time.
    // Due to system scheduling, it might sleep slightly longer.
    EXPECT_GE(elapsed_us, requested_us)
        << "Sleep duration was too short. Requested: " << requested_us << "us ("
        << kTestSleepSecs << "s), Elapsed: " << elapsed_us << "us.";
  }
}

TEST(PosixSleepTests, ZeroDurationSleep) {
  errno = 0;  // Clear errno before the call

  struct timeval start_time, end_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  unsigned int ret = sleep(0);  // Request: 0 seconds

  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  // sleep(0) should return 0 if successful.
  EXPECT_EQ(0, ret) << "Expected immediate return for zero duration sleep "
                       "(return 0). Return: "
                    << ret << ", errno: " << errno << " (" << strerror(errno)
                    << ")";

  if (ret == 0) {
    long elapsed_us = timeval_diff_us(&start_time, &end_time);
    // For a zero duration sleep, the elapsed time should be very small.
    EXPECT_LT(elapsed_us, kShortDurationThresholdUs)
        << "Zero duration sleep took too long. Elapsed: " << elapsed_us
        << "us. Threshold: " << kShortDurationThresholdUs << "us.";
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
