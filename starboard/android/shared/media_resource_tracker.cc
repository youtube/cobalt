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

#include "starboard/android/shared/media_resource_tracker.h"

#include <chrono>
#include <cstdlib>
#include <mutex>

#include "starboard/common/log.h"

namespace starboard {

// static
MediaResourceTracker* MediaResourceTracker::GetInstance() {
  static starboard::NoDestructor<MediaResourceTracker> instance;
  return instance.get();
}

void MediaResourceTracker::Increment() {
  active_media_resource_count_.fetch_add(1, std::memory_order_relaxed);
}

void MediaResourceTracker::Decrement() {
  if (active_media_resource_count_.fetch_sub(1, std::memory_order_release) ==
      1) {
    ExitFunction exit_func = nullptr;
    {
      std::lock_guard lock(mutex_);
      cv_.notify_all();
      if (shutdown_requested_.load(std::memory_order_acquire)) {
        exit_func = exit_func_;
      }
    }
    if (exit_func) {
      SB_LOG(INFO)
          << "All media resources destroyed on shutdown; exiting process.";
      exit_func(0);
    }
  }
}

int MediaResourceTracker::WaitUntilZero(int timeout_ms) {
  if (active_media_resource_count_.load(std::memory_order_acquire) <= 0) {
    return 0;
  }
  std::unique_lock lock(mutex_);
  cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
    return active_media_resource_count_.load(std::memory_order_acquire) <= 0;
  });
  return active_media_resource_count_.load(std::memory_order_acquire);
}

bool MediaResourceTracker::RequestShutdown() {
  std::lock_guard lock(mutex_);
  shutdown_requested_.store(true, std::memory_order_release);
  return active_media_resource_count_.load(std::memory_order_acquire) > 0;
}

int MediaResourceTracker::GetActiveCount() const {
  return active_media_resource_count_.load(std::memory_order_acquire);
}

void MediaResourceTracker::SetExitFunctionForTesting(ExitFunction exit_func) {
  std::lock_guard lock(mutex_);
  exit_func_ = exit_func ? exit_func : std::exit;
}

void MediaResourceTracker::ResetForTesting() {
  std::lock_guard lock(mutex_);
  active_media_resource_count_.store(0, std::memory_order_release);
  shutdown_requested_.store(false, std::memory_order_release);
  exit_func_ = std::exit;
}

}  // namespace starboard
