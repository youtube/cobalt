// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <errno.h>     // For errno, EFAULT, EINVAL
#include <string.h>    // For memset
#include <sys/time.h>  // For gettimeofday, struct timeval, struct timezone
#include <time.h>      // For CLOCK_MONOTONIC, struct timespec, clock_gettime
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Test fixture for POSIX gettimeofday() function.
// Class name updated to PosixTimeGettimeofdayTests as per style guidelines.
class PosixTimeGettimeofdayTests : public ::testing::Test {};

// Tests basic successful operation of gettimeofday().
TEST_F(PosixTimeGettimeofdayTests, SunnyDay) {
  struct timeval tv;
  memset(&tv, 0, sizeof(tv));
  errno = 0;  // Clear errno before the call.
  ASSERT_EQ(0, gettimeofday(&tv, nullptr))
      << "gettimeofday() failed. errno: " << errno;

  // tv_sec should be a large positive number (seconds since Epoch).
  // We expect time to be after year 2000 to ensure it's a sensible value.
  // 2000-01-01 00:00:00 UTC = 946684800 seconds since Epoch.
  const time_t kMinExpectedEpochTime = 946684800L;
  EXPECT_GE(tv.tv_sec, kMinExpectedEpochTime)
      << "tv_sec (" << tv.tv_sec << ") is unexpectedly small, "
      << "expected at least " << kMinExpectedEpochTime
      << " (seconds since Epoch for 2000-01-01 UTC). "
      << "This might indicate an issue with the system clock or "
      << "gettimeofday implementation.";
  EXPECT_GE(tv.tv_usec, 0) << "tv_usec should be non-negative.";
  EXPECT_LE(tv.tv_usec, 999999) << "tv_usec should be less than 1,000,000.";
}

#if SB_IS_EVERGREEN
// Tests that passing nullptr for the timeval struct results in EFAULT.
TEST_F(PosixTimeGettimeofdayTests, NullTvCausesEfault) {
  errno = 0;  // Clear errno before the call.
  // Passing nullptr for the tv argument is an explicit error condition.
  struct timeval* null_tv = nullptr;
  EXPECT_EQ(-1, gettimeofday(null_tv, null_tv));
  EXPECT_EQ(EFAULT, errno)
      << "Expected EFAULT when tv argument is nullptr, but got errno: "
      << errno;
}
#endif

// Tests the behavior when the timezone struct (tz) is not nullptr.
// POSIX.1-2008 states gettimeofday() shall fail with EINVAL if tz is
// non-nullptr and the system does not support the concept of timezones via this
// parameter (which is the common case as tz is obsolete). Otherwise, the
// behavior is unspecified; many systems ignore tz.
TEST_F(PosixTimeGettimeofdayTests, NonNullTzIsIgnoredOrCausesEinval) {
  struct timeval tv;
  memset(&tv, 0, sizeof(tv));
  struct timezone tz;  // A valid, stack-allocated timezone struct.
  memset(
      &tz, 0,
      sizeof(
          tz));  // Initialize to defined values (e.g., 0 minutes West, 0 DST).

  errno = 0;  // Clear errno before the call.
  int result = gettimeofday(&tv, &tz);

  if (result == -1) {
    // This is the behavior expected by POSIX.1-2008 if the implementation
    // does not support the tz parameter.
    EXPECT_EQ(EINVAL, errno)
        << "Expected EINVAL when tz is non-nullptr and not supported, "
        << "but got errno: " << errno;
  } else {
    // Some systems might ignore the tz argument and succeed.
    EXPECT_EQ(0, result)
        << "gettimeofday with non-nullptr tz failed with an unexpected result "
        << result << " and errno: " << errno;
    // If it succeeded, check that tv was populated reasonably,
    // similar to the SunnyDay test.
    const time_t kMinExpectedEpochTime = 946684800L;  // 2000-01-01 00:00:00 UTC
    EXPECT_GE(tv.tv_sec, kMinExpectedEpochTime);
    EXPECT_GE(tv.tv_usec, 0);
    EXPECT_LE(tv.tv_usec, 999999);
    // This path indicates the system likely ignores the tz argument.
    // While POSIX suggests failure, ignoring it is a common unspecified
    // behavior.
    SUCCEED() << "gettimeofday with non-nullptr tz returned 0 (tz was likely "
                 "ignored).";
  }
}

