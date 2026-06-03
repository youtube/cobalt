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
namespace {

// kReasonableMinTime represents a time (2025-01-01 00:00:00 UTC) after which
// the current time is expected to fall.
const int64_t kReasonableMinTimeUsec =
    1'735'689'600'000'000LL;  // 2025-01-01 00:00:00 UTC

// kReasonableMaxTimeUsec represents a time very far in the future and before
// which the current time is expected to fall. Note that this also implicitly
// tests that the code handles timestamps past the Unix Epoch wraparound on
// 03:14:08 UTC on 19 January 2038.
const time_t kReasonableMaxTimeUsec = std::numeric_limits<time_t>::max();

TEST(PosixTimeTest, CurrentPosixTimeIsKindOfSane) {
  int64_t now_usec = CurrentPosixTime();

  // Now should be after 2025-01-01 UTC (the past).
  int64_t past_usec = kReasonableMinTimeUsec;
  EXPECT_GT(now_usec, past_usec);

  // Now should be before 2044-01-01 UTC (the future).
  int64_t future_usec = kReasonableMaxTimeUsec;
  EXPECT_LT(now_usec, future_usec);
}

TEST(PosixTimeTest, CurrentPosixTimeHasDecentResolution) {
  const int kNumIterations = 100;
  for (int i = 0; i < kNumIterations; ++i) {
    int64_t timerStart = CurrentMonotonicTime();
    int64_t initialTime = CurrentPosixTime();

    // Spin tightly until time increments.
    while (CurrentPosixTime() == initialTime) {
      // If time hasn't changed within a second, that's beyond low resolution.
      if ((CurrentMonotonicTime() - timerStart) >= 1'000'000) {
        GTEST_FAIL() << "CurrentPosixTime() hasn't changed within a second.";
      }
    }
  }
}

TEST(PosixTimeTest, CurrentMonotonicTimeIsMonotonic) {
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
        break;
      }

      // If time hasn't increased within a second, our "high-resolution"
      // monotonic timer is broken.
      if (CurrentPosixTime() - timerStart >= 1'000'000) {
        GTEST_FAIL() << "CurrentMonotonicTime() hasn't changed within a "
                     << "second.";
      }
    }
  }
}

// Tests the gmtime_r() function for correct conversion of time_t to struct tm.
TEST(PosixTimeTest, GmtimeRConvertsTimeCorrectly) {
  // A fixed timestamp: Wed Jul 31 23:32:59 2024 UTC.
  // This value (1'722'468'779) can be obtained via `date -d "2024-07-31
  // 23:32:59 UTC" +%s`.
  const time_t kFixedTime = 1'722'468'779;
  struct tm result_tm{};

  struct tm* retval = gmtime_r(&kFixedTime, &result_tm);
  ASSERT_TRUE(retval != NULL)
      << "gmtime_r returned NULL. This indicates an error, possibly EOVERFLOW "
      << "if time_t is out of range for struct tm, or other system error. "
         "errno: "
      << errno;
  EXPECT_EQ(retval, &result_tm)
      << "gmtime_r should return the pointer to the provided result struct.";

  // Validate components of struct tm.
  EXPECT_EQ(result_tm.tm_year, 2024 - 1900)
      << "Year mismatch.";  // Years since 1900
  EXPECT_EQ(result_tm.tm_mon, 7 - 1)
      << "Month mismatch.";  // Month, 0-11 (July is 6)
  EXPECT_EQ(result_tm.tm_mday, 31) << "Day of month mismatch.";
  EXPECT_EQ(result_tm.tm_hour, 23) << "Hour mismatch.";
  EXPECT_EQ(result_tm.tm_min, 32) << "Minute mismatch.";
  EXPECT_EQ(result_tm.tm_sec, 59) << "Second mismatch.";
  EXPECT_EQ(result_tm.tm_wday, 3)
      << "Day of week mismatch.";  // Days since Sunday, 0-6 (Wednesday is 3)

  // Calculate yday for 2024 (leap year), July 31:
  // Jan(31) + Feb(29) + Mar(31) + Apr(30) + May(31) + Jun(30) + Jul(31) = 213
  // days. tm_yday is 0-indexed, so it should be 212.
  EXPECT_EQ(result_tm.tm_yday, 212) << "Day of year mismatch.";
  EXPECT_EQ(result_tm.tm_isdst, 0)
      << "DST flag mismatch (should be 0 for UTC/GMT).";
}

}  // namespace
}  // namespace starboard
