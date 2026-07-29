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

#include <time.h>
#include <unistd.h>

#include <atomic>
#include <type_traits>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

const int64_t kMicrosecondsPerSecond = 1'000'000LL;

// Work time long enough to ensure measurable CPU time usage.
constexpr long kMinimumWorkTimeMicroseconds = 500'000;  // 500ms

static int64_t TimespecToMicroseconds(const struct timespec& ts) {
  return (static_cast<int64_t>(ts.tv_sec) * kMicrosecondsPerSecond) +
         (static_cast<int64_t>(ts.tv_nsec) / 1'000LL);
}

// Helper function to do CPU work for until at least work_time_microseconds
// elapsed on the monotonic wall clock. Return the elapsed time in microseconds.
// Note: This function uses CLOCK_MONOTONIC to measure the elapsed
// time, not the CPU time clock. This is intentional, as the goal is to
// consume CPU time for a specific duration as measured by wall clock time.
// The CPU time clock is then checked to see how much CPU time was actually
// consumed.
long ConsumeCpuForDuration(long work_time_microseconds) {
  struct timespec ts_before{};
  int ret_before = clock_gettime(CLOCK_MONOTONIC, &ts_before);
  // Note: Testing using EXPECT_EQ because ASSERT_EQ can not return with a
  // value.
  EXPECT_EQ(0, ret_before);
  if (ret_before) {
    return 0;
  }
  int64_t time_before_us = TimespecToMicroseconds(ts_before);
  int64_t time_after_us = 0;
  // Loop for at least 100ms to ensure some measurable CPU time is consumed.
  // Using volatile to prevent compiler from optimizing away the loop.
  volatile int_fast32_t counter_sum = 0;
  do {
    const int kCpuWorkIterations = 2'000'000;
    for (int i = 0; i < kCpuWorkIterations; ++i) {
      counter_sum += i;  // Simple work
    }
    struct timespec ts_after{};
    int ret_current = clock_gettime(CLOCK_MONOTONIC, &ts_after);
    // Note: Testing using EXPECT_EQ because ASSERT_EQ can not return with a
    // value.
    EXPECT_EQ(0, ret_current);
    if (ret_current) {
      return 0;
    }
    time_after_us = TimespecToMicroseconds(ts_after);
  } while ((time_after_us - time_before_us) < work_time_microseconds);
  return time_after_us - time_before_us;
}

// TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
// On non-hermetic builds, clock_gettime() is declared "noexcept".
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
// Assert that clock has the signature:
// clock_t clock(void)
static_assert(std::is_same_v<decltype(clock), clock_t(void)>,
              "'clock' is not declared or does not have the signature "
              "'clock_t (void)'");
#endif

TEST(PosixTimeClockTests, ClockReturnsNonNegative) {
  clock_t start_clock = clock();
  ASSERT_NE(start_clock, static_cast<clock_t>(-1))
      << "clock() unexpectedly returned (clock_t)-1, indicating processor time "
         "might be unavailable or an error occurred.";
  // Separately check for other negative values to log a more helpful message
  // when the test fails.
  EXPECT_GE(start_clock, 0)
      << "clock() unexpectedly returned a negative value.";
}

TEST(PosixTimeClockTests, ClockIncreasesOverTime) {
  clock_t clock_val1 = clock();
  ASSERT_NE(clock_val1, static_cast<clock_t>(-1))
      << "clock() unexpectedly returned (clock_t)-1, indicating processor time "
         "might be unavailable or an error occurred.";

  long elapsed_time_us = ConsumeCpuForDuration(kMinimumWorkTimeMicroseconds);
  EXPECT_GE(elapsed_time_us, kMinimumWorkTimeMicroseconds)
      << "Elapsed time shorter than the minimum "
      << kMinimumWorkTimeMicroseconds << " us";

  clock_t clock_val2 = clock();
  ASSERT_NE(clock_val2, static_cast<clock_t>(-1))
      << "clock() unexpectedly returned (clock_t)-1, indicating processor time "
         "might be unavailable or an error occurred.";

  EXPECT_GT(clock_val2, clock_val1)
      << "Clock value did not increase, which is unexpected. val1="
      << clock_val1 << ", val2=" << clock_val2;

  // Expect to have measured at least 60% of the work time as CPU time.
  long measured_work_time =
      kMicrosecondsPerSecond * (clock_val2 - clock_val1) / CLOCKS_PER_SEC;
  long minimum_work_time = 0.60 * elapsed_time_us;
  EXPECT_GE(measured_work_time, minimum_work_time)
      << "Clock value should measure at least " << minimum_work_time
      << " us, of " << elapsed_time_us << " us spent doing CPU work.";
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
  ASSERT_NE(start_clock, static_cast<clock_t>(-1))
      << "clock() unexpectedly returned (clock_t)-1, indicating processor time "
         "might be unavailable or an error occurred.";

  // Sleep for a noticeable duration. During sleep, the process should ideally
  // not consume significant CPU time.
  const unsigned int kSleepDurationMicroseconds = 100 * 1'000;
  usleep(kSleepDurationMicroseconds);

  clock_t end_clock = clock();
  ASSERT_NE(start_clock, static_cast<clock_t>(-1))
      << "clock() unexpectedly returned (clock_t)-1, indicating processor time "
         "might be unavailable or an error occurred.";

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
