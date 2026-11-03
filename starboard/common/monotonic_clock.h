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

#ifndef STARBOARD_COMMON_MONOTONIC_CLOCK_H_
#define STARBOARD_COMMON_MONOTONIC_CLOCK_H_

#include <chrono>
#include <cstdint>

namespace starboard {

// Provides strongly-typed OS monotonic timestamps (CLOCK_MONOTONIC) with
// microsecond resolution.
//
// Satisfies C++17 TrivialClock requirements (so
// std::chrono::time_point<MonotonicClock> and
// std::condition_variable::wait_until work out of the box) and serves as an
// injectable interface for deterministic unit testing via FakeMonotonicClock.
class MonotonicClock {
 public:
  using rep = int64_t;
  using period = std::micro;
  using duration = std::chrono::microseconds;
  using time_point = std::chrono::time_point<MonotonicClock>;
  static constexpr bool is_steady = true;

  // C++17 TrivialClock entry point and concise helper for callers that do not
  // need dependency injection.
  static time_point now() noexcept;

  virtual ~MonotonicClock() = default;
  virtual time_point Now() const = 0;
};

using MonotonicTimestamp = MonotonicClock::time_point;

// Returns the process-wide, thread-safe, trivially-destructible system
// MonotonicClock singleton.
const MonotonicClock* GetSystemMonotonicClock() noexcept;

}  // namespace starboard

#endif  // STARBOARD_COMMON_MONOTONIC_CLOCK_H_
