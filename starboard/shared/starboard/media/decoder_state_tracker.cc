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

#include "starboard/shared/starboard/media/decoder_state_tracker.h"

#include <algorithm>
#include <optional>
#include <ostream>
#include <sstream>
#include <string_view>
#include <utility>

#include "build/build_config.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"

namespace starboard {
namespace {

// Low-water mark for in-flight frames.
// If the total number of in-flight frames goes lower than this value,
// max frames will increase.
constexpr int kFramesLowWatermark = 2;

// There is one known case where a device required 6 encoded frames to produce
// the first decoded frame during 8K playback (see b/405467220#comment36).
// This constant is set to 10 to handle such cases with some safety margin.
constexpr int kMaxAllowedFramesWhenNoDecodedFrameYet = 10;

// The maximum number of frames tracked by this class. If the frame count
// exceeds this limit, the DecoderStateTracker will be disabled. The value 24
// comes from the Chromium codebase, which limits the maximum number of frames
// in the video pipeline to 24.
// https://source.chromium.org/chromium/chromium/src/+/main:media/renderers/video_renderer_impl.cc;l=43;drc=058f840149f10507597892102990f2ab15268fbd
constexpr int kMaxFramesToTrack = 24;

constexpr int64_t kLogIntervalUs = 5'000'000;  // 5 sec.

}  // namespace

DecoderStateTracker::DecoderStateTracker(int initial_max_frames)
    : max_frames_(initial_max_frames) {
  frames_in_flight_.reserve(kMaxFramesToTrack);

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  logging_thread_ =
      std::make_unique<shared::starboard::player::JobThread>("DecStateTrack");
  StartPeriodicalLogging(kLogIntervalUs);
#endif
}

DecoderStateTracker::~DecoderStateTracker() {
  SB_LOG(INFO) << "Destroying DecoderStateTracker.";

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  std::unique_ptr<shared::starboard::player::JobThread> thread_to_destroy;
  {
    std::lock_guard lock(mutex_);
    thread_to_destroy = std::move(logging_thread_);
  }
  if (thread_to_destroy) {
    thread_to_destroy->job_queue()->StopSoon();
  }
#endif
}

void DecoderStateTracker::TrackNewFrame(int64_t presentation_us) {
  std::unique_lock lock(mutex_);

  if (disabled_ || eos_added_) {
    return;
  }
  PruneReleasedFrames_Locked();

  auto it = std::lower_bound(
      frames_in_flight_.begin(), frames_in_flight_.end(), presentation_us,
      [](const auto& pair, int64_t pts) { return pair.first < pts; });

  // This is fail-safe logic.
  std::string kill_reason = [&]() -> std::string {
    if (frames_in_flight_.size() >= kMaxFramesToTrack) {
      return "frame size exceeeds limit: size=" +
             std::to_string(frames_in_flight_.size()) +
             ", limit=" + std::to_string(kMaxFramesToTrack);
    }
    // If frames_in_flight_ grows to an unexpected size, we kill the
    // |DecoderStateTracker| since something has gone wrong.
    if (frames_in_flight_.size() >
        std::max(max_frames_, kMaxAllowedFramesWhenNoDecodedFrameYet)) {
      return "Unexpected # of frames in flight: state=" +
             ToString(GetCurrentState_Locked());
    }
    if (it != frames_in_flight_.end() && it->first == presentation_us) {
      return "Duplicate frame input";
    }

    // Return empty means that we don't kill tracker.
    return "";
  }();

  if (!kill_reason.empty()) {
    EngageKillSwitch_Locked(kill_reason, presentation_us);
    return;
  }

  frames_in_flight_.insert(it, {presentation_us, {FrameStatus::kDecoding}});
  if (frames_in_flight_.size() >= max_frames_) {
    reached_max_ = true;
  }
}

void DecoderStateTracker::MarkEosReached() {
  std::lock_guard lock(mutex_);

  if (disabled_) {
    return;
  }
  SB_LOG(INFO) << "EOS frame is added.";
  eos_added_ = true;
}

void DecoderStateTracker::MarkFrameDecoded(int64_t presentation_us) {
  std::lock_guard lock(mutex_);

  if (disabled_) {
    return;
  }

  auto it = std::upper_bound(
      frames_in_flight_.begin(), frames_in_flight_.end(), presentation_us,
      [](int64_t pts, const auto& pair) { return pts < pair.first; });
  for (auto i = frames_in_flight_.begin(); i != it; ++i) {
    i->second.status = FrameStatus::kDecoded;
  }
  PruneReleasedFrames_Locked();
}

void DecoderStateTracker::MarkFrameReleased(int64_t presentation_us,
                                            int64_t release_us) {
  std::lock_guard lock(mutex_);
  if (disabled_) {
    return;
  }

  auto it = std::upper_bound(
      frames_in_flight_.begin(), frames_in_flight_.end(), presentation_us,
      [](int64_t pts, const auto& pair) { return pts < pair.first; });

  for (auto i = frames_in_flight_.begin(); i != it; ++i) {
    if (i->second.status != FrameStatus::kReleased) {
      i->second.status = FrameStatus::kReleased;
      i->second.release_time_us = release_us;
      pending_released_frames_++;
    }
  }

  PruneReleasedFrames_Locked();
}

bool DecoderStateTracker::CanAcceptMore() {
  std::lock_guard lock(mutex_);
  if (disabled_) {
    return true;
  }
  PruneReleasedFrames_Locked();
  return !IsFull_Locked();
}

void DecoderStateTracker::Reset() {
  std::lock_guard lock(mutex_);
  frames_in_flight_.clear();
  pending_released_frames_ = 0;
  eos_added_ = false;
  reached_max_ = false;
  // We keep the existing max_frames_ instead of resetting it to the initial
  // value. If it was dynamically increased during a previous playback session,
  // it likely means this device needs the higher limit to avoid stalls.
  // We also keep 'disabled_' state, since it means we encountered some fatal
  // error and we should not use this tracker anymore.
  SB_LOG(INFO) << "DecoderStateTracker reset: disabled=" << disabled_;
}

DecoderStateTracker::State DecoderStateTracker::GetCurrentStateForTest() const {
  std::lock_guard lock(mutex_);
  if (disabled_) {
    return {};
  }
  return GetCurrentState_Locked();
}

DecoderStateTracker::State DecoderStateTracker::GetCurrentState_Locked() const {
  State state;
  for (auto const& [pts, info] : frames_in_flight_) {
    if (info.status == FrameStatus::kDecoding) {
      state.decoding_frames++;
    } else {
      state.decoded_frames++;
    }
  }
  return state;
}

bool DecoderStateTracker::IsFull_Locked() const {
  const State state = GetCurrentState_Locked();
  // We accept more frames if no decoded frames have been generated yet.
  // Some devices need a large number of frames when generating the 1st
  // decoded frame. See b/405467220#comment36 for details.
  if (state.decoded_frames == 0) {
    return state.total_frames() >= kMaxAllowedFramesWhenNoDecodedFrameYet;
  }
  return state.total_frames() >= max_frames_;
}

void DecoderStateTracker::EngageKillSwitch_Locked(std::string_view reason,
                                                  int64_t pts) {
  SB_LOG(ERROR) << "KILL SWITCH ENGAGED: " << reason << ", pts=" << pts;
  disabled_ = true;
  frames_in_flight_.clear();
}

void DecoderStateTracker::PruneReleasedFrames_Locked() {
  if (pending_released_frames_ == 0) {
    return;
  }

  int64_t now_us = CurrentMonotonicTime();
  auto it = frames_in_flight_.begin();
  while (it != frames_in_flight_.end()) {
    if (it->second.status == FrameStatus::kReleased &&
        now_us >= it->second.release_time_us) {
      it = frames_in_flight_.erase(it);
      --pending_released_frames_;

      if (reached_max_ && frames_in_flight_.size() <= kFramesLowWatermark) {
        // If the number of frames in flight drops to the low-water mark
        // after we have reached the max frames, it means the current max
        // frames is too small to keep the decoder busy. We bump up the
        // max frames to avoid underrun.
        int old_max = max_frames_;
        max_frames_++;
        reached_max_ = false;
        SB_LOG(WARNING) << "Bump up max frames from " << old_max << " to "
                        << max_frames_
                        << ": state=" << GetCurrentState_Locked();
      }
    } else {
      ++it;
    }
  }
}

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
void DecoderStateTracker::StartPeriodicalLogging(int64_t log_interval_us) {
  std::lock_guard lock(mutex_);
  if (!logging_thread_) {
    return;
  }
  logging_thread_->Schedule(
      [this, log_interval_us] { LogStateAndReschedule(log_interval_us); },
      log_interval_us);
}

void DecoderStateTracker::LogStateAndReschedule(int64_t log_interval_us) {
  // This function runs on the thread managed by `job_thread_`.

  std::lock_guard lock(mutex_);
  if (disabled_) {
    SB_LOG(INFO) << "DecoderStateTracker state: DISABLED";
    return;
  }
  SB_LOG(INFO) << "DecoderStateTracker state: " << GetCurrentState_Locked();

  if (logging_thread_ == nullptr) {
    return;
  }
  logging_thread_->Schedule(
      [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
      log_interval_us);
}
#endif

std::ostream& operator<<(std::ostream& os,
                         const DecoderStateTracker::State& state) {
  return os << "{decoding=" << state.decoding_frames
            << ", decoded=" << state.decoded_frames
            << ", total=" << state.total_frames() << "}";
}

}  // namespace starboard
