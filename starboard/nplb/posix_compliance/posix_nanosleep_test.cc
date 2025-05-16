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
#include <sys/time.h>  // For gettimeofday()
#include <time.h>
#include <cstring>  // For strerror

#include "starboard/configuration_constants.h"  // Included for consistency
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {  // Anonymous namespace for test-local definitions

// Small duration for sleeps that should be short or return immediately.
const long kShortSleepNs = 1'000'000L;  // 1 millisecond
// A slightly longer duration for testing actual sleep.
const long kTestSleepNs = 50'000'000L;  // 50 milliseconds

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
//   to EINTR for sleep functions. If EINTR were to occur, nanosleep() would
//   return -1, set errno to EINTR, and (if 'rem' is not NULL) store the
//   remaining time in 'rem'.
// - EFAULT for 'rem' argument: nanosleep() can return EFAULT if 'rem' is an
//   invalid pointer *and* the function attempts to write to it (i.e., upon
//   interruption by a signal). Since EINTR is not tested, this specific EFAULT
//   scenario for 'rem' is also not covered. Passing a NULL 'rem' is valid and
//   tested.

// Test suite for POSIX nanosleep() function.
// The tests are named using the TEST macro, with PosixNanosleepTests
// as the test suite name.
TEST(PosixNanosleepTests, SuccessfulSleep) {
  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec req = {0,
                         kTestSleepNs};  // Request: 0 seconds, 50 milliseconds
  struct timespec rem;  // To store remaining time, if any (unused on success)
  errno = 0;            // Clear errno before the call
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful sleep. Return: " << ret
                    << ", errno: " << errno << " (" << strerror(errno) << ")";
  // POSIX states that if nanosleep returns 0, the contents of 'rem' are
  // unspecified.

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = timeval_diff_us(&start_time, &end_time);
  // Check that the elapsed time is at least the requested sleep time.
  // Due to system scheduling, it might sleep slightly longer.
  const useconds_t kTestSleepUs = kTestSleepNs / 1'000;
  EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

TEST(PosixNanosleepTests, SuccessfulSleepNullRemain) {
  struct timespec req = {0,
                         kShortSleepNs};  // Request: 0 seconds, 1 millisecond
  errno = 0;                              // Clear errno before the call
  int ret = nanosleep(&req, nullptr);     // 'rem' argument is NULL
  EXPECT_EQ(0, ret) << "Expected successful sleep with NULL remain. Return: "
                    << ret << ", errno: " << errno << " (" << strerror(errno)
                    << ")";
}

TEST(PosixNanosleepTests, ZeroDurationSleep) {
  struct timespec req = {0, 0L};  // Request: 0 seconds, 0 nanoseconds
  struct timespec rem;
  errno = 0;  // Clear errno before the call
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(0, ret)
      << "Expected immediate return for zero duration sleep. Return: " << ret
      << ", errno: " << errno << " (" << strerror(errno) << ")";
}

TEST(PosixNanosleepTests, ErrorEinvalRequestNsNegative) {
  struct timespec req = {0, -1L};  // Invalid: nanoseconds part is negative
  struct timespec rem;
  errno = 0;  // Clear errno before the call
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for invalid input.";
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for negative ns. errno: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST(PosixNanosleepTests, ErrorEinvalRequestNsTooLarge) {
  // Invalid: nanoseconds part is >= 1,000,000,000
  struct timespec req = {0, 1'000'000'000L};
  struct timespec rem;
  errno = 0;  // Clear errno before the call
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for invalid input.";
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for ns >= 10^9. errno: " << errno
                           << " (" << strerror(errno) << ")";
}

TEST(PosixNanosleepTests, ErrorEinvalRequestSecondsNegative) {
  struct timespec req = {-1, 0L};  // Invalid: seconds part is negative
  struct timespec rem;
  errno = 0;  // Clear errno before the call
  int ret = nanosleep(&req, &rem);
  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for invalid input.";
  EXPECT_EQ(EINVAL, errno) << "Expected EINVAL for negative tv_sec. errno: "
                           << errno << " (" << strerror(errno) << ")";
}

TEST(PosixNanosleepTests, ErrorEfaultRequestNull) {
  struct timespec rem;  // 'rem' can be valid or NULL for this specific test.
  errno = 0;            // Clear errno before the call

  timespec* req = nullptr;
  int ret = nanosleep(req, &rem);

  EXPECT_EQ(-1, ret) << "nanosleep should return -1 for NULL request pointer.";
  EXPECT_EQ(EFAULT, errno)
      << "Expected EFAULT for NULL request pointer. errno: " << errno << " ("
      << strerror(errno) << ")";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
