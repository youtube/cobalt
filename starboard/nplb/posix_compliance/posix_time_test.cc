// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <sys/time.h>
#include <time.h>
#include "starboard/common/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

<<<<<<< HEAD
TEST(PosixTimeTest, TimeMatchesGettimeofday) {
  time_t other_time_s = 0;
  time_t time_s = time(&other_time_s);  // Seconds since Unix epoch.
  struct timeval tv;
  gettimeofday(&tv, NULL);  // Microseconds since Unix epoch.

  EXPECT_EQ(time_s, other_time_s);
=======
// kReasonableMinTime represents a time (2025-01-01 00:00:00 UTC) after which
// the current time is expected to fall.
const int64_t kReasonableMinTimeUsec =
    1'735'689'600'000'000LL;  // 2025-01-01 00:00:00 UTC

// kReasonableMaxTimeUsec represents a time very far in the future and before
// which the current time is expected to fall. Note that this also implicitly
// tests that the code handles timestamps past the Unix Epoch wraparound on
// 03:14:08 UTC on 19 January 2038.
const time_t kReasonableMaxTimeUsec = std::numeric_limits<time_t>::max();
>>>>>>> d3e1f4b3efb (starboard: Fix conversion warnings and unused function (#6511))

  int64_t time_us = static_cast<int64_t>(time_s) * 1'000'000;
  int64_t gettimeofday_us =
      (static_cast<int64_t>(tv.tv_sec) * 1'000'000) + tv.tv_usec;
  // Values should be within 1 second of each other.
  EXPECT_NEAR(time_us, gettimeofday_us, 1'000'000);
}

TEST(PosixTimeTest, TimeIsKindOfSane) {
  int64_t now_usec = CurrentPosixTime();

  // Now should be after 2024-01-01 UTC (the past).
  int64_t past_usec = 1704067200000000LL;
  EXPECT_GT(now_usec, past_usec);

  // Now should be before 2044-01-01 UTC (the future).
  int64_t future_usec = 2335219200000000LL;
  EXPECT_LT(now_usec, future_usec);
}

TEST(PosixTimeTest, HasDecentResolution) {
  const int kNumIterations = 100;
  for (int i = 0; i < kNumIterations; ++i) {
    int64_t timerStart = CurrentMonotonicTime();
    int64_t initialTime = CurrentPosixTime();

    // Spin tightly until time increments.
    while (CurrentPosixTime() == initialTime) {
      // If time hasn't changed within a second, that's beyond low resolution.
      if ((CurrentMonotonicTime() - timerStart) >= 1'000'000) {
        GTEST_FAIL() << "CurrentPosixTime() hasn't changed within a second.";
        break;
      }
    }
  }
}

TEST(PosixTimeTest, MonotonicIsMonotonic) {
  const int kTrials = 100;
  for (int trial = 0; trial < kTrials; ++trial) {
    int64_t timerStart = CurrentPosixTime();
    int64_t initialMonotonic = CurrentMonotonicTime();

    // Spin tightly until time changes.
    int64_t newMonotonic = 0;
    while (true) {
      newMonotonic = CurrentMonotonicTime();
      if (initialMonotonic != newMonotonic) {
        EXPECT_GT(newMonotonic, initialMonotonic);
        EXPECT_LT(newMonotonic - initialMonotonic, 1'000'000);  // Less than 1s
        return;
      }

      // If time hasn't increased within a second, our "high-resolution"
      // monotonic timer is broken.
      if (CurrentPosixTime() - timerStart >= 1'000'000) {
        GTEST_FAIL() << "CurrentMonotonicTime() hasn't changed within a "
                     << "second.";
        return;
      }
    }
  }
}

TEST(PosixTimeTest, GmtimeR) {
  time_t timer = 1722468779;  // Wed 2024-07-31 23:32:59 UTC.
  struct tm result;
  memset(&result, 0, sizeof(result));

  struct tm* retval = NULL;
  retval = gmtime_r(&timer, &result);
  EXPECT_EQ(retval, &result);
  EXPECT_EQ(result.tm_year, 2024 - 1900);  // Year since 1900.
  EXPECT_EQ(result.tm_mon, 7 - 1);         // Zero-indexed.
  EXPECT_EQ(result.tm_mday, 31);
  EXPECT_EQ(result.tm_hour, 23);
  EXPECT_EQ(result.tm_min, 32);
  EXPECT_EQ(result.tm_sec, 59);
  EXPECT_EQ(result.tm_wday, 3);    // Wednesday, 0==Sunday.
  EXPECT_EQ(result.tm_yday, 212);  // Zero-indexed; 2024 is a leap year.
  EXPECT_LE(result.tm_isdst, 0);   // <=0; GMT/UTC never has DST (even in July).
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
