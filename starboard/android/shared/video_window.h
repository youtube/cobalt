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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_WINDOW_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_WINDOW_H_

#include <android/native_window.h>
#include <jni.h>

#include <condition_variable>
#include <mutex>

#include "starboard/common/ref_counted.h"
#include "starboard/decode_target.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

class SurfaceDestroyNotifier;

class VideoSurfaceHolder {
 public:
  // Return true only if the video surface is available.
  static bool IsVideoSurfaceAvailable();

  // OnSurfaceDestroyed() will be invoked when surface is destroyed. When this
  // function is called, the decoder no longer owns the surface. Calling
  // AcquireVideoSurface(), ReleaseVideoSurface(), GetVideoWindowSize() or
  // ClearVideoWindow() in this function may cause dead lock.
  virtual void OnSurfaceDestroyed() = 0;

 protected:
  ~VideoSurfaceHolder() {}

  // Returns the surface to which video should be rendered. The surface
  // cannot be acquired before last holder release the surface.
  scoped_refptr<SurfaceDestroyNotifier> AcquireVideoSurface(
      JobQueue* job_queue,
      jni_zero::ScopedJavaLocalRef<jobject>* out_surface);

  // Release the surface to make the surface available for other holder.
  void ReleaseVideoSurface();

  // Get the native window size. Return false if don't have available native
  // window.
  bool GetVideoWindowSize(int* width, int* height);

  // Cleans up the video surface. If |force_clear| is enabled, we will only
  // clear the video window, and post the clearing task to |gpu_provider|.
  // If |force_clear| is false, we will forcefully destroy the surface view,
  // which will then be recreated.
  void CleanUpVideoWindow(
      bool force_clear,
      SbDecodeTargetGraphicsContextProvider* gpu_provider = nullptr);
};

// SurfaceDestroyNotifier is used to safely handle the destruction of the video
// surface from JNI without causing deadlocks.
//
// Purpose: Decouples the JNI thread (which notifies about surface destruction)
// from the player worker thread (which performs the teardown). It schedules
// the teardown task on the player worker thread and waits for it with a timeout
// to avoid hanging the JNI thread.
//
// Lifetime/Ownership: This class is RefCountedThreadSafe. It is created by
// VideoSurfaceHolder when acquiring the surface, and references are held by
// the global `g_surface_destroy_notifier` and the `MediaCodecVideoDecoder`.
// The task scheduled on the JobQueue also retains a reference to ensure
// the object stays alive until execution completes.
//
// Threading Model: `Notify()` is called from the JNI thread. `RunTask()`
// executes on the player worker thread. Internal state is protected by its
// own mutex.
class SurfaceDestroyNotifier
    : public RefCountedThreadSafe<SurfaceDestroyNotifier> {
 public:
  SurfaceDestroyNotifier(VideoSurfaceHolder* holder, JobQueue* job_queue)
      : holder_(holder), job_queue_(job_queue) {}

  void Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    disconnected_ = true;
    job_queue_ = nullptr;
    holder_ = nullptr;
    done_ = true;  // Mark as done_ so Notify() can exit immediately
    cv_.notify_one();
  }

  void Notify() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (disconnected_ || !holder_ || !job_queue_) {
      return;
    }

    done_ = false;
    scoped_refptr<SurfaceDestroyNotifier> self(this);
    job_queue_->Schedule([self]() { self->RunTask(); });

    // Wait for the task to complete with a 1-second timeout.
    cv_.wait_for(lock, std::chrono::seconds(1), [this] { return done_; });
  }

  bool Holds(VideoSurfaceHolder* holder) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return holder_ == holder;
  }

 protected:
  ~SurfaceDestroyNotifier() = default;
  friend class RefCountedThreadSafe<SurfaceDestroyNotifier>;

 private:
  void RunTask();

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  bool done_ = false;
  bool disconnected_ = false;
  VideoSurfaceHolder* holder_;
  JobQueue* job_queue_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_WINDOW_H_
