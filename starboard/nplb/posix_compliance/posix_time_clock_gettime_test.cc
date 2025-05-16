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
#include <sys/time.h>  // For struct timespec (though time.h is primary)
#include <time.h>      // For clock_gettime, CLOCK_* constants
#include "testing/gtest/include/gtest/gtest.h"

// For CPU work in relevant tests
#include <cmath>  // For std::sin, std::cos

namespace starboard {
namespace nplb {
namespace {

// Test fixture for POSIX clock_gettime() function.
class PosixTimeClockGettimeTests : public ::testing::Test {};

// Helper function to convert timespec to microseconds for easier comparison.
static int64_t TimespecToMicroseconds(const struct timespec& ts) {
  return (static_cast<int64_t>(ts.tv_sec) * 1000000LL) +
         (static_cast<int64_t>(ts.tv_nsec) / 1000LL);
}

// Helper function to perform some CPU-intensive work.
// This aims to consume enough CPU time to be measurable by CPU-time clocks.
static void PerformCpuWork() {
  // This value is chosen to ensure some measurable CPU time is consumed
  // on typical test systems without taking excessively long.
  // It may need adjustment if tests are flaky on very fast/slow systems
  // or systems with very coarse CPU clock resolution.
  const int kWorkIterations = 150000;
  volatile double val = 0.0;  // Volatile to help prevent optimization
  for (int i = 0; i < kWorkIterations; ++i) {
    // Perform some floating point operations
    val += std::sin(static_cast<double>(i)) * std::cos(static_cast<double>(i));
  }
}

// --- Success and Behavior Tests for Standard Clocks ---

// Tests that clock_gettime(CLOCK_MONOTONIC) succeeds and is indeed monotonic.
TEST_F(PosixTimeClockGettimeTests, ClockMonotonicSucceedsAndIsMonotonic) {
  // Number of trials to check for monotonicity. Chosen to be reasonably
  // thorough without making the test too slow.
  const int kTrials = 100;

  struct timespec ts_prev;
  memset(&ts_prev, 0, sizeof(ts_prev));
  ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &ts_prev))
      << "Initial clock_gettime(CLOCK_MONOTONIC) failed. errno: " << errno;
  int64_t previousMonotonicTime = TimespecToMicroseconds(ts_prev);

  for (int trial = 0; trial < kTrials; ++trial) {
    struct timespec ts_curr;
    memset(&ts_curr, 0, sizeof(ts_curr));
    ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &ts_curr))
        << "clock_gettime(CLOCK_MONOTONIC) in loop failed for trial " << trial
        << ". errno: " << errno;
    int64_t currentMonotonicTime = TimespecToMicroseconds(ts_curr);

    EXPECT_GE(currentMonotonicTime, previousMonotonicTime)
        << "clock_gettime(CLOCK_MONOTONIC)-based time should be "
           "non-decreasing. Previous: "
        << previousMonotonicTime << ", Current: " << currentMonotonicTime
        << " on trial " << trial;

    if (currentMonotonicTime < previousMonotonicTime) {
      FAIL()
          << "clock_gettime(CLOCK_MONOTONIC)-based time DECREASED. Previous: "
          << previousMonotonicTime << ", Current: " << currentMonotonicTime
          << " on trial " << trial;
      return;  // Exit test immediately
    }
    previousMonotonicTime = currentMonotonicTime;
  }
  SUCCEED() << "clock_gettime(CLOCK_MONOTONIC)-based time maintained "
               "non-decreasing behavior over "
            << kTrials << " trials.";
}

// Tests that clock_gettime(CLOCK_REALTIME) succeeds.
TEST_F(PosixTimeClockGettimeTests, ClockRealtimeSucceeds) {
  struct timespec ts;
  memset(&ts, 0, sizeof(ts));
  errno = 0;  // Clear errno before call
  EXPECT_EQ(0, clock_gettime(CLOCK_REALTIME, &ts))
      << "clock_gettime(CLOCK_REALTIME) failed. errno: " << errno;
  // Basic validation of timespec fields.
  // tv_sec can be negative for dates before the Epoch, but typically positive.
  EXPECT_GE(ts.tv_nsec, 0);
  EXPECT_LT(ts.tv_nsec,
            1000000000L);  // Nanoseconds must be less than 1 billion.
}

