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

#include <pthread.h>
#include <sched.h>

#include "starboard/common/time.h"
#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Returns whether a given index is a yielder for the given trial. We alternate
// whether 0 or 1 is a Yielder to avoid the first-started advantage.
inline bool IsYielder(int trial, int index) {
  return (trial % 2 ? (index % 2 != 0) : (index % 2 == 0));
}

// The more loops, the more the yielders should fall behind.
const int kLoops = 10000;

void* YieldingEntryPoint(void* context) {
  for (int i = 0; i < kLoops; ++i) {
    sched_yield();
  }

  int64_t* end_time = static_cast<int64_t*>(context);
  *end_time = CurrentMonotonicTime();
  return NULL;
}

void* UnyieldingEntryPoint(void* context) {
  for (int i = 0; i < kLoops; ++i) {
    posix::DoNotYield();
  }

  int64_t* end_time = static_cast<int64_t*>(context);
  *end_time = CurrentMonotonicTime();
  return NULL;
}

TEST(PosixThreadYieldTest, SunnyDay) {
  sched_yield();
  // Well, my work here is done.
}

TEST(PosixThreadYieldTest, SunnyDayRace) {
  const int kTrials = 20;
  int passes = 0;
  for (int trial = 0; trial < kTrials; ++trial) {
    // We want enough racers such that the threads must contend for cpu time,
    // and enough data for the averages to be consistently divergent.
    const int64_t kRacers = 32;
    pthread_t threads[kRacers];
    int64_t end_times[kRacers] = {0};
    for (int i = 0; i < kRacers; ++i) {
      pthread_t thread;
      EXPECT_EQ(pthread_create(&threads[i], NULL,
                               (IsYielder(trial, i) ? YieldingEntryPoint
                                                    : UnyieldingEntryPoint),
                               &(end_times[i])),
                0);
    }

    for (int i = 0; i < kRacers; ++i) {
      EXPECT_TRUE(threads[i] != 0) << "thread = " << threads[i];
    }

    for (int i = 0; i < kRacers; ++i) {
      EXPECT_EQ(pthread_join(threads[i], NULL), 0);
    }

    // On average, Unyielders should finish sooner than Yielders.
    int64_t average_yielder = 0;
    int64_t average_unyielder = 0;
    const int64_t kRacersPerGroup = kRacers / 2;
    for (int i = 0; i < kRacers; ++i) {
      if (IsYielder(trial, i)) {
        average_yielder += end_times[i] / kRacersPerGroup;
      } else {
        average_unyielder += end_times[i] / kRacersPerGroup;
      }
    }

    // If unyielders took less time then yielders, on average, then we consider
    // the trial a pass.
    if (average_unyielder < average_yielder) {
      ++passes;
    }
  }

  // We expect at least 2/3 of the trials to pass.
  EXPECT_LT(kTrials * 2 / 3, passes);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
