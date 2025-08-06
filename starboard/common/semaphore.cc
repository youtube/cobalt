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

#include "starboard/common/time.h"

namespace starboard {

Semaphore::Semaphore() : permits_(0) {}

Semaphore::Semaphore(int initial_thread_permits)
    : permits_(initial_thread_permits) {}

Semaphore::~Semaphore() {}

void Semaphore::Put() {
  std::lock_guard lock(mutex_);
  ++permits_;
  condition_.notify_one();
}

void Semaphore::Take() {
  std::unique_lock lock(mutex_);
  condition_.wait(lock, [this]() { return this->permits_ > 0; });
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
  std::unique_lock lock(mutex_);
  if (!condition_.wait_for(lock, std::chrono::microseconds(wait_us),
                           [this]() { return this->permits_ > 0; })) {
    return false;  // Timed out.
  }
  --permits_;
  return true;
}

}  // namespace starboard