// Tests that clock_gettime(CLOCK_PROCESS_CPUTIME_ID) succeeds and increases
// after work.
TEST_F(PosixTimeClockGettimeTests, ClockProcessCpuTimeSucceedsAndIncreases) {
  struct timespec ts_before, ts_after;
  memset(&ts_before, 0, sizeof(ts_before));
  memset(&ts_after, 0, sizeof(ts_after));

  errno = 0;  // Clear errno
  int ret_before = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts_before);
  if (ret_before == -1 && errno == EINVAL) {
    GTEST_SKIP() << "CLOCK_PROCESS_CPUTIME_ID not supported on this system "
                    "(returned EINVAL).";
    return;
  }
  ASSERT_EQ(0, ret_before)
      << "Initial clock_gettime(CLOCK_PROCESS_CPUTIME_ID) failed. errno: "
      << errno;

  PerformCpuWork();

  errno = 0;  // Clear errno
  ASSERT_EQ(0, clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts_after))
      << "Final clock_gettime(CLOCK_PROCESS_CPUTIME_ID) failed. errno: "
      << errno;

  int64_t time_before_us = TimespecToMicroseconds(ts_before);
  int64_t time_after_us = TimespecToMicroseconds(ts_after);

  EXPECT_GE(time_after_us, time_before_us)
      << "Process CPU time should be non-decreasing. Before: " << time_before_us
      << " us, After: " << time_after_us << " us.";
  // Note: A strict increase (>) is ideal, but GE (>=) is used for robustness
  // due to clock resolution and minimal work variance.
}

// Tests that clock_gettime(CLOCK_THREAD_CPUTIME_ID) succeeds and increases
// after work.
TEST_F(PosixTimeClockGettimeTests, ClockThreadCpuTimeSucceedsAndIncreases) {
  struct timespec ts_before, ts_after;
  memset(&ts_before, 0, sizeof(ts_before));
  memset(&ts_after, 0, sizeof(ts_after));

  errno = 0;  // Clear errno
  int ret_before = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts_before);
  if (ret_before == -1 && errno == EINVAL) {
    GTEST_SKIP() << "CLOCK_THREAD_CPUTIME_ID not supported on this system "
                    "(returned EINVAL).";
    return;
  }
  ASSERT_EQ(0, ret_before)
      << "Initial clock_gettime(CLOCK_THREAD_CPUTIME_ID) failed. errno: "
      << errno;

  PerformCpuWork();

  errno = 0;  // Clear errno
  ASSERT_EQ(0, clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts_after))
      << "Final clock_gettime(CLOCK_THREAD_CPUTIME_ID) failed. errno: "
      << errno;

  int64_t time_before_us = TimespecToMicroseconds(ts_before);
  int64_t time_after_us = TimespecToMicroseconds(ts_after);

  EXPECT_GE(time_after_us, time_before_us)
      << "Thread CPU time should be non-decreasing. Before: " << time_before_us
      << " us, After: " << time_after_us << " us.";
}

// --- Tests for Optional Clocks (Availability May Vary) ---

#ifdef CLOCK_MONOTONIC_RAW
TEST_F(PosixTimeClockGettimeTests, ClockMonotonicRawSucceedsAndIsMonotonic) {
  const int kTrials = 100;  // Number of trials for monotonicity.
  struct timespec ts_prev, ts_curr;

  memset(&ts_prev, 0, sizeof(ts_prev));
  errno = 0;  // Clear errno
  int ret_initial = clock_gettime(CLOCK_MONOTONIC_RAW, &ts_prev);
  if (ret_initial == -1 && errno == EINVAL) {
    GTEST_SKIP() << "CLOCK_MONOTONIC_RAW defined but not supported by kernel "
                    "(returned EINVAL).";
    return;
  }
  ASSERT_EQ(0, ret_initial)
      << "Initial clock_gettime(CLOCK_MONOTONIC_RAW) failed. errno: " << errno;

  int64_t previousTime = TimespecToMicroseconds(ts_prev);

  for (int trial = 0; trial < kTrials; ++trial) {
    memset(&ts_curr, 0, sizeof(ts_curr));
    errno = 0;  // Clear errno
    ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC_RAW, &ts_curr))
        << "clock_gettime(CLOCK_MONOTONIC_RAW) in loop failed for trial "
        << trial << ". errno: " << errno;
    int64_t currentTime = TimespecToMicroseconds(ts_curr);
    EXPECT_GE(currentTime, previousTime)
        << "CLOCK_MONOTONIC_RAW time should be non-decreasing. Previous: "
        << previousTime << ", Current: " << currentTime << " on trial "
        << trial;
    if (currentTime < previousTime) {
      FAIL() << "CLOCK_MONOTONIC_RAW time DECREASED. Previous: " << previousTime
             << ", Current: " << currentTime << " on trial " << trial;
      return;
    }
    previousTime = currentTime;
  }
  SUCCEED();
}
#else   // CLOCK_MONOTONIC_RAW
TEST_F(PosixTimeClockGettimeTests,
       ClockMonotonicRawNotTestedDueToMissingDefine) {
  GTEST_SKIP() << "CLOCK_MONOTONIC_RAW not defined, skipping test.";
}
#endif  // CLOCK_MONOTONIC_RAW

