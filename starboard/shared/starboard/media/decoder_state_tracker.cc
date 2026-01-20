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

constexpr int64_t kLogIntervalUs = 5'000'000;  // 5 sec.

}  // namespace

DecoderStateTracker::DecoderStateTracker(int initial_max_frames,
                                         FrameReleaseCB frame_released_cb)
    : max_frames_(initial_max_frames),
      frame_released_cb_(std::move(frame_released_cb)),
      job_thread_(std::make_unique<shared::starboard::player::JobThread>(
          "DecStateTrack")) {
  SB_CHECK(frame_released_cb_);
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  StartPeriodicalLogging(kLogIntervalUs);
#endif
}

DecoderStateTracker::~DecoderStateTracker() {
  SB_LOG(INFO) << "Destroying DecoderStateTracker.";

  std::unique_ptr<shared::starboard::player::JobThread> thread_to_destroy;
  {
    std::lock_guard lock(mutex_);
    // Move the thread out of the member variable while holding the lock. This
    // ensures subsequent calls to Schedule() see a nullptr and return safely.
    thread_to_destroy = std::move(job_thread_);
  }
  // The thread is joined here as it goes out of scope, without holding the
  // lock, avoiding potential deadlocks.
  if (thread_to_destroy) {
    thread_to_destroy->job_queue()->StopSoon();
  }
}

void DecoderStateTracker::OnFrameAdded(int64_t presentation_us) {
  std::unique_lock lock(mutex_);

  if (disabled_ || eos_added_) {
    return;
  }
  // This is fail-safe logic.
  // If frames_in_flight_ grows to an unexpectedly large size, we disable the
  // |DecoderStateTracker| since something has gone wrong.
  if (frames_in_flight_.size() >
      std::max(max_frames_, kMaxAllowedFramesWhenNoDecodedFrameYet)) {
    EngageKillSwitch_Locked("Too many frames in flight: state=" +
                                ToString(GetCurrentState_Locked()),
                            presentation_us);

    // Release the lock before invoking the callback to prevent potential
    // re-entrancy deadlocks.
    lock.unlock();
    frame_released_cb_();
    return;
  }
  if (frames_in_flight_.find(presentation_us) != frames_in_flight_.end()) {
    EngageKillSwitch_Locked("Duplicate frame input", presentation_us);

    // Release the lock before invoking the callback to prevent potential
    // re-entrancy deadlocks.
    lock.unlock();
    frame_released_cb_();
    return;
  }

  frames_in_flight_[presentation_us] = FrameStatus::kDecoding;

  if (frames_in_flight_.size() >= max_frames_) {
    reached_max_ = true;
  }
}

void DecoderStateTracker::OnEosAdded() {
  std::lock_guard lock(mutex_);

  if (disabled_) {
    return;
  }
  SB_LOG(INFO) << "EOS frame is added.";
  eos_added_ = true;
}

void DecoderStateTracker::OnFrameDecoded(int64_t presentation_us) {
  std::lock_guard lock(mutex_);

  if (disabled_) {
    return;
  }

  auto it = frames_in_flight_.upper_bound(presentation_us);
  for (auto i = frames_in_flight_.begin(); i != it; ++i) {
    i->second = FrameStatus::kDecoded;
  }
}

void DecoderStateTracker::OnReleased(int64_t presentation_us,
                                     int64_t release_us) {
  std::lock_guard lock(mutex_);
  if (disabled_) {
    return;
  }
  if (job_thread_ == nullptr) {
    return;
  }

  int64_t delay_us = std::max(release_us - CurrentMonotonicTime(), int64_t{0});
  job_thread_->Schedule(
      [this, presentation_us] {
        {
          std::lock_guard lock(mutex_);
          if (disabled_) {
            return;
          }
          auto it = frames_in_flight_.upper_bound(presentation_us);
          frames_in_flight_.erase(frames_in_flight_.begin(), it);

          if (reached_max_ && frames_in_flight_.size() <= kFramesLowWatermark) {
            // If the number of frames in flight drops to the low-water mark
            // after we have reached the max frames, it means the current max
            // frames is too small to keep the decoder busy. We bump up the
            // max frames to avoid underrun.
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

bool DecoderStateTracker::CanAcceptMore() const {
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
}

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
void DecoderStateTracker::StartPeriodicalLogging(int64_t log_interval_us) {
  std::lock_guard lock(mutex_);
  job_thread_->Schedule(
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

  if (job_thread_ == nullptr) {
    return;
  }
  job_thread_->Schedule(
      [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
      log_interval_us);
}
#endif

std::ostream& operator<<(std::ostream& os,
                         const DecoderStateTracker::State& status) {
  return os << "{decoding: " << status.decoding_frames
            << ", decoded: " << status.decoded_frames
            << ", total: " << status.total_frames() << "}";
}

}  // namespace starboard
