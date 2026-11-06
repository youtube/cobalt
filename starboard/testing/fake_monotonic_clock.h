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

#ifndef STARBOARD_TESTING_FAKE_MONOTONIC_CLOCK_H_
#define STARBOARD_TESTING_FAKE_MONOTONIC_CLOCK_H_

#include <atomic>
#include <chrono>
#include <cstdint>

#include "starboard/common/monotonic_clock.h"

namespace starboard::testing {

// A thread-safe controllable MonotonicClock for deterministic unit tests.
class FakeMonotonicClock final : public MonotonicClock {
 public:
  explicit FakeMonotonicClock(
      MonotonicTimestamp initial = MonotonicTimestamp(std::chrono::seconds(1)))
      : now_us_(initial.time_since_epoch().count()) {}

  FakeMonotonicClock(const FakeMonotonicClock&) = delete;
  FakeMonotonicClock& operator=(const FakeMonotonicClock&) = delete;

  MonotonicTimestamp Now() const override {
    return MonotonicTimestamp(
        duration(now_us_.load(std::memory_order_relaxed)));
  }

  void SetNow(MonotonicTimestamp time) {
    now_us_.store(time.time_since_epoch().count(), std::memory_order_relaxed);
  }

  template <typename Rep, typename Period>
  void Advance(std::chrono::duration<Rep, Period> delta) {
    now_us_.fetch_add(std::chrono::duration_cast<duration>(delta).count(),
                      std::memory_order_relaxed);
  }

 private:
  std::atomic<int64_t> now_us_;
};

}  // namespace starboard::testing

#endif  // STARBOARD_TESTING_FAKE_MONOTONIC_CLOCK_H_
