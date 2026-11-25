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

#include "starboard/android/shared/video_frame_release_platform_helper.h"

#include <cmath>

#include "cobalt/android/jni_headers/VideoFrameReleasePlatformHelper_jni.h"

namespace starboard {
namespace {

using jni_zero::AttachCurrentThread;

}  // namespace

VideoFrameReleasePlatformHelper::VideoFrameReleasePlatformHelper(
    bool track_display_changes,
    bool prefer_vsync_callback_v33,
    bool pause_sampler_when_inactive)
    : pause_sampler_when_inactive_(pause_sampler_when_inactive),
      playback_active_(!pause_sampler_when_inactive) {
  JNIEnv* env = AttachCurrentThread();
  j_helper_.Reset(Java_VideoFrameReleasePlatformHelper_create(
      env, reinterpret_cast<jlong>(this), track_display_changes,
      prefer_vsync_callback_v33, !pause_sampler_when_inactive));
}

VideoFrameReleasePlatformHelper::~VideoFrameReleasePlatformHelper() {
  if (j_helper_) {
    JNIEnv* env = AttachCurrentThread();
    Java_VideoFrameReleasePlatformHelper_release(env, j_helper_);
    j_helper_.Reset();
  }
  std::lock_guard lock(surface_vote_mutex_);
  last_voted_surface_.Reset();
}

VideoFrameReleasePlatform::VsyncSnapshot
VideoFrameReleasePlatformHelper::GetVsyncSnapshot() const {
  std::lock_guard lock(vsync_mutex_);
  return vsync_snapshot_;
}

void VideoFrameReleasePlatformHelper::SetPlaybackActive(bool active) {
  if (!pause_sampler_when_inactive_ || !j_helper_) {
    return;
  }
  if (playback_active_.exchange(active, std::memory_order_relaxed) == active) {
    return;
  }
  Java_VideoFrameReleasePlatformHelper_setPlaybackActive(AttachCurrentThread(),
                                                         j_helper_, active);
}

void VideoFrameReleasePlatformHelper::UpdateSurfaceFrameRate(
    const jni_zero::JavaRef<jobject>& surface,
    float frame_rate,
    bool seamless_only) {
  if (!j_helper_) {
    return;
  }
  const float sanitized_rate =
      (frame_rate > 0.0f && std::isfinite(frame_rate)) ? frame_rate : 0.0f;

  JNIEnv* env = AttachCurrentThread();
  {
    std::lock_guard lock(surface_vote_mutex_);
    const bool same_surface =
        (!surface && !last_voted_surface_) ||
        (surface && last_voted_surface_ &&
         env->IsSameObject(surface.obj(), last_voted_surface_.obj()));
    if (same_surface && last_voted_frame_rate_ == sanitized_rate &&
        last_voted_seamless_only_ == seamless_only) {
      return;
    }
    if (surface) {
      last_voted_surface_.Reset(env, surface.obj());
    } else {
      last_voted_surface_.Reset();
    }
    last_voted_frame_rate_ = sanitized_rate;
    last_voted_seamless_only_ = seamless_only;
  }
  Java_VideoFrameReleasePlatformHelper_updateSurfaceFrameRate(
      env, j_helper_, surface, sanitized_rate, seamless_only);
}

void VideoFrameReleasePlatformHelper::ClearSurfaceFrameRate() {
  if (!j_helper_) {
    return;
  }
  {
    std::lock_guard lock(surface_vote_mutex_);
    if (last_voted_frame_rate_ == 0.0f && !last_voted_surface_) {
      return;
    }
    last_voted_surface_.Reset();
    last_voted_frame_rate_ = 0.0f;
    last_voted_seamless_only_ = false;
  }
  Java_VideoFrameReleasePlatformHelper_clearSurfaceFrameRate(
      AttachCurrentThread(), j_helper_);
}

void VideoFrameReleasePlatformHelper::OnVsyncUpdate(JNIEnv* env,
                                                    jlong sampled_vsync_time_ns,
                                                    jlong vsync_duration_ns,
                                                    jlong vsync_offset_ns) {
  std::lock_guard lock(vsync_mutex_);
  if (sampled_vsync_time_ns > 0) {
    vsync_snapshot_.sampled_vsync_time_ns = sampled_vsync_time_ns;
    vsync_sample_count_.fetch_add(1, std::memory_order_relaxed);
  } else if (sampled_vsync_time_ns < 0) {
    // Clear stale pre-switch or pre-pause phase anchor until a fresh sample
    // arrives for the active display mode.
    vsync_snapshot_.sampled_vsync_time_ns = 0;
  }
  if (vsync_duration_ns > 0) {
    vsync_snapshot_.vsync_duration_ns = vsync_duration_ns;
    vsync_snapshot_.vsync_offset_ns = vsync_offset_ns;
  }
}

}  // namespace starboard
