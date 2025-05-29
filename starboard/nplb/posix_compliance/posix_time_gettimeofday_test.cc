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
#include <sys/time.h>
#include <time.h>
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// kReasonableMinTime represents a time (2025-01-01 00:00:00 UTC) after which
// the current time is expected to fall.
const time_t kReasonableMinTime = 1735689600;  // 2025-01-01 00:00:00 UTC

TEST(PosixTimeGettimeofdayTests, SunnyDay) {
  struct timeval tv;
  memset(&tv, 0, sizeof(tv));
  errno = 0;
  ASSERT_EQ(0, gettimeofday(&tv, nullptr))
      << "gettimeofday() failed. errno: " << errno;

  EXPECT_GT(tv.tv_sec, kReasonableMinTime)
      << "Current time (" << tv.tv_sec
      << ") is unexpectedly earlier than kReasonableMinTime ("
      << kReasonableMinTime << ").";
  EXPECT_GE(tv.tv_usec, 0) << "tv_usec should be non-negative.";
  EXPECT_LE(tv.tv_usec, 999999) << "tv_usec should be less than 1,000,000.";
}

TEST(PosixTimeGettimeofdayTests, NonNullTzIsIgnoredOrCausesEinval) {
  struct timeval tv;
  memset(&tv, 0, sizeof(tv));
  struct timezone tz;
  memset(&tz, 0, sizeof(tz));

  errno = 0;
  int result = gettimeofday(&tv, &tz);

  if (result == -1) {
    EXPECT_EQ(EINVAL, errno)
        << "Expected EINVAL when tz is non-nullptr and not supported, "
        << "but got errno: " << errno;
  } else {
    EXPECT_EQ(0, result)
        << "gettimeofday with non-nullptr tz failed with an unexpected result "
        << result << " and errno: " << errno;
    EXPECT_GE(tv.tv_sec, kReasonableMinTime);
    EXPECT_GE(tv.tv_usec, 0);
    EXPECT_LE(tv.tv_usec, 999999);
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
TEST(PosixTimeGettimeofdayTests, GettimeofdayHasDecentResolution) {
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

    const int64_t kMaxWaitMicroseconds = 1'000'000LL;
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

      if ((monotonicTimeNow - timerStart) >= kMaxWaitMicroseconds) {
        FAIL() << "gettimeofday()-based time hasn't changed within "
               << (kMaxWaitMicroseconds / 1000L) << " ms "
               << "during iteration " << i << ". Initial time: " << initialTime
               << ", Current time (monotonic): " << monotonicTimeNow
               << ", Timer start (monotonic): " << timerStart;
        return;
      }
    }
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
