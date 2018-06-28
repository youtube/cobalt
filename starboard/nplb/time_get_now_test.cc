// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/nplb/time_constants.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbTimeGetNowTest, IsKindOfSane) {
  SbTime time = SbTimeGetNow();

  // Now should be after the time I wrote this test.
  EXPECT_GT(time, kTestTimeWritten);

  // And it should be before 5 years after I wrote this test, at least for a
  // while.
  EXPECT_LT(time, kTestTimePastWritten);
}

TEST(SbTimeGetNowTest, HasDecentResolution) {
  const int kNumIterations = 100;
  for (int i = 0; i < kNumIterations; ++i) {
    SbTime time1 = SbTimeGetNow();
    SbTimeMonotonic timerStart = SbTimeGetMonotonicNow();

    // Spin tightly until time increments.
    SbTime time2 = 0;
    while (true) {
      time2 = SbTimeGetNow();
      if (time2 != time1) {
        EXPECT_NE(time2, time1);
        break;
      }

      // If time hasn't changed within a second, that's beyond low resolution.
      if ((SbTimeGetMonotonicNow() - timerStart) >= kSbTimeSecond) {
        GTEST_FAIL() << "SbTimeGetNow() hasn't changed within a second.";
        break;
      }
    }
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
