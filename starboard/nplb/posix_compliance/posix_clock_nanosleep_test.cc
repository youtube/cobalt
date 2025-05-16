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
#include <signal.h>
#include <sys/time.h>  // For gettimeofday()
#include <time.h>
#include <unistd.h>  // For syscall

#include "build/build_config.h"
#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Small duration for sleeps that should be short or return immediately.
const long kShortSleepNs = 1'000'000;  // 1 millisecond
const long kShortSleepUs = kShortSleepNs / 1'000;
// A slightly longer duration for testing actual sleep.
const long kTestSleepNs = 50'000'000;  // 50 milliseconds
const long kTestSleepUs = kTestSleepNs / 1'000;
// A very long duration for testing interruption.
const long kLongSleepSec = 10;  // 10 seconds

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

// Note: The return value EINTR is not tested here because Starboard does not
// implement signal handling.

// Test successful relative sleep with CLOCK_MONOTONIC
TEST(PosixClockNanosleepTest, RelativeSleepMonotonicClock) {
  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec req = {0, kTestSleepNs};  // 50 milliseconds
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful sleep, got: " << strerror(ret);

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = timeval_diff_us(&start_time, &end_time);
  // Check that the elapsed time is at least the requested sleep time.
  // Due to system scheduling, it might sleep slightly longer.
  EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

// Test successful relative sleep with CLOCK_REALTIME
TEST(PosixClockNanosleepTest, RelativeSleepRealtimeClock) {
  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec req = {0, kTestSleepNs};  // 50 milliseconds
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_REALTIME, 0, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful sleep, got: " << strerror(ret);

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = timeval_diff_us(&start_time, &end_time);
  // Check that the elapsed time is at least the requested sleep time.
  // Due to system scheduling, it might sleep slightly longer.
  EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

// Test successful absolute sleep with CLOCK_MONOTONIC
TEST(PosixClockNanosleepTest, AbsoluteSleepMonotonicClock) {
  struct timespec req;
  clockid_t clock_id = CLOCK_MONOTONIC;

  ASSERT_EQ(0, clock_gettime(clock_id, &req))
      << "Failed to get current time for CLOCK_MONOTONIC";

  // Set an absolute time 50ms in the future
  req.tv_nsec += kTestSleepNs;
  if (req.tv_nsec >= 1000000000) {
    req.tv_sec++;
    req.tv_nsec -= 1000000000;
  }

  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec rem;  // Should not be used for absolute sleep
  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful absolute sleep, got: "
                    << strerror(ret);

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = timeval_diff_us(&start_time, &end_time);
  // Check that the elapsed time is at least the requested sleep time.
  // Due to system scheduling, it might sleep slightly longer.
  EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

// Test successful absolute sleep with CLOCK_REALTIME
TEST(PosixClockNanosleepTest, AbsoluteSleepRealtimeClock) {
  struct timespec req;
  clockid_t clock_id = CLOCK_REALTIME;

  ASSERT_EQ(0, clock_gettime(clock_id, &req))
      << "Failed to get current time for CLOCK_REALTIME";

  // Set an absolute time 50ms in the future
  req.tv_nsec += kTestSleepNs;
  if (req.tv_nsec >= 1000000000) {
    req.tv_sec++;
    req.tv_nsec -= 1000000000;
  }

  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec rem;  // Should not be used for absolute sleep
  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected successful absolute sleep, got: "
                    << strerror(ret);

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = timeval_diff_us(&start_time, &end_time);
  // Check that the elapsed time is at least the requested sleep time.
  // Due to system scheduling, it might sleep slightly longer.
  EXPECT_GE(elapsed_us, static_cast<long>(kTestSleepUs))
      << "Sleep duration was too short. Requested: " << kTestSleepUs
      << "us, Elapsed: " << elapsed_us << "us.";
}

// Test immediate return for absolute sleep when requested time is in the past
TEST(PosixClockNanosleepTest, AbsoluteSleepTimeInPastReturnsImmediately) {
  struct timespec req;
  clockid_t clock_id = CLOCK_MONOTONIC;
  ASSERT_EQ(0, clock_gettime(clock_id, &req))
      << "Failed to get current time for CLOCK_MONOTONIC";

  // Set an absolute time in the past (or very near present)
  if (req.tv_nsec >= kShortSleepNs) {
    req.tv_nsec -= kShortSleepNs;
  } else if (req.tv_sec > 0) {
    req.tv_sec--;
    req.tv_nsec += (1000000000 - kShortSleepNs);
  }
  // else req is very close to 0,0 so it's effectively in the past or immediate

  struct timeval start_time;
  ASSERT_EQ(0, gettimeofday(&start_time, nullptr))
      << "gettimeofday failed for start_time";

  struct timespec rem;
  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, &rem);
  EXPECT_EQ(0, ret) << "Expected immediate return for past absolute time, got: "
                    << strerror(ret);

  struct timeval end_time;
  ASSERT_EQ(0, gettimeofday(&end_time, nullptr))
      << "gettimeofday failed for end_time";

  long elapsed_us = timeval_diff_us(&start_time, &end_time);
  // Check that the elapsed time is at least the requested sleep time.
  // Due to system scheduling, it might sleep slightly longer.
  EXPECT_LT(elapsed_us, static_cast<long>(kShortSleepUs))
      << "Sleep duration was too long. Requested time in the past, Elapsed: "
      << elapsed_us << "us.";
}

// Test EINVAL for invalid nanoseconds in request (negative)
// Linux man page for clock_nanosleep(2) states for EINVAL: "The value in the
// tv_nsec field was not in the range 0 to 999,999,999 or tv_sec was negative."
// This applies to the 'request' timespec.
TEST(PosixClockNanosleepTest, ErrorEinvalRequestNsNegative) {
  struct timespec req = {0, -1};  // Invalid nanoseconds
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for negative ns, got: "
                         << strerror(ret);
}

// Test EINVAL for invalid nanoseconds in request (too large)
// Linux man page for clock_nanosleep(2) states for EINVAL: "The value in the
// tv_nsec field was not in the range 0 to 999,999,999 or tv_sec was negative."
// This applies to the 'request' timespec.
TEST(PosixClockNanosleepTest, ErrorEinvalRequestNsTooLarge) {
  struct timespec req = {0, 1000000000};  // Invalid nanoseconds (>= 10^9)
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for ns >= 10^9, got: "
                         << strerror(ret);
}

// Test EINVAL for negative seconds in request with TIMER_ABSTIME
// This specific condition (tv_sec < 0 for TIMER_ABSTIME) is not explicitly
// listed as EINVAL in POSIX for clock_nanosleep, but it's implied by "rqtp
// argument is outside the range for the clock". However, a negative absolute
// time might be valid for some clocks if their epoch allows it, though
// practically it would be in the past and return immediately. Linux man page
// for clock_nanosleep(2) states for EINVAL: "The value in the tv_nsec field was
// not in the range 0 to 999,999,999 or tv_sec was negative." This applies to
// the 'request' timespec.
TEST(PosixClockNanosleepTest, ErrorEinvalRequestSecondsNegative) {
  struct timespec req = {-1, 0};  // Invalid seconds
  struct timespec rem;
  // For relative sleep, a negative tv_sec is an invalid interval.
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret)
      << "Expected EINVAL for negative tv_sec in relative sleep, got: "
      << strerror(ret);
}

// Test EINVAL for invalid clock_id
TEST(PosixClockNanosleepTest, ErrorEinvalInvalidClockId) {
  struct timespec req = {0, kShortSleepNs};  // 1 millisecond
  struct timespec rem;
  clockid_t bad_clock_id = -123;  // An unlikely to be valid clock ID
  int ret = clock_nanosleep(bad_clock_id, 0, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for invalid clock_id, got: "
                         << strerror(ret);
}

// Test EINVAL for invalid flags
TEST(PosixClockNanosleepTest, ErrorEinvalInvalidFlags) {
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)  //
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  struct timespec req = {0, kShortSleepNs};
  struct timespec rem;
  int invalid_flags = 0xFF;  // Some likely invalid flags
  // POSIX only defines TIMER_ABSTIME. Other bits should be 0.
  // Some systems might tolerate other bits if TIMER_ABSTIME is correctly
  // set/unset. The most reliable way to get EINVAL for flags is usually an
  // undefined flag value. If flags has bits set other than TIMER_ABSTIME, it's
  // technically invalid.
  int ret = clock_nanosleep(CLOCK_MONOTONIC, invalid_flags, &req, &rem);
  EXPECT_EQ(EINVAL, ret) << "Expected EINVAL for invalid flags, got: "
                         << strerror(ret);
}

// Test ENOTSUP for a clock that might not be supported for nanosleep,
// like a CPU-time clock.
TEST(PosixClockNanosleepTest, ErrorEnotsupForProcessCpuClock) {
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)  //
  GTEST_SKIP() << "Non-hermetic builds fail this test.";
#endif
  // If CLOCK_PROCESS_CPUTIME_ID itself is not supported for clock_nanosleep
  // or if it specifically refers to the calling process's CPU time, it should
  // give ENOTSUP.
  struct timespec req = {0, kShortSleepNs};
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_PROCESS_CPUTIME_ID, 0, &req, &rem);

  // Some systems might return EINVAL if CLOCK_PROCESS_CPUTIME_ID is not a
  // "known clock" in this context, or ENOTSUP if it's known but not usable for
  // sleep. The man page is more specific about ENOTSUP for CPU-time clocks.
  EXPECT_TRUE(ret == ENOTSUP || ret == EINVAL)
      << "Expected ENOTSUP or EINVAL for CLOCK_PROCESS_CPUTIME_ID, got: "
      << strerror(ret);
}

