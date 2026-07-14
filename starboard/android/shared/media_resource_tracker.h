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

#include "base/memory/singleton.h"

namespace starboard {

class MediaResourceTracker {
 public:
  static MediaResourceTracker* GetInstance() {
    return base::Singleton<MediaResourceTracker>::get();
  }

  void Increment() {
    active_media_resource_count_.fetch_add(1, std::memory_order_relaxed);
  }

  void Decrement() {
    active_media_resource_count_.fetch_sub(1, std::memory_order_release);
  }

  int GetCount() const {
    return active_media_resource_count_.load(std::memory_order_acquire);
  }

 private:
  friend struct base::DefaultSingletonTraits<MediaResourceTracker>;

  MediaResourceTracker() = default;
  ~MediaResourceTracker() = default;

  std::atomic<int> active_media_resource_count_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_RESOURCE_TRACKER_H_
