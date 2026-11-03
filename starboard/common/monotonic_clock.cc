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

#include <time.h>

#include "starboard/common/log.h"
#include "starboard/common/no_destructor.h"

namespace starboard {
namespace {

constexpr int64_t kMicrosecondsPerSecond = 1'000'000;

class SystemMonotonicClock final : public MonotonicClock {
 public:
  SystemMonotonicClock() = default;

  MonotonicTimestamp Now() const override {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
      SB_NOTREACHED() << "clock_gettime(CLOCK_MONOTONIC) failed.";
      return MonotonicTimestamp{};
    }
    return MonotonicTimestamp(
        duration(ts.tv_sec * kMicrosecondsPerSecond + ts.tv_nsec / 1000));
  }
};

}  // namespace

// static
MonotonicTimestamp MonotonicClock::now() noexcept {
  return GetSystemMonotonicClock()->Now();
}

const MonotonicClock* GetSystemMonotonicClock() noexcept {
  static const NoDestructor<SystemMonotonicClock> kInstance;
  return kInstance.get();
}

}  // namespace starboard
