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
  int prev_count =
      active_media_resource_count_.fetch_sub(1, std::memory_order_release);
  if (prev_count <= 0) {
    SB_LOG(WARNING) << "MediaResourceTracker::Decrement called when active "
                       "resource count was "
                    << prev_count << ".";
    return;
  }
  if (prev_count == 1) {
    SB_LOG(INFO) << "All media resources destroyed.";
    std::lock_guard lock(mutex_);
    cv_.notify_all();
  }
}

int MediaResourceTracker::WaitUntilZero(int timeout_ms) {
  if (active_media_resource_count_.load(std::memory_order_acquire) <= 0) {
    SB_LOG(INFO) << "Active media resource count is already 0.";
    return 0;
  }
  std::unique_lock lock(mutex_);
  cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
    return active_media_resource_count_.load(std::memory_order_acquire) <= 0;
  });
  int remaining = active_media_resource_count_.load(std::memory_order_acquire);
  if (remaining <= 0) {
    SB_LOG(INFO) << "Finished waiting for all media resources to be destroyed.";
  }
  return remaining;
}

}  // namespace starboard
