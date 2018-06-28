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

#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Allow millisecond-level precision.
const SbTime kPrecision = kSbTimeMillisecond;

TEST(SbThreadSleepTest, SunnyDay) {
  SbThreadSleep(0);
  // Well, my work here is done.
}

// My strategy here is to sleep for an amount of time, and ensure that AT LEAST
// that amount of time has passed. There's really no upper bound for how late
// the thread can wake up, something I completely sympathize with.
TEST(SbThreadSleepTest, SunnyDayAtLeastDelay) {
  const int kTrials = 12;
  const int64_t one = 1;
  for (int trial = 0; trial < kTrials; ++trial) {
    // This tests several delays, between about 15 to about 4 milliseconds.
    const SbTime kDelay = kSbTimeSecond / (one << ((trial % 3) + 6));
    SbTimeMonotonic start = SbTimeGetMonotonicNow();
    SbThreadSleep(kDelay);
    SbTimeMonotonic end = SbTimeGetMonotonicNow();
    EXPECT_LE(start + kDelay, end + kPrecision) << "Trial " << trial
                                                << ", kDelay=" << kDelay;
  }
}

TEST(SbThreadSleepTest, RainyDayNegativeDuration) {
  const int kTrials = 128;
  for (int trial = 0; trial < kTrials; ++trial) {
    SbTimeMonotonic start = SbTimeGetMonotonicNow();
    SbThreadSleep(-kSbTimeSecond);
    EXPECT_GT(kSbTimeSecond / 5, SbTimeGetMonotonicNow() - start) << "Trial "
                                                                  << trial;
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
