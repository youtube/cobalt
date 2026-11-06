// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/monotonic_clock.h"

#include <chrono>
#include <type_traits>

#include "starboard/testing/fake_monotonic_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(MonotonicClockTest, DifferenceYieldsChronoMicroseconds) {
  testing::FakeMonotonicClock clock(
      MonotonicTimestamp(std::chrono::seconds(10)));
  const MonotonicTimestamp start = clock.Now();

  clock.Advance(std::chrono::milliseconds(250));
  const auto elapsed = clock.Now() - start;

  static_assert(
      std::is_same_v<decltype(elapsed), const std::chrono::microseconds>);
  EXPECT_EQ(elapsed, std::chrono::milliseconds(250));
  EXPECT_EQ(elapsed, std::chrono::microseconds(250'000));
  EXPECT_EQ(start + std::chrono::milliseconds(250), clock.Now());
}

TEST(MonotonicClockTest, SystemClockIsNonDecreasing) {
  const MonotonicClock* clock = GetSystemMonotonicClock();
  ASSERT_NE(clock, nullptr);
  EXPECT_EQ(clock, GetSystemMonotonicClock());

  const MonotonicTimestamp t1 = clock->Now();
  const MonotonicTimestamp t2 = MonotonicClock::now();
  EXPECT_GT(t1.time_since_epoch(), std::chrono::microseconds::zero());
  EXPECT_GE(t2, t1);
}

}  // namespace
}  // namespace starboard