#ifdef CLOCK_REALTIME_COARSE
TEST_F(PosixTimeClockGettimeTests, ClockRealtimeCoarseSucceeds) {
  struct timespec ts;
  memset(&ts, 0, sizeof(ts));
  errno = 0;  // Clear errno
  int ret = clock_gettime(CLOCK_REALTIME_COARSE, &ts);
  if (ret == -1 && errno == EINVAL) {
    GTEST_SKIP() << "CLOCK_REALTIME_COARSE defined but not supported by kernel "
                    "(returned EINVAL).";
    return;
  }
  ASSERT_EQ(0, ret) << "clock_gettime(CLOCK_REALTIME_COARSE) failed. errno: "
                    << errno;
  EXPECT_GE(ts.tv_nsec, 0);
  EXPECT_LT(ts.tv_nsec, 1000000000L);
  SUCCEED();
}
#else   // CLOCK_REALTIME_COARSE
TEST_F(PosixTimeClockGettimeTests,
       ClockRealtimeCoarseNotTestedDueToMissingDefine) {
  GTEST_SKIP() << "CLOCK_REALTIME_COARSE not defined, skipping test.";
}
#endif  // CLOCK_REALTIME_COARSE

#ifdef CLOCK_MONOTONIC_COARSE
TEST_F(PosixTimeClockGettimeTests, ClockMonotonicCoarseSucceedsAndIsMonotonic) {
  const int kTrials = 100;  // Number of trials for monotonicity.
  struct timespec ts_prev, ts_curr;

  memset(&ts_prev, 0, sizeof(ts_prev));
  errno = 0;  // Clear errno
  int ret_initial = clock_gettime(CLOCK_MONOTONIC_COARSE, &ts_prev);
  if (ret_initial == -1 && errno == EINVAL) {
    GTEST_SKIP() << "CLOCK_MONOTONIC_COARSE defined but not supported by "
                    "kernel (returned EINVAL).";
    return;
  }
  ASSERT_EQ(0, ret_initial)
      << "Initial clock_gettime(CLOCK_MONOTONIC_COARSE) failed. errno: "
      << errno;

  int64_t previousTime = TimespecToMicroseconds(ts_prev);

  for (int trial = 0; trial < kTrials; ++trial) {
    memset(&ts_curr, 0, sizeof(ts_curr));
    errno = 0;  // Clear errno
    ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC_COARSE, &ts_curr))
        << "clock_gettime(CLOCK_MONOTONIC_COARSE) in loop failed for trial "
        << trial << ". errno: " << errno;
    int64_t currentTime = TimespecToMicroseconds(ts_curr);
    EXPECT_GE(currentTime, previousTime)
        << "CLOCK_MONOTONIC_COARSE time should be non-decreasing. Previous: "
        << previousTime << ", Current: " << currentTime << " on trial "
        << trial;
    if (currentTime < previousTime) {
      FAIL() << "CLOCK_MONOTONIC_COARSE time DECREASED. Previous: "
             << previousTime << ", Current: " << currentTime << " on trial "
             << trial;
      return;
    }
    previousTime = currentTime;
  }
  SUCCEED();
}
#else   // CLOCK_MONOTONIC_COARSE
TEST_F(PosixTimeClockGettimeTests,
       ClockMonotonicCoarseNotTestedDueToMissingDefine) {
  GTEST_SKIP() << "CLOCK_MONOTONIC_COARSE not defined, skipping test.";
}
#endif  // CLOCK_MONOTONIC_COARSE

