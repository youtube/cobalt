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

#include "starboard/shared/starboard/media/decoder_state_tracker.h"

#include <algorithm>
#include <optional>
#include <ostream>
#include <utility>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {
namespace {

constexpr int kMaxInFlightFrames = 100;
constexpr std::optional<int64_t> kFrameTrackerLogIntervalUs =
    5'000'000;  // 5 sec.

std::string to_ms_string(std::optional<int64_t> us_opt) {
  if (!us_opt) {
    return "(nullopt)";
  }
  return std::to_string(*us_opt / 1'000);
}

}  // namespace

DecoderStateTracker::DecoderStateTracker(int max_frames,
                                         StateChangedCB state_changed_cb)
    : max_frames_(max_frames),
      state_changed_cb_(std::move(state_changed_cb)),
      task_runner_("frame_tracker") {
  SB_CHECK(state_changed_cb_);
  if (kFrameTrackerLogIntervalUs) {
    task_runner_.Schedule(
        [this]() { LogStateAndReschedule(*kFrameTrackerLogIntervalUs); },
        *kFrameTrackerLogIntervalUs);
  }
  SB_LOG(INFO) << "DecoderStateTracker is created: max_frames=" << max_frames_
               << ", log_interval(msec)="
               << to_ms_string(kFrameTrackerLogIntervalUs);
}

void DecoderStateTracker::AddFrame(int64_t presentation_time_us) {
  std::lock_guard lock(mutex_);

  if (frames_in_flight_.find(presentation_time_us) != frames_in_flight_.end()) {
    SB_LOG(WARNING) << "Duplicate frame PTS: " << presentation_time_us;
  }
  frames_in_flight_[presentation_time_us] = FrameStatus::kDecoding;
}

void DecoderStateTracker::SetFrameDecoded(int64_t presentation_time_us) {
  std::lock_guard lock(mutex_);

  auto it = frames_in_flight_.find(presentation_time_us);
  if (it == frames_in_flight_.end()) {
    SB_LOG(ERROR) << __func__
                  << ": Unknown frame decoded, pts=" << presentation_time_us;
    frames_in_flight_[presentation_time_us] = FrameStatus::kDecoded;
  } else {
    it->second = FrameStatus::kDecoded;
  }
}

void DecoderStateTracker::OnFrameReleased(int64_t presentation_time_us,
                                          int64_t release_us) {
  int64_t delay_us = std::max(release_us - CurrentMonotonicTime(), int64_t{0});
  task_runner_.Schedule(
      [this, presentation_time_us] {
        bool should_signal = false;
        {
          std::lock_guard lock(mutex_);
          bool was_full = IsFull_Locked();
          frames_in_flight_.erase(presentation_time_us);
          if (was_full && !IsFull_Locked()) {
            should_signal = true;
          }
        }
        if (should_signal) {
          state_changed_cb_();
        }
      },
      delay_us);
}

DecoderStateTracker::State DecoderStateTracker::GetCurrentState() const {
  std::lock_guard lock(mutex_);
  return GetCurrentState_Locked();
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

bool DecoderStateTracker::CanAcceptMore() {
  std::lock_guard lock(mutex_);
  return !IsFull_Locked();
}

bool DecoderStateTracker::IsFull_Locked() const {
  State state = GetCurrentState_Locked();

  // We accept more frames if no decoded frames have been generated yet.
  // Some devices need a large number of frames when generating the 1st
  // decoded frame. See b/405467220#comment36 for details.
  if (state.decoded_frames == 0 && state.total_frames() < kMaxInFlightFrames) {
    return false;
  }
  return state.total_frames() >= max_frames_;
}

void DecoderStateTracker::LogStateAndReschedule(int64_t log_interval_us) {
  SB_DCHECK(task_runner_.BelongsToCurrentThread());

  SB_LOG(INFO) << "DecoderStateTracker state: " << GetCurrentState();

  task_runner_.Schedule(
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
