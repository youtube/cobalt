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

#include <time.h>
#include <unistd.h>

#include <atomic>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const int64_t kMicrosecondsPerSecond = 1'000'000LL;

TEST(PosixTimeClockTests, ClockReturnsNonNegativeOrError) {
  clock_t start_clock = clock();
  if (start_clock == static_cast<clock_t>(-1)) {
    SUCCEED() << "clock() returned (clock_t)-1, indicating processor time "
                 "might be unavailable or an error occurred.";
  } else {
    EXPECT_GE(start_clock, 0) << "clock() returned a negative value other than "
                                 "-1, which is unexpected.";
  }
}
TEST(PosixTimeClockTests, ClockIncreasesOverTime) {
  clock_t clock_val1 = clock();
  if (clock_val1 == static_cast<clock_t>(-1)) {
    GTEST_SKIP() << "Initial call to clock() returned error; "
                 << "cannot perform time increase test.";
  }

  // Consume some CPU time.
  const int kCpuWorkIterations = 2000000;
  std::atomic<int> counter(0);
  for (int i = 0; i < kCpuWorkIterations; ++i) {
    counter++;
  }

  clock_t clock_val2 = clock();
  if (clock_val2 == static_cast<clock_t>(-1)) {
    GTEST_SKIP() << "Second call to clock() returned error.";
  }

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

TEST(PosixTimeClockTests, ClocksPerSecIsPositive) {
  EXPECT_GT(CLOCKS_PER_SEC, 0L)
      << "CLOCKS_PER_SEC should be a positive value. Actual: "
      << CLOCKS_PER_SEC;
}

TEST(PosixTimeClockTests, ClocksPerSecIsPosixStandardValue) {
  const long kPosixClocksPerSec = 1'000'000L;
  EXPECT_EQ(CLOCKS_PER_SEC, kPosixClocksPerSec)
      << "POSIX.1-2001 requires CLOCKS_PER_SEC to be " << kPosixClocksPerSec
      << ". Actual value: " << CLOCKS_PER_SEC;
}

TEST(PosixTimeClockTests, ClockMeasuresCpuTimeNotWallTimeDuringSleep) {
  clock_t start_clock = clock();
  if (start_clock == static_cast<clock_t>(-1)) {
    GTEST_SKIP() << "Initial call to clock() failed; cannot perform test.";
  }

  // Sleep for a noticeable duration. During sleep, the process should ideally
  // not consume significant CPU time.
  const unsigned int kSleepDurationMicroseconds = 100 * 1'000;
  usleep(kSleepDurationMicroseconds);

  clock_t end_clock = clock();
  if (end_clock == static_cast<clock_t>(-1)) {
    GTEST_SKIP() << "Subsequent call to clock() failed; cannot perform test.";
  }

  EXPECT_GE(end_clock, start_clock) << "Clock value decreased during a short "
                                       "sleep, which is unexpected. Start: "
                                    << start_clock << ", End: " << end_clock;

  clock_t clock_delta = end_clock - start_clock;

  // Allow for up to 10% of the wall clock sleep time to be registered as CPU
  // time, accounting for scheduler overhead, timer interrupts, etc. For 100ms
  // sleep, this is 10ms of CPU time. (0.1 seconds (sleep) * 0.1 (10% tolerance)
  // * CLOCKS_PER_SEC)
  const clock_t kMaxExpectedCpuTicksDuringSleep =
      static_cast<clock_t>(kSleepDurationMicroseconds * 0.1 * CLOCKS_PER_SEC /
                           kMicrosecondsPerSecond);

  EXPECT_LT(clock_delta, kMaxExpectedCpuTicksDuringSleep)
      << "clock() advanced by " << clock_delta << " ticks (approx. "
      << (static_cast<double>(clock_delta) * 1000.0 / CLOCKS_PER_SEC) << " ms) "
      << "during a " << (kSleepDurationMicroseconds / 1000) << " ms sleep. "
      << "This is more CPU activity than the allowed tolerance (approx. < "
      << (static_cast<double>(kMaxExpectedCpuTicksDuringSleep) * 1000.0 /
          CLOCKS_PER_SEC)
      << " ms). CLOCKS_PER_SEC: " << CLOCKS_PER_SEC;
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
