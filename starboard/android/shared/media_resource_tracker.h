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
#include <condition_variable>
#include <mutex>

#include "starboard/common/no_destructor.h"

namespace starboard {

// MediaResourceTracker provides thread-safe accounting of active native media
// components (SbPlayer decoders, AudioTrack audio sinks, and MediaDrm crypto
// sessions) on Android.
//
// During application teardown (e.g. CobaltActivity.onDestroy), the Java UI
// thread calls JNI_BaseStarboardBridge_CloseNativeStarboard and blocks until
// all background worker threads finish destroying media resources. This
// prevents JNI race conditions and crashes caused by calling System.exit(0)
// while media teardown tasks are still executing.
class MediaResourceTracker {
 public:
  static MediaResourceTracker* GetInstance();

  MediaResourceTracker(const MediaResourceTracker&) = delete;
  MediaResourceTracker& operator=(const MediaResourceTracker&) = delete;

  void Increment();
  void Decrement();
  int WaitUntilZero(int timeout_ms);

 private:
  friend class starboard::NoDestructor<MediaResourceTracker>;

  MediaResourceTracker() = default;
  ~MediaResourceTracker() = default;

  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<int> active_media_resource_count_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_RESOURCE_TRACKER_H_