// Test ENOTSUP for a clock that might not be supported for nanosleep,
// like a CPU-time clock.
TEST(PosixClockNanosleepTest, ErrorEnotsupForThreadCpuClock) {
  // If CLOCK_PROCESS_CPUTIME_ID itself is not supported for clock_nanosleep
  // or if it specifically refers to the calling process's CPU time, it should
  // give ENOTSUP.
  struct timespec req = {0, kShortSleepNs};
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_THREAD_CPUTIME_ID, 0, &req, &rem);

  // Some systems might return EINVAL if CLOCK_PROCESS_CPUTIME_ID is not a
  // "known clock" in this context, or ENOTSUP if it's known but not usable for
  // sleep. The man page is more specific about ENOTSUP for CPU-time clocks.
  EXPECT_TRUE(ret == ENOTSUP || ret == EINVAL)
      << "Expected ENOTSUP or EINVAL for CLOCK_THREAD_CPUTIME_ID, got: "
      << strerror(ret);
}

// Test behavior with NULL remain and relative sleep (should be fine)
TEST(PosixClockNanosleepTest, RelativeSleepNullRemain) {
  struct timespec req = {0, kShortSleepNs};  // 1 millisecond
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, nullptr);
  EXPECT_EQ(0, ret) << "Expected successful sleep with NULL remain, got: "
                    << strerror(ret);
}

// Test behavior with NULL remain and absolute sleep (should be fine)
TEST(PosixClockNanosleepTest, AbsoluteSleepNullRemain) {
  struct timespec req;
  clockid_t clock_id = CLOCK_MONOTONIC;
  ASSERT_EQ(0, clock_gettime(clock_id, &req));
  req.tv_nsec += kShortSleepNs;  // Sleep for 1ms in the future
  if (req.tv_nsec >= 1000000000) {
    req.tv_sec++;
    req.tv_nsec -= 1000000000;
  }
  int ret = clock_nanosleep(clock_id, TIMER_ABSTIME, &req, nullptr);
  EXPECT_EQ(0, ret)
      << "Expected successful absolute sleep with NULL remain, got: "
      << strerror(ret);
}

// Test relative sleep with zero duration (should return immediately)
TEST(PosixClockNanosleepTest, RelativeSleepZeroDuration) {
  struct timespec req = {0, 0};  // Zero duration
  struct timespec rem;
  int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);
  EXPECT_EQ(0, ret)
      << "Expected immediate return for zero duration relative sleep, got: "
      << strerror(ret);
}

}  // namespace.
}  // namespace nplb.
}  // namespace starboard.
