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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_RESOURCE_TRACKER_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_RESOURCE_TRACKER_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace starboard {

class MediaResourceTracker {
 public:
  // Returns the process-wide singleton instance. Implemented using a C++11
  // thread-safe static local variable to remain decoupled from base::Singleton.
  static MediaResourceTracker* GetInstance() {
    static MediaResourceTracker instance;
    return &instance;
  }

  // Increments the count of active media resources. Called in the constructors
  // of SbPlayer, AudioTrackBridge, and MediaDrmBridge.
  void Increment() {
    active_media_resource_count_.fetch_add(1, std::memory_order_relaxed);
  }

  // Decrements the count of active media resources. Called in the destructors
  // of SbPlayer, AudioTrackBridge, and MediaDrmBridge. Uses release memory
  // ordering to guarantee all destructor writes are committed before
  // decrements.
  void Decrement() {
    if (active_media_resource_count_.fetch_sub(1, std::memory_order_release) ==
        1) {
      // Last active media resource destroyed: notify the waiting UI thread
      // immediately without polling latency.
      std::lock_guard<std::mutex> lock(mutex_);
      cv_.notify_all();
    }
  }

  // Blocks the calling thread (typically the Android UI thread in
  // CloseNativeStarboard) until all active media resources hit 0, or until
  // |timeout_ms| elapses. Returns the remaining active resource count (0 on
  // success, >0 on timeout).
  int WaitUntilZero(int timeout_ms) {
    // Fast path: if no resources are active, return immediately without
    // locking.
    if (active_media_resource_count_.load(std::memory_order_acquire) <= 0) {
      return 0;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
      return active_media_resource_count_.load(std::memory_order_acquire) <= 0;
    });
    return active_media_resource_count_.load(std::memory_order_acquire);
  }

 private:
  MediaResourceTracker() = default;
  ~MediaResourceTracker() = default;

  MediaResourceTracker(const MediaResourceTracker&) = delete;
  MediaResourceTracker& operator=(const MediaResourceTracker&) = delete;

  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<int> active_media_resource_count_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_RESOURCE_TRACKER_H_