// Note on EFAULT for tz:
// EFAULT could occur if tz is a non-nullptr, invalid pointer, AND the system
// attempts to dereference it. However, the tz parameter is obsolete.
// POSIX.1-2008 specifies that gettimeofday() should fail with EINVAL if tz
// is non-nullptr and timezones are not supported through this interface.
// Most implementations will likely check if tz is non-nullptr and either return
// EINVAL or ignore tz without dereferencing it. Therefore, reliably
// triggering EFAULT due to an invalid tz pointer (while tv is valid) is
// difficult to test portably and is not a primary compliance concern since
// tz should always be nullptr in modern usage.

// Tests the resolution of time obtained via gettimeofday().
TEST_F(PosixTimeGettimeofdayTests, GettimeofdayHasDecentResolution) {
  // Number of iterations to check for time change.
  // Chosen to be large enough to observe time changes on typical systems
  // but small enough to not make the test excessively long.
  const int kNumIterations = 100;
  for (int i = 0; i < kNumIterations; ++i) {
    struct timespec monotonic_ts_start;
    memset(&monotonic_ts_start, 0, sizeof(monotonic_ts_start));
    ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &monotonic_ts_start))
        << "clock_gettime(CLOCK_MONOTONIC) for timerStart failed. errno: "
        << errno;
    int64_t timerStart =
        (static_cast<int64_t>(monotonic_ts_start.tv_sec) * 1000000LL) +
        (static_cast<int64_t>(monotonic_ts_start.tv_nsec) / 1000LL);

    struct timeval posix_tv_initial;
    memset(&posix_tv_initial, 0, sizeof(posix_tv_initial));
    ASSERT_EQ(0, gettimeofday(&posix_tv_initial, nullptr))
        << "gettimeofday() for initialTime failed. errno: " << errno;
    int64_t initialTime =
        (static_cast<int64_t>(posix_tv_initial.tv_sec) * 1000000LL) +
        posix_tv_initial.tv_usec;

    int64_t currentTime = initialTime;
    // Maximum wait time for the clock to advance, in microseconds.
    // Set to 1 second (1,000,000 microseconds). If gettimeofday() doesn't show
    // a change within this period, the resolution is considered too low or
    // stuck.
    const int64_t kMaxWaitMicroseconds = 1000000LL;
    while (currentTime == initialTime) {
      struct timeval posix_tv_current;
      memset(&posix_tv_current, 0, sizeof(posix_tv_current));
      ASSERT_EQ(0, gettimeofday(&posix_tv_current, nullptr))
          << "gettimeofday() in loop failed. errno: " << errno;
      currentTime =
          (static_cast<int64_t>(posix_tv_current.tv_sec) * 1000000LL) +
          posix_tv_current.tv_usec;

      struct timespec monotonic_ts_current;
      memset(&monotonic_ts_current, 0, sizeof(monotonic_ts_current));
      ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &monotonic_ts_current))
          << "clock_gettime(CLOCK_MONOTONIC) in loop failed. errno: " << errno;
      int64_t monotonicTimeNow =
          (static_cast<int64_t>(monotonic_ts_current.tv_sec) * 1000000LL) +
          (static_cast<int64_t>(monotonic_ts_current.tv_nsec) / 1000LL);

      // If time hasn't changed within kMaxWaitMicroseconds, that's beyond low
      // resolution.
      if ((monotonicTimeNow - timerStart) >= kMaxWaitMicroseconds) {
        FAIL() << "gettimeofday()-based time hasn't changed within "
               << (kMaxWaitMicroseconds / 1000L)
               << " ms "  // Convert to ms for readability
               << "during iteration " << i << ". Initial time: " << initialTime
               << ", Current time (monotonic): " << monotonicTimeNow
               << ", Timer start (monotonic): " << timerStart;
        return;  // Exit test on failure
      }
    }
  }
  SUCCEED() << "gettimeofday()-based time showed change across "
            << kNumIterations << " iterations.";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