#ifdef CLOCK_BOOTTIME
TEST_F(PosixTimeClockGettimeTests, ClockBoottimeSucceedsAndIsMonotonic) {
  const int kTrials = 100;  // Number of trials for monotonicity.
  struct timespec ts_prev, ts_curr;

  memset(&ts_prev, 0, sizeof(ts_prev));
  errno = 0;  // Clear errno
  int ret_initial = clock_gettime(CLOCK_BOOTTIME, &ts_prev);
  if (ret_initial == -1 && errno == EINVAL) {
    GTEST_SKIP() << "CLOCK_BOOTTIME defined but not supported by kernel "
                    "(returned EINVAL).";
    return;
  }
  ASSERT_EQ(0, ret_initial)
      << "Initial clock_gettime(CLOCK_BOOTTIME) failed. errno: " << errno;

  int64_t previousTime = TimespecToMicroseconds(ts_prev);

  for (int trial = 0; trial < kTrials; ++trial) {
    memset(&ts_curr, 0, sizeof(ts_curr));
    errno = 0;  // Clear errno
    ASSERT_EQ(0, clock_gettime(CLOCK_BOOTTIME, &ts_curr))
        << "clock_gettime(CLOCK_BOOTTIME) in loop failed for trial " << trial
        << ". errno: " << errno;
    int64_t currentTime = TimespecToMicroseconds(ts_curr);
    EXPECT_GE(currentTime, previousTime)
        << "CLOCK_BOOTTIME time should be non-decreasing. Previous: "
        << previousTime << ", Current: " << currentTime << " on trial "
        << trial;
    if (currentTime < previousTime) {
      FAIL() << "CLOCK_BOOTTIME time DECREASED. Previous: " << previousTime
             << ", Current: " << currentTime << " on trial " << trial;
      return;
    }
    previousTime = currentTime;
  }
  SUCCEED();
}
#else   // CLOCK_BOOTTIME
TEST_F(PosixTimeClockGettimeTests, ClockBoottimeNotTestedDueToMissingDefine) {
  GTEST_SKIP() << "CLOCK_BOOTTIME not defined, skipping test.";
}
#endif  // CLOCK_BOOTTIME

#ifdef CLOCK_TAI
TEST_F(PosixTimeClockGettimeTests, ClockTaiSucceeds) {
  struct timespec ts;
  memset(&ts, 0, sizeof(ts));
  errno = 0;  // Clear errno
  int ret = clock_gettime(CLOCK_TAI, &ts);
  if (ret == -1 && errno == EINVAL) {
    GTEST_SKIP()
        << "CLOCK_TAI defined but not supported by kernel (returned EINVAL).";
    return;
  }
  ASSERT_EQ(0, ret) << "clock_gettime(CLOCK_TAI) failed. errno: " << errno;
  EXPECT_GE(ts.tv_nsec, 0);
  EXPECT_LT(ts.tv_nsec, 1000000000L);
  SUCCEED();
}
#else   // CLOCK_TAI
TEST_F(PosixTimeClockGettimeTests, ClockTaiNotTestedDueToMissingDefine) {
  GTEST_SKIP() << "CLOCK_TAI not defined, skipping test.";
}
#endif  // CLOCK_TAI

// --- Error Condition Tests ---

#if SB_IS_EVERGREEN
//  Tests that clock_gettime returns EFAULT when the timespec pointer is null.
TEST_F(PosixTimeClockGettimeTests, ReturnsEfaultForNullTimespec) {
  // EFAULT: tp points outside the accessible address space.
  // Passing nullptr for tp should trigger this.
  // CLOCK_MONOTONIC is used as a valid clock ID for this check.
  errno = 0;  // Clear errno before the call
  EXPECT_EQ(-1, clock_gettime(CLOCK_MONOTONIC, nullptr));
  EXPECT_EQ(EFAULT, errno);
}
#endif

// Tests that clock_gettime returns EINVAL for an invalid clock ID.
TEST_F(PosixTimeClockGettimeTests, ReturnsEinvalForInvalidClockId) {
  struct timespec ts;
  memset(&ts, 0, sizeof(ts));
  // EINVAL: The clk_id specified is not supported on this system.

  // A large positive integer unlikely to be a valid clock ID.
  // Chosen to be distinct and clearly not a standard clock_id.
  const clockid_t kInvalidPositiveClockId = 0xDEFEC8ED;
  errno = 0;  // Clear errno
  EXPECT_EQ(-1, clock_gettime(kInvalidPositiveClockId, &ts));
  EXPECT_EQ(EINVAL, errno);

  // A negative integer unlikely to be a valid clock ID (other than special
  // ones). Chosen to be a simple negative value distinct from standard clock
  // IDs.
  const clockid_t kInvalidNegativeClockId = -4242;
  errno = 0;  // Clear errno
  EXPECT_EQ(-1, clock_gettime(kInvalidNegativeClockId, &ts));
  EXPECT_EQ(EINVAL, errno);
}

// Comment on EPERM:
// The EPERM error is generally not applicable to clock_gettime() for reading
// standard, publicly accessible clocks (like CLOCK_REALTIME, CLOCK_MONOTONIC)
// or the calling process/thread's CPU time (CLOCK_PROCESS_CPUTIME_ID,
// CLOCK_THREAD_CPUTIME_ID). EPERM might arise if trying to get a CPU-time
// clock ID for *another* process (obtained via clock_getcpuclockid()) for which
// the caller lacks permissions. Testing that scenario is more complex as it
// involves inter-process permissions and multiple function calls, and is
// outside the direct scope of testing clock_gettime with its predefined clock
// IDs. Therefore, a specific EPERM test for clock_gettime with these standard
// IDs is not included.

}  // namespace
}  // namespace nplb
}  // namespace starboard
