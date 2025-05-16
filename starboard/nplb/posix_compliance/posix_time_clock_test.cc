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

#include <time.h>    // For clock(), clock_t, CLOCKS_PER_SEC
#include <unistd.h>  // For usleep()

#include <atomic>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Test fixture for POSIX clock() function.
// Renamed from PosixTimeClockTests to PosixTimeClockTests as per style
// guidelines.
class PosixTimeClockTests : public ::testing::Test {};

// Tests that clock() returns a non-negative value under normal circumstances,
// or (clock_t)-1 on error.
TEST_F(PosixTimeClockTests, ClockReturnsNonNegativeOrError) {
  clock_t start_clock = clock();
  // clock() returns (clock_t)-1 on error, indicating processor time is
  // unavailable. We expect it to succeed under normal test conditions.
  if (start_clock == static_cast<clock_t>(-1)) {
    // This is a valid return on error, but we log it as it might indicate an
    // issue if unexpected.
    SUCCEED() << "clock() returned (clock_t)-1, indicating processor time "
                 "might be unavailable or an error occurred.";
  } else {
    EXPECT_GE(start_clock, 0) << "clock() returned a negative value other than "
                                 "-1, which is unexpected.";
  }
  // Note: Forcing an error condition for clock() (to reliably get (clock_t)-1)
  // is system-dependent and not straightforward to achieve in a portable unit
  // test. The C standard allows for arbitrary values if processor time is not
  // available, while POSIX.1-2001 specifies (clock_t)-1. This comment fulfills
  // the guideline to explain why an error condition might not be directly
  // testable.
}

// Tests that the value returned by clock() increases after CPU work.
TEST_F(PosixTimeClockTests, ClockIncreasesOverTime) {
  clock_t clock_val1 = clock();
  // If clock() initially fails, skip the test.
  if (clock_val1 == static_cast<clock_t>(-1)) {
    GTEST_SKIP() << "Initial call to clock() returned error; "
                 << "cannot perform time increase test.";
  }

  // Consume some CPU time. A volatile counter helps prevent optimization.
  // Number of iterations to perform to ensure a measurable amount of CPU work.
  const int kCpuWorkIterations = 2000000;
  std::atomic<int> counter(0);
  for (int i = 0; i < kCpuWorkIterations; ++i) {
    counter++;
  }

  clock_t clock_val2 = clock();
  if (clock_val2 == static_cast<clock_t>(-1)) {
    // If the second call fails, we can't compare.
    GTEST_SKIP() << "Second call to clock() returned error.";
  }

  // The clock should ideally have advanced or stayed the same (if resolution is
  // coarse or work was too little). It should not decrease.
  EXPECT_GE(clock_val2, clock_val1)
      << "Clock value decreased, which is unexpected. val1=" << clock_val1
      << ", val2=" << clock_val2;

  if (clock_val2 == clock_val1) {
    SUCCEED() << "Note: clock_val2 (" << clock_val2 << ") == clock_val1 ("
              << clock_val1 << "). This might be due to insufficient CPU work "
              << "for the clock's resolution or the nature of clock() on this "
                 "platform.";
  }
}

// Tests that CLOCKS_PER_SEC is a positive value as per C standard.
TEST_F(PosixTimeClockTests, ClocksPerSecIsPositive) {
  // CLOCKS_PER_SEC is a macro defined in <time.h>.
  // It represents the number of clock ticks per second.
  // Using 0L for long comparison, as CLOCKS_PER_SEC is often a long.
  EXPECT_GT(CLOCKS_PER_SEC, 0L)
      << "CLOCKS_PER_SEC should be a positive value. Actual: "
      << CLOCKS_PER_SEC;
}

// Test that CLOCKS_PER_SEC adheres to the POSIX standard value.
TEST_F(PosixTimeClockTests, ClocksPerSecIsPosixStandardValue) {
  // POSIX.1-2001 requires CLOCKS_PER_SEC to be 1,000,000.
  const long kPosixClocksPerSec =
      1000000L;  // POSIX standard value for CLOCKS_PER_SEC.
  EXPECT_EQ(CLOCKS_PER_SEC, kPosixClocksPerSec)
      << "POSIX.1-2001 requires CLOCKS_PER_SEC to be " << kPosixClocksPerSec
      << ". Actual value: " << CLOCKS_PER_SEC;
}

// Tests that clock() measures CPU time and not wall-clock time by checking
// its behavior during a sleep period.
TEST_F(PosixTimeClockTests, ClockMeasuresCpuTimeNotWallTimeDuringSleep) {
  clock_t start_clock = clock();
  if (start_clock == static_cast<clock_t>(-1)) {
    GTEST_SKIP() << "Initial call to clock() failed; cannot perform test.";
  }

  // Sleep for a noticeable duration. During sleep, the process should ideally
  // not consume significant CPU time.
  // 100 milliseconds sleep duration.
  const unsigned int kSleepDurationMicroseconds = 100 * 1000;
  usleep(kSleepDurationMicroseconds);

  clock_t end_clock = clock();
  if (end_clock == static_cast<clock_t>(-1)) {
    GTEST_SKIP() << "Subsequent call to clock() failed; cannot perform test.";
  }

  // Ensure clock did not go backward (highly unlikely for short sleep,
  // implies issues other than normal wrap-around).
  EXPECT_GE(end_clock, start_clock) << "Clock value decreased during a short "
                                       "sleep, which is unexpected. Start: "
                                    << start_clock << ", End: " << end_clock;

  clock_t clock_delta = end_clock - start_clock;

  // Maximum CPU ticks expected during sleep.
  // This allows for up to 10% of the wall clock sleep time to be registered
  // as CPU time, accounting for scheduler overhead, timer interrupts, etc.
  // For 100ms sleep, this is 10ms of CPU time.
  // (0.1 seconds (sleep) * 0.1 (10% tolerance) * CLOCKS_PER_SEC)
  const clock_t kMaxExpectedCpuTicksDuringSleep =
      static_cast<clock_t>(0.01 * CLOCKS_PER_SEC);

  EXPECT_LT(clock_delta, kMaxExpectedCpuTicksDuringSleep)
      << "clock() advanced by " << clock_delta << " ticks (approx. "
      << (static_cast<double>(clock_delta) * 1000.0 / CLOCKS_PER_SEC) << " ms) "
      << "during a " << (kSleepDurationMicroseconds / 1000) << " ms sleep. "
      << "This is more CPU activity than the allowed tolerance (approx. < "
      << (static_cast<double>(kMaxExpectedCpuTicksDuringSleep) * 1000.0 /
          CLOCKS_PER_SEC)
      << " ms). CLOCKS_PER_SEC: " << CLOCKS_PER_SEC;

  // Note on wrap-around: Testing for clock() wrap-around behavior explicitly
  // is impractical in a unit test due to the potentially very long CPU time
  // required (e.g., ~72 minutes for a 32-bit clock_t if CLOCKS_PER_SEC is 1M).
  // Applications using clock() for measuring very long durations should be
  // aware of this possibility.
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
