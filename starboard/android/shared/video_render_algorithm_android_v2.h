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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_RENDER_ALGORITHM_ANDROID_V2_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_RENDER_ALGORITHM_ANDROID_V2_H_

#include <android/api-level.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <mutex>

#include "starboard/android/shared/fixed_frame_rate_estimator.h"
#include "starboard/android/shared/video_frame_release_platform_helper.h"
#include "starboard/android/shared/video_frame_tracker.h"
#include "starboard/android/shared/video_render_algorithm_host.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"

namespace starboard {

namespace video_render_algorithm_v2 {

// Only bits 0..30 are valid; bit 31 (sign bit) is never used.
inline constexpr uint32_t kEnableNewAlgorithm = 1u << 0;               // 0x1
inline constexpr uint32_t kFirstFrameImmediateAndAudioGate = 1u << 1;  // 0x2
inline constexpr uint32_t kStarvationFloor100ms = 1u << 2;             // 0x4
inline constexpr uint32_t kIncrementalFloatSpeedSmoother = 1u << 3;    // 0x8
inline constexpr uint32_t kFixedFrameRateEstimator = 1u << 4;          // 0x10
inline constexpr uint32_t kSkipSameVsyncReleaseTime = 1u << 5;         // 0x20
inline constexpr uint32_t kDynamicDisplayRefreshRate = 1u << 6;        // 0x40
inline constexpr uint32_t kSurfaceSetFrameRate = 1u << 7;              // 0x80
inline constexpr uint32_t kVsyncMidpointHysteresis = 1u << 8;          // 0x100
inline constexpr uint32_t kVsyncCallbackFrameTimeline = 1u << 9;       // 0x200
inline constexpr uint32_t kAudioEosDrawLastAndLastFrameGuard = 1u
                                                               << 10;  // 0x400
inline constexpr uint32_t kDisableTextureModeLookahead = 1u << 11;     // 0x800
inline constexpr uint32_t kDeadlineWakeupsAndSamplerHygiene = 1u
                                                              << 12;  // 0x1000
inline constexpr uint32_t kSurfaceFrameRateSeamlessOnly = 1u << 13;   // 0x2000
inline constexpr uint32_t kForceLegacyAlgorithm = 1u << 30;  // 0x40000000

}  // namespace video_render_algorithm_v2

// Resolves hard bit dependencies and strips unsupported Android SDK bits.
uint32_t NormalizeVideoRenderAlgorithmMask(
    uint32_t raw_mask,
    int api_level = android_get_device_api_level());

// Returns true when Media.VideoRenderAlgorithmMask selects V2 over legacy.
bool ShouldUseVideoRenderAlgorithmAndroidV2(
    int raw_mask,
    int api_level = android_get_device_api_level());

class VideoRenderAlgorithmAndroidV2 final : public VideoRenderAlgorithm {
 public:
  using ClockNowNsFn = std::function<int64_t()>;

  struct Stats {
    int64_t rendered_frames = 0;
    int64_t immediate_first_frames = 0;
    int64_t forced_by_starvation_floor = 0;
    int64_t dropped_late_frames = 0;
    int64_t skipped_same_vsync = 0;
    int64_t skipped_audio_eos = 0;
    int64_t smoother_resets = 0;
    float estimated_fps = FixedFrameRateEstimator::kNoFrameRate;
    float last_surface_fps_vote = 0.0f;
  };

  VideoRenderAlgorithmAndroidV2(VideoRenderAlgorithmHost* host,
                                VideoFrameTracker* frame_tracker,
                                uint32_t raw_mask);
  VideoRenderAlgorithmAndroidV2(
      VideoRenderAlgorithmHost* host,
      VideoFrameTracker* frame_tracker,
      uint32_t raw_mask,
      int api_level,
      std::unique_ptr<VideoFrameReleasePlatform> platform_for_testing,
      ClockNowNsFn clock_now_ns_for_testing = {});
  ~VideoRenderAlgorithmAndroidV2() override;

  VideoRenderAlgorithmAndroidV2(const VideoRenderAlgorithmAndroidV2&) = delete;
  VideoRenderAlgorithmAndroidV2& operator=(
      const VideoRenderAlgorithmAndroidV2&) = delete;

  void Render(MediaTimeProvider* media_time_provider,
              std::list<scoped_refptr<VideoFrame>>* frames,
              VideoRendererSink::DrawFrameCB draw_frame_cb) override;
  void Seek(int64_t seek_to_time) override;
  int GetDroppedFrames() override;

  uint32_t normalized_mask() const { return mask_; }
  Stats GetStats() const;

