// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/semaphore.h"

#include <chrono>
#include <limits>

#include "starboard/common/check_op.h"

namespace starboard {
namespace {
const int64_t kOneYearUs = 365LL * 24 * 60 * 60 * 1000 * 1000;
}

Semaphore::Semaphore() : permits_(0) {}

Semaphore::Semaphore(int initial_thread_permits)
    : permits_(initial_thread_permits) {}

Semaphore::~Semaphore() {}

void Semaphore::Put() {
  {
    std::lock_guard lock(mutex_);
    SB_CHECK_LT(permits_, std::numeric_limits<int>::max());
    ++permits_;
  }
  permits_cv_.notify_one();
}

void Semaphore::Take() {
  std::unique_lock lock(mutex_);
  permits_cv_.wait(lock, [this] { return permits_ > 0; });
  --permits_;
}

bool Semaphore::TakeTry() {
  std::lock_guard lock(mutex_);
  if (permits_ <= 0) {
    return false;
  }
  --permits_;
  return true;
}

bool Semaphore::TakeWait(int64_t wait_us) {
  if (wait_us <= 0) {
    return TakeTry();
  }
  std::unique_lock<std::mutex> lock(mutex_);
  // To avoid wait time overflow, we limit wait time to one year.
  const auto wait_duration =
      std::chrono::microseconds(std::min(wait_us, kOneYearUs));
  if (!permits_cv_.wait_for(lock, wait_duration,
                            [this] { return permits_ > 0; })) {
    return false;
  }
  --permits_;
  return true;
}

}  // namespace starboard
