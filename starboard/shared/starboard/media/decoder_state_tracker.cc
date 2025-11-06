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
#include <deque>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <numeric>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {
namespace {

constexpr bool kVerbose = false;

class DecoderStateTrackerImpl : public DecoderStateTracker {
 public:
  DecoderStateTrackerImpl(int max_frames,
                          int64_t log_interval_us,
                          StateChangedCB state_changed_cb);
  ~DecoderStateTrackerImpl() override = default;

  bool AddFrame(int64_t presentation_time_us) override;
  bool SetFrameDecoded(int64_t presentation_time_us) override;
  bool ReleaseFrameAt(int64_t release_us) override;

  State GetCurrentState() const override;
  bool CanAcceptMore() override;

 private:
  void UpdateDecodingStat_Locked();
  void UpdateState_Locked();
  void LogStateAndReschedule(int64_t log_interval_us);

  const int max_frames_;
  const StateChangedCB state_changed_cb_;

  mutable std::mutex mutex_;
  State state_;  // GUARDED_BY mutex_;

  shared::starboard::player::JobThread task_runner_;

  int entering_frame_id_ = 0;
  int decoded_frame_id_ = 0;
};

DecoderStateTrackerImpl::DecoderStateTrackerImpl(
    int max_frames,
    int64_t log_interval_us,
    StateChangedCB state_changed_cb)
    : max_frames_(max_frames),
      state_changed_cb_(std::move(state_changed_cb)),
      state_({}),
      task_runner_("frame_tracker") {
  SB_CHECK(state_changed_cb_);
  if (log_interval_us > 0) {
    task_runner_.Schedule(
        [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
        log_interval_us);
  }
  SB_LOG(INFO) << "DecoderStateTrackerImpl is created: max_frames="
               << max_frames_
               << ", log_interval(msec)=" << (log_interval_us / 1'000);
}

bool DecoderStateTrackerImpl::AddFrame(int64_t presentation_time_us) {
  std::lock_guard lock(mutex_);

  if (state_.total_frames() >= max_frames_) {
    SB_LOG(WARNING) << __func__ << " accepts no more frames: frames="
                    << state_.total_frames() << "/" << max_frames_;
    return false;
  }
  state_.decoding_frames++;

  UpdateState_Locked();
  entering_frame_id_++;

  if (kVerbose) {
    SB_LOG(INFO) << "AddFrame: id=" << entering_frame_id_
                 << ", pts(msec)=" << presentation_time_us / 1'000
                 << ", decoding=" << state_.decoding_frames
                 << ", decoded=" << state_.decoded_frames;
  }
  return true;
}

bool DecoderStateTrackerImpl::SetFrameDecoded(int64_t presentation_time_us) {
  std::lock_guard lock(mutex_);

  if (state_.decoding_frames == 0) {
    SB_LOG(ERROR) << __func__ << " < no decoding frames"
                  << ", pts(msec)=" << presentation_time_us / 1000;
    return false;
  }
  state_.decoding_frames--;
  state_.decoded_frames++;
  SB_CHECK_GE(state_.decoded_frames, 0);

  decoded_frame_id_++;
  if (kVerbose) {
    SB_LOG(INFO) << "SetFrameDecoded: id=" << decoded_frame_id_
                 << ", pts(msec)=" << presentation_time_us / 1000
                 << ", decoding=" << state_.decoding_frames
                 << ", decoded=" << state_.decoded_frames;
  }
  UpdateState_Locked();
  return true;
}

bool DecoderStateTrackerImpl::ReleaseFrameAt(int64_t release_us) {
  std::lock_guard lock(mutex_);

  if (state_.decoded_frames == 0) {
    SB_LOG(ERROR) << __func__ << " < no decoded frames";
    return false;
  }

  int64_t delay_us = std::max<int64_t>(release_us - CurrentMonotonicTime(), 0);
  task_runner_.Schedule(
      [this] {
        bool was_full;
        {
          std::lock_guard lock(mutex_);
          was_full = state_.total_frames() >= max_frames_;
          state_.decoded_frames--;
          UpdateState_Locked();
        }
        if (was_full) {
          state_changed_cb_();
        }
      },
      delay_us);

  return true;
}

DecoderStateTracker::State DecoderStateTrackerImpl::GetCurrentState() const {
  std::lock_guard lock(mutex_);
  return state_;
}

bool DecoderStateTrackerImpl::CanAcceptMore() {
  std::lock_guard lock(mutex_);
  if (state_.total_frames() < max_frames_) {
    return true;
  }

  return false;
}

void DecoderStateTrackerImpl::UpdateState_Locked() {
  SB_CHECK_LE(state_.total_frames(), max_frames_);
  SB_CHECK_GE(state_.decoding_frames, 0);
  SB_CHECK_GE(state_.decoded_frames, 0);
}

void DecoderStateTrackerImpl::LogStateAndReschedule(int64_t log_interval_us) {
  SB_DCHECK(task_runner_.BelongsToCurrentThread());

  SB_LOG(INFO) << "DecoderFlowControl state: " << GetCurrentState();

  task_runner_.Schedule(
      [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
      log_interval_us);
}

class NoOpDecoderFlowControl : public DecoderStateTracker {
 public:
  NoOpDecoderFlowControl() {
    SB_LOG(INFO) << "NoOpDecoderFlowControl is created.";
  }
  ~NoOpDecoderFlowControl() override = default;

  bool AddFrame(int64_t presentation_time_us) override { return true; }
  bool SetFrameDecoded(int64_t presentation_time_us) override { return true; }
  bool ReleaseFrameAt(int64_t release_us) override { return true; }

  State GetCurrentState() const override { return State(); }
  bool CanAcceptMore() override { return true; }
};

}  // namespace

// static
std::unique_ptr<DecoderStateTracker> DecoderStateTracker::CreateNoOp() {
  return std::make_unique<NoOpDecoderFlowControl>();
}

// static
std::unique_ptr<DecoderStateTracker> DecoderStateTracker::CreateThrottling(
    int max_frames,
    int64_t log_interval_us,
    StateChangedCB state_changed_cb) {
  return std::make_unique<DecoderStateTrackerImpl>(max_frames, log_interval_us,
                                                   std::move(state_changed_cb));
}

std::ostream& operator<<(std::ostream& os,
                         const DecoderStateTracker::State& status) {
  os << "{decoding: " << status.decoding_frames
     << ", decoded: " << status.decoded_frames << "}";
  return os;
}

}  // namespace starboard