 private:
  static constexpr int64_t kIndexUnset = -1;

  bool HasBit(uint32_t bit) const { return (mask_ & bit) != 0; }

  int64_t AdjustReleaseTimeLocked(
      int64_t presentation_time_us,
      int64_t unadjusted_release_time_ns,
      double playback_rate,
      const VideoFrameReleasePlatform::VsyncSnapshot& vsync,
      bool suppress_vsync_lead_offset);
  int64_t AdjustReleaseTimeIncrementalLocked(int64_t presentation_time_us,
                                             int64_t unadjusted_release_time_ns,
                                             double playback_rate);
  int64_t AdjustReleaseTimeLegacyParityLocked(
      int64_t presentation_time_us,
      int64_t unadjusted_release_time_ns,
      double playback_rate);
  int64_t SnapToVsyncLocked(
      int64_t presentation_time_us,
      int64_t adjusted_release_time_ns,
      const VideoFrameReleasePlatform::VsyncSnapshot& vsync,
      bool suppress_vsync_lead_offset);

  int64_t NextMonotonicReleaseNsLocked(int64_t desired_release_ns,
                                       int64_t vsync_duration_ns) const;
  void ResetHysteresisLocked();
  void ResetIncrementalSmootherLocked();
  void ResetAllTimingStateLocked();
  void SyncSurfaceGenerationIfNeededLocked();
  void MaybeUpdateSurfaceFrameRateLocked(double effective_playback_rate,
                                         bool is_effectively_playing);
  void LogStatsSummaryLocked(const char* reason,
                             int64_t vsync_duration_ns) const;

  VideoRenderAlgorithmHost* const host_;
  VideoFrameTracker* const frame_tracker_;
  const uint32_t mask_;
  const bool release_frames_after_audio_starts_;
  const ClockNowNsFn clock_now_ns_;
  std::unique_ptr<VideoFrameReleasePlatform> platform_;

  mutable std::mutex state_mutex_;
  std::atomic<int> dropped_frames_{0};

  double playback_rate_ = 1.0;
  bool first_frame_released_ = false;
  bool has_clock_advanced_ = false;
  int64_t seek_to_time_ = 0;
  bool has_gate_media_time_ = true;
  int64_t gate_media_time_us_ = 0;

  bool has_last_rendered_media_time_ = false;
  int64_t last_rendered_media_time_us_ = 0;
  int64_t last_render_realtime_ns_ = 0;
  bool has_previous_drawn_release_time_ = false;
  int64_t previous_drawn_release_time_ns_ = 0;
  bool had_same_vsync_skip_since_last_render_ = false;

  // Incremental float-speed drift smoother state (Bit 3).
  bool has_pending_last_presentation_time_ = false;
  int64_t pending_last_adjusted_frame_index_ = kIndexUnset;
  int64_t pending_last_adjusted_release_time_ns_ = 0;
  int64_t pending_last_presentation_time_us_ = 0;
  int64_t last_adjusted_frame_index_ = kIndexUnset;
  int64_t last_adjusted_release_time_ns_ = 0;
  int64_t last_adjusted_presentation_time_us_ = 0;
  int64_t progressive_frame_index_ = 0;

  // Legacy-parity smoother state (Mask = 1 when Bit 3 is off).
  bool legacy_have_sync_ = false;
  bool has_legacy_last_frame_pts_ = false;
  double legacy_last_playback_rate_ = -1.0;
  int64_t legacy_last_frame_pts_us_ = 0;
  int64_t legacy_adjusted_last_frame_time_ns_ = 0;
  int64_t legacy_pending_adjusted_frame_time_ns_ = 0;
  int64_t legacy_sync_unadjusted_release_time_ns_ = 0;
  int64_t legacy_sync_frame_pts_ns_ = 0;
  int64_t legacy_frame_count_ = 0;

  // VSYNC midpoint hysteresis state (Bit 8).
  bool has_pending_hysteresis_pts_ = false;
  int64_t pending_hysteresis_pts_us_ = 0;
  int64_t last_vsync_hysteresis_offset_ns_ = 0;
  int64_t pending_vsync_hysteresis_offset_ns_ = 0;
  int64_t last_seen_vsync_duration_ns_ = -1;

  // Cadence estimator & Surface.setFrameRate vote cache (Bits 4, 7, 13).
  FixedFrameRateEstimator frame_rate_estimator_;
  uint64_t last_voted_surface_generation_ = 0;
  float last_voted_surface_fps_ = 0.0f;
  bool has_voted_surface_ = false;

  Stats stats_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_RENDER_ALGORITHM_ANDROID_V2_H_
