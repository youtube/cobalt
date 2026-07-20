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
#include <cstdlib>
#include <mutex>

#include "starboard/common/no_destructor.h"

namespace starboard {

// MediaResourceTracker provides thread-safe accounting of active native media
// components (SbPlayer decoders, AudioTrack audio sinks, and MediaDrm crypto
// sessions) on Android.
//
// During application teardown (e.g. CobaltActivity.onDestroy), the Java UI
// thread calls JNI_BaseStarboardBridge_CloseNativeStarboard and triggers
// RequestShutdown(). If media resources are still active on background threads,
// RequestShutdown() returns true so Java can arm a fallback watchdog timer.
// When background threads finish destroying the remaining media resources,
// Decrement() directly invokes exit(0) from the background thread.
class MediaResourceTracker {
 public:
  using ExitFunction = void (*)(int);

  static MediaResourceTracker* GetInstance();

  MediaResourceTracker(const MediaResourceTracker&) = delete;
  MediaResourceTracker& operator=(const MediaResourceTracker&) = delete;

  void Increment();
  void Decrement();
  int WaitUntilZero(int timeout_ms);

  // Marks that application shutdown has been requested.
  // Returns true if there are active media resources still being destroyed.
  // Returns false if there are no active media resources (count <= 0).
  bool RequestShutdown();

  int GetActiveCount() const;

  void SetExitFunctionForTesting(ExitFunction exit_func);
  void ResetForTesting();

 private:
  friend class starboard::NoDestructor<MediaResourceTracker>;

  MediaResourceTracker() = default;
  ~MediaResourceTracker() = default;

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<int> active_media_resource_count_ = 0;
  std::atomic<bool> shutdown_requested_ = false;
  ExitFunction exit_func_ = std::exit;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_RESOURCE_TRACKER_H_
