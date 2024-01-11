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

#if SB_API_VERSION >= 16

#include "starboard/common/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

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

TEST(PosixTimeTest, MonotonickIsMonotonic) {
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

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION >= 16
