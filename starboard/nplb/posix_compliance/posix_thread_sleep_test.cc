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

#include <unistd.h>

#include "starboard/common/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Allow millisecond-level precision.
const int64_t kPrecision = 1000;  // 1ms

TEST(PosixThreadSleepTest, SunnyDay) {
  usleep(0);
  // Well, my work here is done.
}

TEST(PosixThreadSleepTest, SunnyDayAtLeastDelay) {
  const int kTrials = 12;
  const int64_t one = 1;
  for (int trial = 0; trial < kTrials; ++trial) {
    // This tests several delays, between about 15 to about 4 milliseconds.
    const int64_t kDelay = 1'000'000LL / (one << ((trial % 3) + 6));
    int64_t start = CurrentMonotonicTime();
    usleep(kDelay);
    int64_t end = CurrentMonotonicTime();
    EXPECT_LE(start + kDelay, end + kPrecision)
        << "Trial " << trial << ", kDelay=" << kDelay;
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
