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
#include "starboard/shared/starboard/player/job_queue.h"

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

  // acquired before last holder release the surface.
  scoped_refptr<SurfaceDestroyNotifier> AcquireVideoSurface(
      JobQueue* job_queue,
      jobject* out_surface);

  // Release the surface to make the surface available for other holder.
  void ReleaseVideoSurface();

  // Get the native window size. Return false if don't have available native
  // window.
  bool GetVideoWindowSize(int* width, int* height);

  // Clear the video window by painting it Black.
  void ClearVideoWindow(bool force_reset_surface);
};

class SurfaceDestroyNotifier
    : public RefCountedThreadSafe<SurfaceDestroyNotifier> {
 public:
  SurfaceDestroyNotifier(VideoSurfaceHolder* holder, JobQueue* job_queue)
      : holder_(holder), job_queue_(job_queue) {}

  void Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    disconnected_ = true;
    job_queue_ = nullptr;
  }

  void Notify() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (disconnected_ || !holder_ || !job_queue_) {
      return;
    }

    done_ = false;
    job_queue_->Schedule([this]() { RunTask(); });

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
