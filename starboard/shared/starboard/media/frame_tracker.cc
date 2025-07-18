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

#include "starboard/shared/starboard/media/frame_tracker.h"

#include <algorithm>
#include <deque>
#include <memory>
#include <numeric>
#include <ostream>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard::shared::starboard::media {
namespace {

constexpr int kMaxDecodingHistory = 30;
constexpr int64_t kDecodingTimeWarningThresholdUs = 50'000;  // 50 ms

class FrameTrackerImpl : public FrameTracker {
 public:
  FrameTrackerImpl(int max_frames,
                   int64_t log_interval_us,
                   FrameReleasedCB frame_released_cb);
  ~FrameTrackerImpl() override = default;

  bool AddFrame() override;
  bool SetFrameDecoded() override;
  bool ReleaseFrameAt(int64_t release_us) override;

  State GetCurrentState() const override;
  bool IsFull() override;

 private:
  void UpdateDecodingStat_Locked();
  void UpdateState_Locked();
  void LogStateAndReschedule(int64_t log_interval_us);

  const int max_frames_;
  const FrameReleasedCB frame_released_cb_;

  mutable std::mutex mutex_;
  State state_;  // GUARDED_BY mutex_;

  std::deque<int64_t> decoding_start_times_us_;
  std::deque<int64_t> previous_decoding_times_us_;

  ::starboard::shared::starboard::player::JobThread task_runner_;
};

FrameTrackerImpl::FrameTrackerImpl(int max_frames,
                                   int64_t log_interval_us,
                                   FrameReleasedCB frame_released_cb)
    : max_frames_(max_frames),
      frame_released_cb_(std::move(frame_released_cb)),
      state_({}),
      task_runner_("frame_tracker") {
  SB_CHECK(frame_released_cb_);
  if (log_interval_us > 0) {
    task_runner_.Schedule(
        [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
        log_interval_us);
  }
  SB_LOG(INFO) << "FrameTracker is created: max_frames=" << max_frames_
               << ", log_interval(msec)=" << (log_interval_us / 1'000);
}

bool FrameTrackerImpl::AddFrame() {
  std::lock_guard lock(mutex_);
  if (state_.total_frames() == max_frames_) {
    return false;
  }
  state_.decoding_frames++;
  decoding_start_times_us_.push_back(CurrentMonotonicTime());

  UpdateState_Locked();
  return true;
}

bool FrameTrackerImpl::SetFrameDecoded() {
  std::lock_guard lock(mutex_);
  if (state_.decoding_frames == 0) {
    return false;
  }
  state_.decoding_frames--;
  state_.decoded_frames++;
  SB_CHECK_GE(state_.decoded_frames, 0);

  if (!decoding_start_times_us_.empty()) {
    auto start_time = decoding_start_times_us_.front();
    decoding_start_times_us_.pop_front();
    auto decoding_time = CurrentMonotonicTime() - start_time;
    if (decoding_time > kDecodingTimeWarningThresholdUs) {
      SB_LOG(WARNING) << "Decoding time exceeded threshold: "
                      << decoding_time / 1'000 << " msec";
    }
    previous_decoding_times_us_.push_back(decoding_time);
    if (previous_decoding_times_us_.size() > kMaxDecodingHistory) {
      previous_decoding_times_us_.pop_front();
    }
  }

  UpdateState_Locked();
  UpdateDecodingStat_Locked();
  return true;
}

bool FrameTrackerImpl::ReleaseFrameAt(int64_t release_us) {
  std::lock_guard lock(mutex_);

  if (state_.decoded_frames == 0) {
    return false;
  }

  int64_t delay_us = std::max<int64_t>(release_us - CurrentMonotonicTime(), 0);
  task_runner_.Schedule(
      [this] {
        frame_released_cb_();

        std::lock_guard lock(mutex_);
        state_.decoded_frames--;
        UpdateState_Locked();
      },
      delay_us);

  return true;
}

FrameTracker::State FrameTrackerImpl::GetCurrentState() const {
  std::lock_guard lock(mutex_);
  return state_;
}

bool FrameTrackerImpl::IsFull() {
  std::lock_guard lock(mutex_);
  return state_.total_frames() == max_frames_;
}

void FrameTrackerImpl::UpdateDecodingStat_Locked() {
  if (previous_decoding_times_us_.empty()) {
    return;
  }
  int64_t min_decoding_time_us = previous_decoding_times_us_[0];
  int64_t max_decoding_time_us = previous_decoding_times_us_[0];
  int64_t sum = 0;
  for (auto decoding_us : previous_decoding_times_us_) {
    min_decoding_time_us = std::min(min_decoding_time_us, decoding_us);
    max_decoding_time_us = std::max(max_decoding_time_us, decoding_us);
    sum += decoding_us;
  }

  state_.min_decoding_time_us = min_decoding_time_us;
  state_.max_decoding_time_us = max_decoding_time_us;
  state_.avg_decoding_time_us = sum / previous_decoding_times_us_.size();
}

void FrameTrackerImpl::UpdateState_Locked() {
  SB_CHECK_LE(state_.total_frames(), max_frames_);
  SB_CHECK_GE(state_.decoding_frames, 0);
  SB_CHECK_GE(state_.decoded_frames, 0);

  state_.decoding_frames_high_water_mark =
      std::max(state_.decoding_frames_high_water_mark, state_.decoding_frames);
  state_.decoded_frames_high_water_mark =
      std::max(state_.decoded_frames_high_water_mark, state_.decoded_frames);
  state_.total_frames_high_water_mark =
      std::max(state_.total_frames_high_water_mark,
               state_.decoding_frames + state_.decoded_frames);
}

void FrameTrackerImpl::LogStateAndReschedule(int64_t log_interval_us) {
  SB_DCHECK(task_runner_.BelongsToCurrentThread());

  SB_LOG(INFO) << "FrameTracker state: " << GetCurrentState();

  task_runner_.Schedule(
      [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
      log_interval_us);
}

}  // namespace

// static
std::unique_ptr<FrameTracker> FrameTracker::Create(
    int max_frames,
    int64_t log_interval_us,
    FrameReleasedCB frame_released_cb) {
  return std::make_unique<FrameTrackerImpl>(max_frames, log_interval_us,
                                            std::move(frame_released_cb));
}

std::ostream& operator<<(std::ostream& os, const FrameTracker::State& status) {
  os << "{decoding: " << status.decoding_frames
     << ", decoded: " << status.decoded_frames
     << ", decoding (hw): " << status.decoding_frames_high_water_mark
     << ", decoded (hw): " << status.decoded_frames_high_water_mark
     << ", total (hw): " << status.total_frames_high_water_mark
     << ", min decoding(msec): " << status.min_decoding_time_us / 1'000
     << ", max decoding(msec): " << status.max_decoding_time_us / 1'000
     << ", avg decoding(msec): " << status.avg_decoding_time_us / 1'000 << "}";
  return os;
}

}  // namespace starboard::shared::starboard::media
