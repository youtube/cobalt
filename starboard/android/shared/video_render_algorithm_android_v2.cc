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

#include "starboard/android/shared/video_render_algorithm_android_v2.h"

#include <time.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/features.h"

namespace starboard {
namespace {

namespace v2 = video_render_algorithm_v2;

constexpr int64_t kBufferTooLateThresholdUs = -32'000;   // -32 ms
constexpr int64_t kBufferReadyThresholdUs = 50'000;      // +50 ms
constexpr int64_t kMinTextureReadyThresholdUs = 11'000;  // +11 ms
constexpr int64_t kDefaultVsyncDurationUs = 16'667;      // ~60 Hz fallback
constexpr int64_t kDefaultVsyncDurationNs = 16'666'667LL;

// VideoRendererImpl warns when Render() is not called for >66 ms, so Bit 12
// clamps dynamic output waits strictly within [8 ms, 50 ms].
constexpr int64_t kMinDynamicWaitDurationUs = 8'000;   // 8 ms
constexpr int64_t kMaxDynamicWaitDurationUs = 50'000;  // 50 ms
constexpr int64_t kDeadlineLeadMarginUs = 8'000;       // Wake 8 ms early

constexpr int64_t kStarvationFloorNs = 100'000'000LL;          // 100 ms
constexpr int64_t kStarvationFloorAfterSkipNs = 85'000'000LL;  // 0.85 * 100 ms
constexpr int64_t kMaxAllowedDriftNs = 20'000'000LL;           // 20 ms
constexpr int64_t kLegacyMinFramesForAdjustment = 6;
constexpr int64_t kVsyncOffsetPercentage = 80;
constexpr int64_t kMaxClampedEarlyUs = 1'000'000'000LL;  // +-1000 s
constexpr double kMaxClampedStepNs = 1.0e15;
constexpr double kMaxValidPlaybackRate = 1.0e6;

int64_t DefaultClockNowNs() {
  timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<int64_t>(now.tv_sec) * 1'000'000'000LL +
         static_cast<int64_t>(now.tv_nsec);
}

bool IsValidPlaybackRate(double rate) {
  return std::isfinite(rate) && rate > 0.0 && rate <= kMaxValidPlaybackRate;
}

int64_t SaturatingUsToNs(int64_t time_us) {
  constexpr int64_t kMaxSafeUs = std::numeric_limits<int64_t>::max() / 1'000LL;
  constexpr int64_t kMinSafeUs = std::numeric_limits<int64_t>::min() / 1'000LL;
  if (time_us > kMaxSafeUs) {
    return std::numeric_limits<int64_t>::max();
  }
  if (time_us < kMinSafeUs) {
    return std::numeric_limits<int64_t>::min();
  }
  return time_us * 1'000LL;
}

bool AbsDiffExceedsLimit(int64_t a, int64_t b, uint64_t limit) {
  const uint64_t ua = static_cast<uint64_t>(a);
  const uint64_t ub = static_cast<uint64_t>(b);
  const uint64_t diff = a >= b ? (ua - ub) : (ub - ua);
  return diff > limit;
}

int64_t ClampEarlyUs(double raw_early_us) {
  if (!std::isfinite(raw_early_us)) {
    return 0;
  }
  if (raw_early_us > static_cast<double>(kMaxClampedEarlyUs)) {
    return kMaxClampedEarlyUs;
  }
  if (raw_early_us < static_cast<double>(-kMaxClampedEarlyUs)) {
    return -kMaxClampedEarlyUs;
  }
  return static_cast<int64_t>(raw_early_us);
}

int64_t LegacyAdjustFrameTimeForPlaybackRate(int64_t frame_time_ns,
                                             double playback_rate) {
  if (!IsValidPlaybackRate(playback_rate) || playback_rate == 1.0) {
    return frame_time_ns;
  }
  constexpr int64_t kMultiplier = 4;
  const int64_t quantized =
      static_cast<int64_t>(std::llround(playback_rate * kMultiplier));
  if (quantized <= 0) {
    return frame_time_ns;
  }
  const int64_t quotient = frame_time_ns / quantized;
  int64_t scaled = 0;
  if (__builtin_mul_overflow(quotient, kMultiplier, &scaled)) {
    return quotient >= 0 ? std::numeric_limits<int64_t>::max()
                         : std::numeric_limits<int64_t>::min();
  }
  return scaled;
}

int64_t StatelessClosestVsync(int64_t release_time_ns,
                              int64_t sampled_vsync_time_ns,
                              int64_t vsync_duration_ns) {
  int64_t delta_ns = 0;
  if (__builtin_sub_overflow(release_time_ns, sampled_vsync_time_ns,
                             &delta_ns)) {
    return release_time_ns;
  }
  const int64_t vsync_count = delta_ns / vsync_duration_ns;
  int64_t count_offset_ns = 0;
  int64_t snapped_time_ns = 0;
  if (__builtin_mul_overflow(vsync_duration_ns, vsync_count,
                             &count_offset_ns) ||
      __builtin_add_overflow(sampled_vsync_time_ns, count_offset_ns,
                             &snapped_time_ns)) {
    return release_time_ns;
  }
  int64_t snapped_before_ns = 0;
  int64_t snapped_after_ns = 0;
  if (release_time_ns <= snapped_time_ns) {
    if (__builtin_sub_overflow(snapped_time_ns, vsync_duration_ns,
                               &snapped_before_ns)) {
      return release_time_ns;
    }
    snapped_after_ns = snapped_time_ns;
  } else {
    snapped_before_ns = snapped_time_ns;
    if (__builtin_add_overflow(snapped_time_ns, vsync_duration_ns,
                               &snapped_after_ns)) {
      return release_time_ns;
    }
  }
  const int64_t after_diff_ns = snapped_after_ns - release_time_ns;
  const int64_t before_diff_ns = release_time_ns - snapped_before_ns;
  return after_diff_ns < before_diff_ns ? snapped_after_ns : snapped_before_ns;
}

bool QueryReleaseFramesAfterAudioStarts() {
#if !BUILDFLAG(IS_STARBOARD)
  return features::FeatureList::IsEnabled(
      features::kReleaseVideoFramesAfterAudioStarts);
#else
  return false;
#endif
}

}  // namespace

uint32_t NormalizeVideoRenderAlgorithmMask(uint32_t raw_mask, int api_level) {
  // Bit 31 (sign bit) is never valid; Bit 30 forces the legacy algorithm.
  if ((raw_mask & (1u << 31)) != 0 ||
      (raw_mask & v2::kForceLegacyAlgorithm) != 0 ||
      (raw_mask & v2::kEnableNewAlgorithm) == 0) {
    return 0u;
  }
  uint32_t mask = raw_mask;
  if ((mask & v2::kSurfaceFrameRateSeamlessOnly) != 0) {
    mask |= v2::kSurfaceSetFrameRate;
  }
  if ((mask & v2::kSurfaceSetFrameRate) != 0) {
    mask |= v2::kDynamicDisplayRefreshRate;
  }
  if ((mask & v2::kFixedFrameRateEstimator) != 0) {
    mask |= v2::kIncrementalFloatSpeedSmoother;
  }
  if (api_level < 30) {
    mask &= ~(v2::kSurfaceSetFrameRate | v2::kSurfaceFrameRateSeamlessOnly);
  }
  if (api_level < 33) {
    mask &= ~v2::kVsyncCallbackFrameTimeline;
  }
  return mask;
}

bool ShouldUseVideoRenderAlgorithmAndroidV2(int raw_mask, int api_level) {
  if (raw_mask <= 0) {
    return false;
  }
  return NormalizeVideoRenderAlgorithmMask(static_cast<uint32_t>(raw_mask),
                                           api_level) != 0u;
}

VideoRenderAlgorithmAndroidV2::VideoRenderAlgorithmAndroidV2(
    VideoRenderAlgorithmHost* host,
    VideoFrameTracker* frame_tracker,
    uint32_t raw_mask)
    : VideoRenderAlgorithmAndroidV2(host,
                                    frame_tracker,
                                    raw_mask,
                                    android_get_device_api_level(),
                                    /*platform_for_testing=*/nullptr,
                                    /*clock_now_ns_for_testing=*/{}) {}

VideoRenderAlgorithmAndroidV2::VideoRenderAlgorithmAndroidV2(
    VideoRenderAlgorithmHost* host,
    VideoFrameTracker* frame_tracker,
    uint32_t raw_mask,
    int api_level,
    std::unique_ptr<VideoFrameReleasePlatform> platform_for_testing,
    ClockNowNsFn clock_now_ns_for_testing)
    : host_(host),
      frame_tracker_(frame_tracker),
      mask_(NormalizeVideoRenderAlgorithmMask(raw_mask, api_level)),
      release_frames_after_audio_starts_(QueryReleaseFramesAfterAudioStarts()),
      clock_now_ns_(clock_now_ns_for_testing
                        ? std::move(clock_now_ns_for_testing)
                        : ClockNowNsFn(&DefaultClockNowNs)),
      platform_(platform_for_testing
                    ? std::move(platform_for_testing)
                    : std::make_unique<VideoFrameReleasePlatformHelper>(
                          /*track_display_changes=*/HasBit(
                              v2::kDynamicDisplayRefreshRate),
                          /*prefer_vsync_callback_v33=*/
                          HasBit(v2::kVsyncCallbackFrameTimeline),
                          /*pause_sampler_when_inactive=*/
                          HasBit(v2::kDeadlineWakeupsAndSamplerHygiene))) {
  SB_CHECK(host_);
  SB_CHECK(platform_);
  if (!host_->is_playback_paused()) {
    host_->SetPlaybackRate(playback_rate_);
  }
}

VideoRenderAlgorithmAndroidV2::~VideoRenderAlgorithmAndroidV2() {
  std::lock_guard lock(state_mutex_);
  const auto vsync = platform_->GetVsyncSnapshot();
  LogStatsSummaryLocked("dtor", vsync.vsync_duration_ns);
  if (HasBit(v2::kSurfaceSetFrameRate)) {
    platform_->ClearSurfaceFrameRate();
  }
  // Note: VideoRendererImpl::~VideoRendererImpl() destroys |decoder_| (|host_|
  // and |frame_tracker_|) before |algorithm_|, so never dereference |host_| or
  // |frame_tracker_| here.
}

void VideoRenderAlgorithmAndroidV2::Render(
    MediaTimeProvider* media_time_provider,
    std::list<scoped_refptr<VideoFrame>>* frames,
    VideoRendererSink::DrawFrameCB draw_frame_cb) {
  SB_CHECK(media_time_provider);
  SB_CHECK(frames);
  SB_CHECK(draw_frame_cb);

  std::lock_guard lock(state_mutex_);

  const bool has_bit_1 = HasBit(v2::kFirstFrameImmediateAndAudioGate);
  const bool has_bit_2 = HasBit(v2::kStarvationFloor100ms);
  const bool has_bit_4 = HasBit(v2::kFixedFrameRateEstimator);
  const bool has_bit_5 = HasBit(v2::kSkipSameVsyncReleaseTime);
  const bool has_bit_7 = HasBit(v2::kSurfaceSetFrameRate);
  const bool has_bit_10 = HasBit(v2::kAudioEosDrawLastAndLastFrameGuard);
  const bool has_bit_11 = HasBit(v2::kDisableTextureModeLookahead);
  const bool has_bit_12 = HasBit(v2::kDeadlineWakeupsAndSamplerHygiene);

  const bool sink_paused = host_->is_playback_paused();
  if (has_bit_7) {
    SyncSurfaceGenerationIfNeededLocked();
  }

  // Fast paused path when Bit 12 is enabled and no immediate first-frame
  // preview can be drawn: stop Choreographer sampling, re-arm the unpause audio
  // gate, and sleep 50 ms without locking AudioRendererPcm's clock mutex.
  if (has_bit_12 && sink_paused && (frames->empty() || first_frame_released_)) {
    has_gate_media_time_ = false;
    if (has_clock_advanced_) {
      has_clock_advanced_ = false;
      ResetIncrementalSmootherLocked();
    }
    platform_->SetPlaybackActive(false);
    host_->SetNextTickWaitDurationUs(kMaxDynamicWaitDurationUs);
    return;
  }

  if (sink_paused && frames->empty()) {
    has_gate_media_time_ = false;
    if (has_clock_advanced_) {
      has_clock_advanced_ = false;
      if (HasBit(v2::kIncrementalFloatSpeedSmoother)) {
        ResetIncrementalSmootherLocked();
      }
    }
  }

  const VideoFrameReleasePlatform::VsyncSnapshot vsync =
      platform_->GetVsyncSnapshot();
  if (vsync.vsync_duration_ns > 0 &&
      last_seen_vsync_duration_ns_ != vsync.vsync_duration_ns) {
    if (last_seen_vsync_duration_ns_ > 0) {
      // Clear midpoint bias from the previous display refresh rate.
      ResetHysteresisLocked();
    }
    last_seen_vsync_duration_ns_ = vsync.vsync_duration_ns;
  }

  const bool is_texture_mode =
      host_->GetOutputMode() == kSbPlayerOutputModeDecodeToTexture;
  const bool suppress_vsync_lead_offset = has_bit_11 && is_texture_mode;
  int64_t ready_threshold_us = kBufferReadyThresholdUs;
  if (suppress_vsync_lead_offset) {
    const int64_t vsync_us = vsync.vsync_duration_ns > 0
                                 ? (vsync.vsync_duration_ns / 1'000)
                                 : kDefaultVsyncDurationUs;
    ready_threshold_us = std::min<int64_t>(
        std::max<int64_t>(vsync_us, kMinTextureReadyThresholdUs),
        kBufferReadyThresholdUs);
  }

  // With Bit 12 enabled, sample GetCurrentMediaTime() and CLOCK_MONOTONIC at
  // most once per Render() pass.
  bool has_cached_media_time = false;
  bool has_pass_start_ns = false;
  MediaTimeInfo cached_media_time_info{};
  int64_t pass_start_ns = 0;
  std::optional<int64_t> waiting_frame_early_us;
  bool waiting_on_clock_gate = false;
  bool observed_effectively_playing = !sink_paused;

  auto get_clock_sample = [&](MediaTimeInfo* out_info, int64_t* out_now_ns) {
    if (has_bit_12) {
      if (!has_cached_media_time) {
        cached_media_time_info = media_time_provider->GetCurrentMediaTime();
        has_cached_media_time = true;
      }
      if (!has_pass_start_ns) {
        pass_start_ns = clock_now_ns_();
        has_pass_start_ns = true;
      }
      *out_info = cached_media_time_info;
      *out_now_ns = pass_start_ns;
    } else {
      *out_info = media_time_provider->GetCurrentMediaTime();
      *out_now_ns = clock_now_ns_();
    }
  };

  while (!frames->empty()) {
    if (frames->front()->is_end_of_stream()) {
      frames->pop_front();
      SB_DCHECK(frames->empty())
          << "Expected end of stream output buffer to be the last buffer.";
      break;
    }

    // Bit 1: Release the very first frame after startup or Seek() immediately
    // (even while paused), bypassing drift smoothing and [-32ms, +50ms) checks.
    if (has_bit_1 && !first_frame_released_) {
      MediaTimeInfo first_frame_time_info{};
      int64_t now_ns = 0;
      if (sink_paused) {
        if (has_bit_12) {
          if (!has_pass_start_ns) {
            pass_start_ns = clock_now_ns_();
            has_pass_start_ns = true;
          }
          now_ns = pass_start_ns;
        } else {
          now_ns = clock_now_ns_();
        }
      } else {
        get_clock_sample(&first_frame_time_info, &now_ns);
      }

      if (!sink_paused && first_frame_time_info.is_eos_played && has_bit_10) {
        // Fall through to the Bit 10 audio-EOS handler below so intermediate
        // frames are skipped and only the final tail frame is drawn.
      } else {
        const int64_t first_pts_us = frames->front()->timestamp();
        if (has_bit_4 || has_bit_7) {
          frame_rate_estimator_.OnNextFrame(SaturatingUsToNs(first_pts_us));
          stats_.estimated_fps = frame_rate_estimator_.GetFrameRate();
        }
        const int64_t release_ns =
            NextMonotonicReleaseNsLocked(now_ns, vsync.vsync_duration_ns);
        [[maybe_unused]] auto status =
            draw_frame_cb(frames->front(), release_ns);
        SB_DCHECK_EQ(status, VideoRendererSink::kReleased);
        frames->pop_front();
        first_frame_released_ = true;
        last_render_realtime_ns_ = now_ns;
        has_previous_drawn_release_time_ = true;
        previous_drawn_release_time_ns_ = release_ns;
        had_same_vsync_skip_since_last_render_ = false;
        ++stats_.rendered_frames;
        ++stats_.immediate_first_frames;

        if (sink_paused) {
          has_gate_media_time_ = false;
          has_clock_advanced_ = false;
          observed_effectively_playing = false;
          break;
        }
        last_rendered_media_time_us_ = first_frame_time_info.media_time;
        has_last_rendered_media_time_ = true;
        if (!has_gate_media_time_) {
          has_gate_media_time_ = true;
          gate_media_time_us_ = first_frame_time_info.media_time;
        }
        continue;
      }
    }

    MediaTimeInfo media_time_info{};
    int64_t system_time_ns = 0;
    get_clock_sample(&media_time_info, &system_time_ns);

    const int64_t playback_time = media_time_info.media_time;
    const double playback_rate = media_time_info.playback_rate;
    const bool valid_rate = IsValidPlaybackRate(playback_rate);
    const bool is_effectively_playing =
        media_time_info.is_playing && !sink_paused && valid_rate;
    observed_effectively_playing = is_effectively_playing;

    if (has_bit_1) {
      if (!is_effectively_playing) {
        // Record the stalled media timestamp (or clear when sink_paused) so
        // unpausing waits for the audio sink to advance before Frame 1+.
        if (sink_paused) {
          has_gate_media_time_ = false;
        } else {
          has_gate_media_time_ = true;
          gate_media_time_us_ = playback_time;
          waiting_on_clock_gate = true;
        }
        if (has_clock_advanced_) {
          has_clock_advanced_ = false;
          ResetIncrementalSmootherLocked();
        }
        break;
      }
      if (!has_clock_advanced_) {
        if (!has_gate_media_time_) {
          has_gate_media_time_ = true;
          gate_media_time_us_ = playback_time;
        }
        if (playback_time == gate_media_time_us_ &&
            !media_time_info.is_eos_played) {
          waiting_on_clock_gate = true;
          break;
        }
        has_clock_advanced_ = true;
      }
    } else if (release_frames_after_audio_starts_) {
      if (first_frame_released_ &&
          (!is_effectively_playing || playback_time == seek_to_time_)) {
        waiting_on_clock_gate = !sink_paused;
        break;
      }
      if (is_effectively_playing && playback_time != seek_to_time_) {
        has_clock_advanced_ = true;
      }
    } else {
      if (!media_time_info.is_playing || sink_paused) {
        waiting_on_clock_gate = !sink_paused;
        break;
      }
      if (playback_time != seek_to_time_) {
        has_clock_advanced_ = true;
      }
    }

    if (!sink_paused && valid_rate && playback_rate != playback_rate_) {
      playback_rate_ = playback_rate;
      host_->SetPlaybackRate(playback_rate);
    }

    if (media_time_info.is_eos_played) {
      if (has_bit_10) {
        // When video EOS has not arrived yet, skip intermediate buffers down to
        // one held frame without drawing so multi-batch drains never overwrite
        // SurfaceFlinger slots; once EOS arrives, draw the single final frame.
        const bool back_is_eos = frames->back()->is_end_of_stream();
        if (!back_is_eos) {
          while (frames->size() > 1) {
            if (frame_tracker_) {
              frame_tracker_->OnFrameSkipped(frames->front()->timestamp());
            }
            frames->pop_front();
            ++stats_.skipped_audio_eos;
          }
          waiting_on_clock_gate = true;
          break;
        }

        const size_t non_eos_count = frames->size() - 1;
        for (size_t i = 1; i < non_eos_count; ++i) {
          if (frame_tracker_) {
            frame_tracker_->OnFrameSkipped(frames->front()->timestamp());
          }
          frames->pop_front();
          ++stats_.skipped_audio_eos;
        }
        if (non_eos_count >= 1) {
          const int64_t release_ns = NextMonotonicReleaseNsLocked(
              system_time_ns, vsync.vsync_duration_ns);
          [[maybe_unused]] auto status =
              draw_frame_cb(frames->front(), release_ns);
          SB_DCHECK_EQ(status, VideoRendererSink::kReleased);
          frames->pop_front();
          first_frame_released_ = true;
          last_render_realtime_ns_ = system_time_ns;
          last_rendered_media_time_us_ = playback_time;
          has_last_rendered_media_time_ = true;
          has_previous_drawn_release_time_ = true;
          previous_drawn_release_time_ns_ = release_ns;
          had_same_vsync_skip_since_last_render_ = false;
          ++stats_.rendered_frames;
        }
        if (!frames->empty() && frames->front()->is_end_of_stream()) {
          frames->pop_front();
          SB_DCHECK(frames->empty());
        }
        break;
      }

      bool popped = false;
      while (frames->size() > 1) {
        frames->pop_front();
        popped = true;
      }
      if (popped) {
        continue;
      }
      break;
    }

    const int64_t frame_pts_us = frames->front()->timestamp();
    const bool is_last_frame =
        frames->size() >= 2 &&
        std::next(frames->begin())->get()->is_end_of_stream();

    if (has_bit_4 || has_bit_7) {
      frame_rate_estimator_.OnNextFrame(SaturatingUsToNs(frame_pts_us));
      stats_.estimated_fps = frame_rate_estimator_.GetFrameRate();
    }
    if (has_bit_7) {
      MaybeUpdateSurfaceFrameRateLocked(valid_rate ? playback_rate : 1.0,
                                        is_effectively_playing);
    }

    const double safe_rate = valid_rate ? playback_rate : 1.0;
    const double pts_diff_us =
        static_cast<double>(frame_pts_us) - static_cast<double>(playback_time);
    int64_t early_us = ClampEarlyUs(pts_diff_us / safe_rate);
    int64_t unadjusted_release_time_ns = system_time_ns;
    if (__builtin_add_overflow(system_time_ns, early_us * 1'000LL,
                               &unadjusted_release_time_ns)) {
      unadjusted_release_time_ns = early_us >= 0
                                       ? std::numeric_limits<int64_t>::max()
                                       : std::numeric_limits<int64_t>::min();
    }

    const int64_t adjusted_release_time_ns = AdjustReleaseTimeLocked(
        frame_pts_us, unadjusted_release_time_ns, playback_rate, vsync,
        suppress_vsync_lead_offset);
    int64_t adjusted_delta_ns = 0;
    if (__builtin_sub_overflow(adjusted_release_time_ns, system_time_ns,
                               &adjusted_delta_ns)) {
      early_us = adjusted_release_time_ns >= system_time_ns
                     ? kMaxClampedEarlyUs
                     : -kMaxClampedEarlyUs;
    } else {
      early_us = adjusted_delta_ns / 1'000LL;
    }

    if (early_us < kBufferTooLateThresholdUs) {
      // Bit 10: Never drop the final video frame before EOS.
      if (has_bit_10 && is_last_frame) {
        const int64_t release_ns = NextMonotonicReleaseNsLocked(
            std::max(adjusted_release_time_ns, system_time_ns),
            vsync.vsync_duration_ns);
        [[maybe_unused]] auto status =
            draw_frame_cb(frames->front(), release_ns);
        SB_DCHECK_EQ(status, VideoRendererSink::kReleased);
        frames->pop_front();
        first_frame_released_ = true;
        last_render_realtime_ns_ = system_time_ns;
        last_rendered_media_time_us_ = playback_time;
        has_last_rendered_media_time_ = true;
        has_previous_drawn_release_time_ = true;
        previous_drawn_release_time_ns_ = release_ns;
        had_same_vsync_skip_since_last_render_ = false;
        ++stats_.rendered_frames;
        continue;
      }

      // Bit 2: Guarantee ~10 fps forward progress under sustained decode lag
      // (scaled by 0.85x to 85 ms following a same-VSYNC skip).
      const int64_t starvation_threshold_ns =
          (has_bit_5 && had_same_vsync_skip_since_last_render_)
              ? kStarvationFloorAfterSkipNs
              : kStarvationFloorNs;
      const bool clock_advanced_since_last_render =
          !has_last_rendered_media_time_ ||
          playback_time > last_rendered_media_time_us_;
      if (has_bit_2 && is_effectively_playing && has_clock_advanced_ &&
          clock_advanced_since_last_render && last_render_realtime_ns_ > 0 &&
          system_time_ns >= last_render_realtime_ns_ &&
          (system_time_ns - last_render_realtime_ns_) >=
              starvation_threshold_ns) {
        const int64_t release_ns = NextMonotonicReleaseNsLocked(
            system_time_ns, vsync.vsync_duration_ns);
        [[maybe_unused]] auto status =
            draw_frame_cb(frames->front(), release_ns);
        SB_DCHECK_EQ(status, VideoRendererSink::kReleased);
        frames->pop_front();
        first_frame_released_ = true;
        last_render_realtime_ns_ = system_time_ns;
        last_rendered_media_time_us_ = playback_time;
        has_last_rendered_media_time_ = true;
        has_previous_drawn_release_time_ = true;
        previous_drawn_release_time_ns_ = release_ns;
        had_same_vsync_skip_since_last_render_ = false;
        ++stats_.rendered_frames;
        ++stats_.forced_by_starvation_floor;
        continue;
      }

      frames->pop_front();
      dropped_frames_.fetch_add(1, std::memory_order_relaxed);
      ++stats_.dropped_late_frames;
    } else if (early_us < ready_threshold_us) {
      int64_t draw_release_time_ns = adjusted_release_time_ns;
      // Bit 5: Return buffer immediately without rendering when it snaps to
      // the exact same VSYNC timestamp as the previously drawn frame.
      if (has_bit_5 && has_previous_drawn_release_time_ &&
          adjusted_release_time_ns == previous_drawn_release_time_ns_) {
        if (has_bit_10 && is_last_frame) {
          draw_release_time_ns = NextMonotonicReleaseNsLocked(
              adjusted_release_time_ns, vsync.vsync_duration_ns);
        } else {
          if (frame_tracker_) {
            frame_tracker_->OnFrameSkipped(frame_pts_us);
          }
          frames->pop_front();
          had_same_vsync_skip_since_last_render_ = true;
          ++stats_.skipped_same_vsync;
          continue;
        }
      }

      [[maybe_unused]] auto status =
          draw_frame_cb(frames->front(), draw_release_time_ns);
      SB_DCHECK_EQ(status, VideoRendererSink::kReleased);
      frames->pop_front();
      first_frame_released_ = true;
      last_render_realtime_ns_ = system_time_ns;
      last_rendered_media_time_us_ = playback_time;
      has_last_rendered_media_time_ = true;
      has_previous_drawn_release_time_ = true;
      previous_drawn_release_time_ns_ = draw_release_time_ns;
      had_same_vsync_skip_since_last_render_ = false;
      ++stats_.rendered_frames;
    } else {
      waiting_frame_early_us = early_us;
      break;
    }
  }

  if (has_bit_12) {
    if (!has_cached_media_time && !sink_paused) {
      // Determine play/pause state even when `frames` was empty so the Java
      // Choreographer sampler stops promptly when paused via MediaTimeProvider.
      const MediaTimeInfo idle_info =
          media_time_provider->GetCurrentMediaTime();
      observed_effectively_playing =
          idle_info.is_playing && IsValidPlaybackRate(idle_info.playback_rate);
    }
    const bool keep_sampler_active =
        !sink_paused && (observed_effectively_playing ||
                         waiting_on_clock_gate || !first_frame_released_);
    platform_->SetPlaybackActive(keep_sampler_active);

    int64_t next_wait_us = kMaxDynamicWaitDurationUs;
    if (sink_paused) {
      next_wait_us = kMaxDynamicWaitDurationUs;
    } else if (!first_frame_released_ || waiting_on_clock_gate) {
      next_wait_us = kMinDynamicWaitDurationUs;
    } else if (!observed_effectively_playing) {
      next_wait_us = kMaxDynamicWaitDurationUs;
    } else if (waiting_frame_early_us.has_value()) {
      // Target waking 8 ms inside the ready window (42 ms in PunchOut mode),
      // clamped to [8 ms, 50 ms] to avoid VideoRendererImpl >66 ms warnings.
      const int64_t target_early_threshold_us =
          std::max<int64_t>(0, ready_threshold_us - kDeadlineLeadMarginUs);
      next_wait_us =
          std::clamp(*waiting_frame_early_us - target_early_threshold_us,
                     kMinDynamicWaitDurationUs, kMaxDynamicWaitDurationUs);
    } else {
      next_wait_us = kMaxDynamicWaitDurationUs;
    }
    host_->SetNextTickWaitDurationUs(next_wait_us);
  }
}

void VideoRenderAlgorithmAndroidV2::Seek(int64_t seek_to_time) {
  if (frame_tracker_) {
    frame_tracker_->Seek(seek_to_time);
  }
  std::lock_guard lock(state_mutex_);
  const auto vsync = platform_->GetVsyncSnapshot();
  LogStatsSummaryLocked("seek", vsync.vsync_duration_ns);

  first_frame_released_ = false;
  has_clock_advanced_ = false;
  seek_to_time_ = seek_to_time;
  has_gate_media_time_ = true;
  gate_media_time_us_ = seek_to_time;
  has_last_rendered_media_time_ = false;
  last_rendered_media_time_us_ = seek_to_time;
  last_render_realtime_ns_ = 0;
  has_previous_drawn_release_time_ = false;
  previous_drawn_release_time_ns_ = 0;
  had_same_vsync_skip_since_last_render_ = false;
  ResetHysteresisLocked();

  if (HasBit(v2::kIncrementalFloatSpeedSmoother) ||
      HasBit(v2::kSurfaceSetFrameRate)) {
    ResetAllTimingStateLocked();
  }
  if (HasBit(v2::kSurfaceSetFrameRate)) {
    platform_->ClearSurfaceFrameRate();
    last_voted_surface_fps_ = 0.0f;
    has_voted_surface_ = false;
    stats_.last_surface_fps_vote = 0.0f;
  }
  if (HasBit(v2::kDeadlineWakeupsAndSamplerHygiene)) {
    host_->SetNextTickWaitDurationUs(kMinDynamicWaitDurationUs);
  }
}

int VideoRenderAlgorithmAndroidV2::GetDroppedFrames() {
  if (frame_tracker_) {
    return frame_tracker_->UpdateAndGetDroppedFrames();
  }
  return dropped_frames_.load(std::memory_order_relaxed);
}

VideoRenderAlgorithmAndroidV2::Stats VideoRenderAlgorithmAndroidV2::GetStats()
    const {
  std::lock_guard lock(state_mutex_);
  return stats_;
}

int64_t VideoRenderAlgorithmAndroidV2::AdjustReleaseTimeLocked(
    int64_t presentation_time_us,
    int64_t unadjusted_release_time_ns,
    double playback_rate,
    const VideoFrameReleasePlatform::VsyncSnapshot& vsync,
    bool suppress_vsync_lead_offset) {
  if (!IsValidPlaybackRate(playback_rate)) {
    return unadjusted_release_time_ns;
  }

  const int64_t adjusted_release_time_ns =
      HasBit(v2::kIncrementalFloatSpeedSmoother)
          ? AdjustReleaseTimeIncrementalLocked(
                presentation_time_us, unadjusted_release_time_ns, playback_rate)
          : AdjustReleaseTimeLegacyParityLocked(presentation_time_us,
                                                unadjusted_release_time_ns,
                                                playback_rate);

  if (vsync.sampled_vsync_time_ns <= 0 || vsync.vsync_duration_ns <= 0) {
    return adjusted_release_time_ns;
  }
  return SnapToVsyncLocked(presentation_time_us, adjusted_release_time_ns,
                           vsync, suppress_vsync_lead_offset);
}

int64_t VideoRenderAlgorithmAndroidV2::AdjustReleaseTimeIncrementalLocked(
    int64_t presentation_time_us,
    int64_t unadjusted_release_time_ns,
    double playback_rate) {
  // Advance anchor only when evaluating a new presentation timestamp so
  // waiting frames re-probed on subsequent ticks remain idempotent.
  if (!has_pending_last_presentation_time_ ||
      presentation_time_us != pending_last_presentation_time_us_) {
    last_adjusted_frame_index_ = pending_last_adjusted_frame_index_;
    last_adjusted_release_time_ns_ = pending_last_adjusted_release_time_ns_;
    last_adjusted_presentation_time_us_ = pending_last_presentation_time_us_;
    ++progressive_frame_index_;
  }

  const int64_t current_frame_index = HasBit(v2::kFixedFrameRateEstimator)
                                          ? frame_rate_estimator_.frame_index()
                                          : progressive_frame_index_;
  const int64_t estimated_frame_duration_ns =
      HasBit(v2::kFixedFrameRateEstimator)
          ? frame_rate_estimator_.GetFrameDurationNs()
          : FixedFrameRateEstimator::kTimeUnset;

  int64_t adjusted_release_time_ns = unadjusted_release_time_ns;
  if (last_adjusted_frame_index_ != kIndexUnset) {
    double step_ns = 0.0;
    if (estimated_frame_duration_ns != FixedFrameRateEstimator::kTimeUnset &&
        current_frame_index >= last_adjusted_frame_index_) {
      step_ns = (static_cast<double>(estimated_frame_duration_ns) *
                 static_cast<double>(current_frame_index -
                                     last_adjusted_frame_index_)) /
                playback_rate;
    } else {
      step_ns = (static_cast<double>(presentation_time_us) -
                 static_cast<double>(last_adjusted_presentation_time_us_)) *
                1'000.0 / playback_rate;
    }

    int64_t candidate_ns = 0;
    if (std::isfinite(step_ns) && std::abs(step_ns) <= kMaxClampedStepNs &&
        !__builtin_add_overflow(last_adjusted_release_time_ns_,
                                static_cast<int64_t>(std::llround(step_ns)),
                                &candidate_ns) &&
        !AbsDiffExceedsLimit(candidate_ns, unadjusted_release_time_ns,
                             static_cast<uint64_t>(kMaxAllowedDriftNs))) {
      adjusted_release_time_ns = candidate_ns;
    } else {
      ResetIncrementalSmootherLocked();
      ++stats_.smoother_resets;
    }
  }

  has_pending_last_presentation_time_ = true;
  pending_last_adjusted_frame_index_ = current_frame_index;
  pending_last_adjusted_release_time_ns_ = adjusted_release_time_ns;
  pending_last_presentation_time_us_ = presentation_time_us;
  return adjusted_release_time_ns;
}

int64_t VideoRenderAlgorithmAndroidV2::AdjustReleaseTimeLegacyParityLocked(
    int64_t presentation_time_us,
    int64_t unadjusted_release_time_ns,
    double playback_rate) {
  if (playback_rate != legacy_last_playback_rate_) {
    legacy_have_sync_ = false;
    legacy_last_playback_rate_ = playback_rate;
    ResetHysteresisLocked();
  }

  const int64_t frame_pts_ns = SaturatingUsToNs(presentation_time_us);
  int64_t adjusted_frame_time_ns = frame_pts_ns;
  int64_t adjusted_release_time_ns = unadjusted_release_time_ns;

  auto is_legacy_drift_too_large = [&](int64_t candidate_frame_ns) {
    int64_t elapsed_frame_ns = 0;
    int64_t elapsed_release_ns = 0;
    if (__builtin_sub_overflow(candidate_frame_ns, legacy_sync_frame_pts_ns_,
                               &elapsed_frame_ns) ||
        __builtin_sub_overflow(unadjusted_release_time_ns,
                               legacy_sync_unadjusted_release_time_ns_,
                               &elapsed_release_ns)) {
      return true;
    }
    const int64_t expected_release_ns =
        LegacyAdjustFrameTimeForPlaybackRate(elapsed_frame_ns, playback_rate);
    return AbsDiffExceedsLimit(elapsed_release_ns, expected_release_ns,
                               static_cast<uint64_t>(kMaxAllowedDriftNs));
  };

  if (legacy_have_sync_) {
    if (!has_legacy_last_frame_pts_ ||
        presentation_time_us != legacy_last_frame_pts_us_) {
      ++legacy_frame_count_;
      legacy_adjusted_last_frame_time_ns_ =
          legacy_pending_adjusted_frame_time_ns_;
    }
    if (legacy_frame_count_ >= kLegacyMinFramesForAdjustment) {
      int64_t total_pts_delta_ns = 0;
      int64_t candidate_frame_ns = 0;
      if (__builtin_sub_overflow(frame_pts_ns, legacy_sync_frame_pts_ns_,
                                 &total_pts_delta_ns) ||
          __builtin_add_overflow(legacy_adjusted_last_frame_time_ns_,
                                 total_pts_delta_ns / legacy_frame_count_,
                                 &candidate_frame_ns) ||
          is_legacy_drift_too_large(candidate_frame_ns)) {
        legacy_have_sync_ = false;
        ResetHysteresisLocked();
        ++stats_.smoother_resets;
      } else {
        adjusted_frame_time_ns = candidate_frame_ns;
        const int64_t scaled_delta_ns = LegacyAdjustFrameTimeForPlaybackRate(
            adjusted_frame_time_ns - legacy_sync_frame_pts_ns_, playback_rate);
        if (__builtin_add_overflow(legacy_sync_unadjusted_release_time_ns_,
                                   scaled_delta_ns,
                                   &adjusted_release_time_ns)) {
          adjusted_release_time_ns = unadjusted_release_time_ns;
          legacy_have_sync_ = false;
          ResetHysteresisLocked();
          ++stats_.smoother_resets;
        }
      }
    } else if (is_legacy_drift_too_large(frame_pts_ns)) {
      legacy_have_sync_ = false;
      ResetHysteresisLocked();
      ++stats_.smoother_resets;
    }
  }

  if (!legacy_have_sync_) {
    legacy_sync_frame_pts_ns_ = frame_pts_ns;
    legacy_sync_unadjusted_release_time_ns_ = unadjusted_release_time_ns;
    legacy_frame_count_ = 0;
    legacy_have_sync_ = true;
  }

  has_legacy_last_frame_pts_ = true;
  legacy_last_frame_pts_us_ = presentation_time_us;
  legacy_pending_adjusted_frame_time_ns_ = adjusted_frame_time_ns;
  return adjusted_release_time_ns;
}

int64_t VideoRenderAlgorithmAndroidV2::SnapToVsyncLocked(
    int64_t presentation_time_us,
    int64_t adjusted_release_time_ns,
    const VideoFrameReleasePlatform::VsyncSnapshot& vsync,
    bool suppress_vsync_lead_offset) {
  const int64_t vsync_duration_ns = vsync.vsync_duration_ns;
  // In DecodeToTexture (Bit 11), SurfaceTexture latches buffers immediately on
  // releaseOutputBuffer rather than waiting for SurfaceFlinger's VSYNC target,
  // so do not subtract SurfaceFlinger's 80% VSYNC lead offset.
  const int64_t raw_offset_ns =
      vsync.vsync_offset_ns >= 0
          ? vsync.vsync_offset_ns
          : ((vsync_duration_ns * kVsyncOffsetPercentage) / 100LL);
  const int64_t vsync_offset_ns =
      suppress_vsync_lead_offset ? 0 : raw_offset_ns;

  if (!HasBit(v2::kVsyncMidpointHysteresis)) {
    const int64_t chosen =
        StatelessClosestVsync(adjusted_release_time_ns,
                              vsync.sampled_vsync_time_ns, vsync_duration_ns);
    int64_t result = chosen;
    if (__builtin_sub_overflow(chosen, vsync_offset_ns, &result)) {
      return adjusted_release_time_ns;
    }
    return result;
  }

  if (!has_pending_hysteresis_pts_ ||
      presentation_time_us != pending_hysteresis_pts_us_) {
    last_vsync_hysteresis_offset_ns_ = pending_vsync_hysteresis_offset_ns_;
  }
  has_pending_hysteresis_pts_ = true;
  pending_hysteresis_pts_us_ = presentation_time_us;

  int64_t delta_ns = 0;
  if (__builtin_sub_overflow(adjusted_release_time_ns,
                             vsync.sampled_vsync_time_ns, &delta_ns)) {
    return adjusted_release_time_ns;
  }
  const int64_t vsync_count = delta_ns / vsync_duration_ns;
  int64_t count_offset_ns = 0;
  int64_t snapped_time_ns = 0;
  if (__builtin_mul_overflow(vsync_duration_ns, vsync_count,
                             &count_offset_ns) ||
      __builtin_add_overflow(vsync.sampled_vsync_time_ns, count_offset_ns,
                             &snapped_time_ns)) {
    return adjusted_release_time_ns;
  }
  int64_t snapped_before_ns = 0;
  int64_t snapped_after_ns = 0;
  if (adjusted_release_time_ns <= snapped_time_ns) {
    if (__builtin_sub_overflow(snapped_time_ns, vsync_duration_ns,
                               &snapped_before_ns)) {
      return adjusted_release_time_ns;
    }
    snapped_after_ns = snapped_time_ns;
  } else {
    snapped_before_ns = snapped_time_ns;
    if (__builtin_add_overflow(snapped_time_ns, vsync_duration_ns,
                               &snapped_after_ns)) {
      return adjusted_release_time_ns;
    }
  }
  const int64_t after_diff_ns = snapped_after_ns - adjusted_release_time_ns;
  const int64_t before_diff_ns = adjusted_release_time_ns - snapped_before_ns;
  const int64_t diffs_diff_ns = std::abs(after_diff_ns - before_diff_ns);

  // Only update sticky hysteresis within D/2 of a midpoint so alternating
  // clear/ambiguous cadences (such as 60fps on 90Hz) preserve their lock.
  if (diffs_diff_ns < vsync_duration_ns / 2) {
    const int64_t hysteresis_range_ns = vsync_duration_ns / 4;
    if (diffs_diff_ns < hysteresis_range_ns) {
      if (last_vsync_hysteresis_offset_ns_ != 0) {
        pending_vsync_hysteresis_offset_ns_ = last_vsync_hysteresis_offset_ns_;
      } else {
        pending_vsync_hysteresis_offset_ns_ = after_diff_ns < before_diff_ns
                                                  ? -hysteresis_range_ns
                                                  : hysteresis_range_ns;
      }
    } else {
      pending_vsync_hysteresis_offset_ns_ = 0;
    }
  } else {
    pending_vsync_hysteresis_offset_ns_ = last_vsync_hysteresis_offset_ns_;
  }

  const int64_t chosen_vsync_ns =
      (after_diff_ns + pending_vsync_hysteresis_offset_ns_ < before_diff_ns)
          ? snapped_after_ns
          : snapped_before_ns;
  int64_t result = chosen_vsync_ns;
  if (__builtin_sub_overflow(chosen_vsync_ns, vsync_offset_ns, &result)) {
    return adjusted_release_time_ns;
  }
  return result;
}

int64_t VideoRenderAlgorithmAndroidV2::NextMonotonicReleaseNsLocked(
    int64_t desired_release_ns,
    int64_t vsync_duration_ns) const {
  if (!has_previous_drawn_release_time_) {
    return desired_release_ns;
  }
  const int64_t step_ns =
      vsync_duration_ns > 0 ? vsync_duration_ns : kDefaultVsyncDurationNs;
  int64_t min_next_ns = 0;
  if (__builtin_add_overflow(previous_drawn_release_time_ns_, step_ns,
                             &min_next_ns)) {
    return desired_release_ns;
  }
  return std::max(desired_release_ns, min_next_ns);
}

void VideoRenderAlgorithmAndroidV2::ResetHysteresisLocked() {
  has_pending_hysteresis_pts_ = false;
  pending_hysteresis_pts_us_ = 0;
  last_vsync_hysteresis_offset_ns_ = 0;
  pending_vsync_hysteresis_offset_ns_ = 0;
}

void VideoRenderAlgorithmAndroidV2::ResetIncrementalSmootherLocked() {
  last_adjusted_frame_index_ = kIndexUnset;
  pending_last_adjusted_frame_index_ = kIndexUnset;
  has_pending_last_presentation_time_ = false;
  pending_last_presentation_time_us_ = 0;
  ResetHysteresisLocked();
}

void VideoRenderAlgorithmAndroidV2::ResetAllTimingStateLocked() {
  ResetIncrementalSmootherLocked();
  progressive_frame_index_ = 0;
  frame_rate_estimator_.Reset();
  stats_.estimated_fps = FixedFrameRateEstimator::kNoFrameRate;
}

void VideoRenderAlgorithmAndroidV2::SyncSurfaceGenerationIfNeededLocked() {
  if (!has_voted_surface_ ||
      host_->GetOutputMode() != kSbPlayerOutputModePunchOut) {
    return;
  }
  const uint64_t surface_gen = host_->GetSurfaceGeneration();
  if (surface_gen == last_voted_surface_generation_) {
    return;
  }
  jni_zero::ScopedJavaLocalRef<jobject> surface = host_->AcquireJavaSurface();
  if (!surface) {
    platform_->ClearSurfaceFrameRate();
    has_voted_surface_ = false;
    last_voted_surface_fps_ = 0.0f;
    last_voted_surface_generation_ = surface_gen;
    stats_.last_surface_fps_vote = 0.0f;
    return;
  }
  if (last_voted_surface_fps_ > 0.0f) {
    platform_->UpdateSurfaceFrameRate(
        surface, last_voted_surface_fps_,
        HasBit(v2::kSurfaceFrameRateSeamlessOnly));
  }
  last_voted_surface_generation_ = surface_gen;
}

void VideoRenderAlgorithmAndroidV2::MaybeUpdateSurfaceFrameRateLocked(
    double effective_playback_rate,
    bool is_effectively_playing) {
  if (host_->GetOutputMode() != kSbPlayerOutputModePunchOut) {
    return;
  }
  const float media_fps = frame_rate_estimator_.GetFrameRate();
  float target_fps = 0.0f;
  if (is_effectively_playing && media_fps > 0.0f &&
      IsValidPlaybackRate(effective_playback_rate)) {
    const double scaled =
        static_cast<double>(media_fps) * effective_playback_rate;
    if (std::isfinite(scaled) && scaled > 0.0 && scaled <= 10'000.0) {
      target_fps = static_cast<float>(scaled);
    }
  }

  const uint64_t surface_gen = host_->GetSurfaceGeneration();
  const bool seamless_only = HasBit(v2::kSurfaceFrameRateSeamlessOnly);

  if (target_fps <= 0.0f) {
    if (last_voted_surface_fps_ > 0.0f || has_voted_surface_) {
      platform_->ClearSurfaceFrameRate();
      last_voted_surface_fps_ = 0.0f;
      has_voted_surface_ = false;
      stats_.last_surface_fps_vote = 0.0f;
    }
    return;
  }

  if (has_voted_surface_ && surface_gen == last_voted_surface_generation_ &&
      std::abs(target_fps - last_voted_surface_fps_) < 1e-4f) {
    return;
  }

  jni_zero::ScopedJavaLocalRef<jobject> surface = host_->AcquireJavaSurface();
  if (!surface) {
    if (last_voted_surface_fps_ > 0.0f || has_voted_surface_) {
      platform_->ClearSurfaceFrameRate();
      last_voted_surface_fps_ = 0.0f;
      stats_.last_surface_fps_vote = 0.0f;
    }
    has_voted_surface_ = false;
    return;
  }

  platform_->UpdateSurfaceFrameRate(surface, target_fps, seamless_only);
  has_voted_surface_ = true;
  last_voted_surface_generation_ = surface_gen;
  last_voted_surface_fps_ = target_fps;
  stats_.last_surface_fps_vote = target_fps;
}

void VideoRenderAlgorithmAndroidV2::LogStatsSummaryLocked(
    const char* reason,
    int64_t vsync_duration_ns) const {
  const int64_t total_events =
      stats_.rendered_frames + stats_.dropped_late_frames +
      stats_.skipped_same_vsync + stats_.skipped_audio_eos;
  if (total_events == 0 && std::strcmp(reason, "dtor") != 0) {
    return;
  }
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2)
      << "[VideoRenderStats] reason=" << reason << " mask=" << mask_
      << " rendered=" << stats_.rendered_frames
      << " immediate_first=" << stats_.immediate_first_frames
      << " forced_starvation=" << stats_.forced_by_starvation_floor
      << " dropped_late=" << stats_.dropped_late_frames
      << " skipped_same_vsync=" << stats_.skipped_same_vsync
      << " skipped_audio_eos=" << stats_.skipped_audio_eos
      << " smoother_resets=" << stats_.smoother_resets
      << " cadence_fps=" << stats_.estimated_fps
      << " vsync_period_ns=" << vsync_duration_ns
      << " surface_fps_vote=" << stats_.last_surface_fps_vote
      << " vsync_samples=" << platform_->vsync_sample_count();
  SB_LOG(INFO) << oss.str();
}

}  // namespace starboard
