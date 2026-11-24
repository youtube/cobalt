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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_FRAME_RELEASE_PLATFORM_HELPER_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_FRAME_RELEASE_PLATFORM_HELPER_H_

#include <jni.h>

#include <atomic>
#include <cstdint>
#include <mutex>

#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

// Abstract platform seam for VSYNC parameters and Surface.setFrameRate votes.
class VideoFrameReleasePlatform {
 public:
  struct VsyncSnapshot {
    int64_t sampled_vsync_time_ns = 0;
    int64_t vsync_duration_ns = -1;
    int64_t vsync_offset_ns = -1;
  };

  virtual ~VideoFrameReleasePlatform() = default;

  virtual VsyncSnapshot GetVsyncSnapshot() const = 0;
  virtual void SetPlaybackActive(bool active) = 0;
  virtual void UpdateSurfaceFrameRate(const jni_zero::JavaRef<jobject>& surface,
                                      float frame_rate,
                                      bool seamless_only) = 0;
  virtual void ClearSurfaceFrameRate() = 0;
  virtual int64_t vsync_sample_count() const { return 0; }
};

// JNI-backed platform helper that stores Java DisplayListener/Choreographer
// updates in lock-free C++ atomics so Render() never crosses JNI per frame.
class VideoFrameReleasePlatformHelper final : public VideoFrameReleasePlatform {
 public:
  VideoFrameReleasePlatformHelper(bool track_display_changes,
                                  bool prefer_vsync_callback_v33,
                                  bool pause_sampler_when_inactive);
  ~VideoFrameReleasePlatformHelper() override;

  VideoFrameReleasePlatformHelper(const VideoFrameReleasePlatformHelper&) =
      delete;
  VideoFrameReleasePlatformHelper& operator=(
      const VideoFrameReleasePlatformHelper&) = delete;

  VsyncSnapshot GetVsyncSnapshot() const override;
  void SetPlaybackActive(bool active) override;
  void UpdateSurfaceFrameRate(const jni_zero::JavaRef<jobject>& surface,
                              float frame_rate,
                              bool seamless_only) override;
  void ClearSurfaceFrameRate() override;
  int64_t vsync_sample_count() const override {
    return vsync_sample_count_.load(std::memory_order_relaxed);
  }

  // Invoked by Java VideoFrameReleasePlatformHelperJni.onVsyncUpdate().
  void OnVsyncUpdate(JNIEnv* env,
                     jlong sampled_vsync_time_ns,
                     jlong vsync_duration_ns,
                     jlong vsync_offset_ns);

 private:
  const bool pause_sampler_when_inactive_;

  mutable std::mutex vsync_mutex_;
  VsyncSnapshot vsync_snapshot_;
  std::atomic<int64_t> vsync_sample_count_{0};
  std::atomic<bool> playback_active_{true};

  std::mutex surface_vote_mutex_;
  jni_zero::ScopedJavaGlobalRef<jobject> last_voted_surface_;
  float last_voted_frame_rate_ = 0.0f;
  bool last_voted_seamless_only_ = false;

  jni_zero::ScopedJavaGlobalRef<jobject> j_helper_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_FRAME_RELEASE_PLATFORM_HELPER_H_
