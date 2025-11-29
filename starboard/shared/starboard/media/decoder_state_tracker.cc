// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

constexpr int kInitialMaxFramesInDecoder = 4;

// Maximum number of endoding frames to accept when no frame is generated.
// Some devices need a large number of frames when generating the 1st
// decoded frame. See b/405467220#comment36 for details.
constexpr int kMaxAllowedFramesWhenNoDecodedFrameYet = 10;

std::string to_ms_string(std::optional<int64_t> us_opt) {
  if (!us_opt) {
    return "(nullopt)";
  }
  return std::to_string(*us_opt / 1'000);
}

}  // namespace

DecoderStateTracker::DecoderStateTracker(FrameReleaseCB frame_released_cb)
    : DecoderStateTracker(kInitialMaxFramesInDecoder,
                          std::move(frame_released_cb),
                          /*frame_log_internval_us=*/std::nullopt) {}

DecoderStateTracker::DecoderStateTracker(int max_frames,
                                         FrameReleaseCB frame_released_cb,
                                         std::optional<int> log_interval_us)
    : max_frames_(max_frames),
      frame_released_cb_(std::move(frame_released_cb)),
      job_thread_(std::make_unique<shared::starboard::player::JobThread>(
          "DecStateTrack")) {
  SB_CHECK(frame_released_cb_);
  if (log_interval_us) {
    job_thread_->Schedule(
        [this, log_interval_us] { LogStateAndReschedule(*log_interval_us); },
        *log_interval_us);
  }
  SB_LOG(INFO) << "DecoderStateTracker is created: max_frames=" << max_frames_
               << ", log_interval(msec)=" << to_ms_string(log_interval_us);
}

DecoderStateTracker::~DecoderStateTracker() {
  SB_LOG(INFO) << "Destroying DecoderStateTracker.";
}

void DecoderStateTracker::SetFrameAdded(int64_t presentation_time_us) {
  std::lock_guard lock(mutex_);

  if (disabled_) {
    return;
  }
  // frames_in_flight_ can exceed max_frames_ initially to accommodate
  // decoders that require a larger batch of input frames before producing the
  // first decoded output.
  if (frames_in_flight_.size() >
      std::max(max_frames_, kMaxAllowedFramesWhenNoDecodedFrameYet)) {
    EngageKillSwitch_Locked("Too many frames in flight: state=" +
                                ToString(GetCurrentState_Locked()),
                            presentation_time_us);
    return;
  }
  if (frames_in_flight_.find(presentation_time_us) != frames_in_flight_.end()) {
    EngageKillSwitch_Locked("Duplicate frame input", presentation_time_us);
    return;
  }
  frames_in_flight_[presentation_time_us] = FrameStatus::kDecoding;

  if (frames_in_flight_.size() >= max_frames_) {
    reached_max_ = true;
  }
}

void DecoderStateTracker::SetEosFrameAdded() {
  std::lock_guard lock(mutex_);

  if (disabled_) {
    return;
  }
  SB_LOG(INFO) << "EOS frame is added and we disable this tracker, since we no "
                  "longer needs it";
  disabled_ = true;
}

void DecoderStateTracker::SetFrameDecoded(int64_t presentation_time_us) {
  std::lock_guard lock(mutex_);

  if (disabled_) {
    return;
  }

  auto it = frames_in_flight_.upper_bound(presentation_time_us);
  for (auto i = frames_in_flight_.begin(); i != it; ++i) {
    i->second = FrameStatus::kDecoded;
  }
}

void DecoderStateTracker::SetFrameReleasedAt(int64_t presentation_time_us,
                                             int64_t release_us) {
  {
    std::lock_guard lock(mutex_);
    if (disabled_) {
      return;
    }
  }

  int64_t delay_us = std::max(release_us - CurrentMonotonicTime(), int64_t{0});
  job_thread_->Schedule(
      [this, presentation_time_us] {
        {
          std::lock_guard lock(mutex_);
          if (disabled_) {
            return;
          }
          auto it = frames_in_flight_.upper_bound(presentation_time_us);
          frames_in_flight_.erase(frames_in_flight_.begin(), it);

          if (reached_max_ && frames_in_flight_.size() <= kFramesLowWatermark) {
            int old_max = max_frames_;
            max_frames_++;
            reached_max_ = false;
            SB_LOG(WARNING)
                << "Bump up max frames from " << old_max << " to "
                << max_frames_ << ": state=" << GetCurrentState_Locked();
          }
        }
        frame_released_cb_();
      },
      delay_us);
}

void DecoderStateTracker::Reset() {
  std::lock_guard lock(mutex_);
  frames_in_flight_.clear();
  disabled_ = false;
  reached_max_ = false;
  // We keep the existing max_frames_ instead of resetting it to the initial
  // value. If it was dynamically increased during a previous playback session,
  // it likely means this device needs the higher limit to avoid stalls.
  SB_LOG(INFO) << "DecoderStateTracker reset.";
}

DecoderStateTracker::State DecoderStateTracker::GetCurrentStateForTest() const {
  std::lock_guard lock(mutex_);
  if (disabled_) {
    return {};
  }
  return GetCurrentState_Locked();
}

bool DecoderStateTracker::CanAcceptMore() {
  std::lock_guard lock(mutex_);
  if (disabled_) {
    return true;
  }
  return !IsFull_Locked();
}

DecoderStateTracker::State DecoderStateTracker::GetCurrentState_Locked() const {
  State state;
  for (auto const& [pts, status] : frames_in_flight_) {
    if (status == FrameStatus::kDecoding) {
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
  frame_released_cb_();
}

void DecoderStateTracker::LogStateAndReschedule(int64_t log_interval_us) {
  // This is running on the thread managed by job_owner_.
  // But we don't have BelongsToCurrentThread() method in DecoderStateTracker
  // anymore. SB_CHECK(BelongsToCurrentThread()); // Removed

  {
    std::lock_guard lock(mutex_);
    if (disabled_) {
      SB_LOG(INFO) << "DecoderStateTracker state: DISABLED";
    } else {
      SB_LOG(INFO) << "DecoderStateTracker state: " << GetCurrentState_Locked();
    }
  }

  job_thread_->Schedule(
      [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
      log_interval_us);
}

std::ostream& operator<<(std::ostream& os,
                         const DecoderStateTracker::State& status) {
  return os << "{decoding: " << status.decoding_frames
            << ", decoded: " << status.decoded_frames
            << ", total: " << status.total_frames() << "}";
}

}  // namespace starboard
