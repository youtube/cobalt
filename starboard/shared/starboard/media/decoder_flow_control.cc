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

#include "starboard/shared/starboard/media/decoder_flow_control.h"

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

constexpr int kMaxDecodingHistory = 20;
constexpr int64_t kDecodingTimeWarningThresholdUs = 1'000'000;

class ThrottlingDecoderFlowControl : public DecoderFlowControl {
 public:
  ThrottlingDecoderFlowControl(int max_frames,
                               int64_t log_interval_us,
                               StateChangedCB state_changed_cb);
  ~ThrottlingDecoderFlowControl() override = default;

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

  std::deque<int64_t> decoding_start_times_us_;
  std::deque<int64_t> previous_decoding_times_us_;

  JobThread task_runner_;

  int entering_frame_id_ = 0;
  int decoded_frame_id_ = 0;
};

ThrottlingDecoderFlowControl::ThrottlingDecoderFlowControl(
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
  SB_LOG(INFO) << "ThrottlingDecoderFlowControl is created: max_frames="
               << max_frames_
               << ", log_interval(msec)=" << (log_interval_us / 1'000);
}

bool ThrottlingDecoderFlowControl::AddFrame(int64_t presentation_time_us) {
  std::lock_guard lock(mutex_);

  if (state_.total_frames() >= max_frames_) {
    SB_LOG(WARNING) << __func__ << " accepts no more frames: frames="
                    << state_.total_frames() << "/" << max_frames_;
    return false;
  }
  state_.decoding_frames++;
  decoding_start_times_us_.push_back(CurrentMonotonicTime());

  UpdateState_Locked();
  entering_frame_id_++;

  SB_LOG(INFO) << "AddFrame: id=" << entering_frame_id_
               << ", pts(msec)=" << presentation_time_us / 1'000
               << ", decoding=" << state_.decoding_frames
               << ", decoded=" << state_.decoded_frames;
  return true;
}

bool ThrottlingDecoderFlowControl::SetFrameDecoded(
    int64_t presentation_time_us) {
  std::lock_guard lock(mutex_);

  if (state_.decoding_frames == 0) {
    SB_LOG(ERROR) << __func__ << " < no decoding frames"
                  << ", pts(msec)=" << presentation_time_us / 1000;
    return false;
  }
  state_.decoding_frames--;
  state_.decoded_frames++;
  SB_CHECK_GE(state_.decoded_frames, 0);

  SB_CHECK(!decoding_start_times_us_.empty());

  auto start_time = decoding_start_times_us_.front();
  decoding_start_times_us_.pop_front();
  auto decoding_time_us = CurrentMonotonicTime() - start_time;
  decoded_frame_id_++;
  SB_LOG(INFO) << "SetFrameDecoded: id=" << decoded_frame_id_
               << ", pts(msec)=" << presentation_time_us / 1000
               << ", decoding-elapsed time(msec)=" << (decoding_time_us / 1'000)
               << ", decoding=" << state_.decoding_frames
               << ", decoded=" << state_.decoded_frames;
  if (decoding_time_us > kDecodingTimeWarningThresholdUs) {
    SB_LOG(WARNING) << "Decoding time exceeded threshold: "
                    << decoding_time_us / 1'000 << " msec";
  }
  previous_decoding_times_us_.push_back(decoding_time_us);
  if (previous_decoding_times_us_.size() > kMaxDecodingHistory) {
    previous_decoding_times_us_.pop_front();
  }

  UpdateState_Locked();
  UpdateDecodingStat_Locked();
  return true;
}

bool ThrottlingDecoderFlowControl::ReleaseFrameAt(int64_t release_us) {
  std::lock_guard lock(mutex_);

  if (state_.decoded_frames == 0) {
    SB_LOG(ERROR) << __func__ << " < no decoded frames";
    return false;
  }

  int64_t delay_us = std::max<int64_t>(release_us - CurrentMonotonicTime(), 0);
  task_runner_.Schedule(
      [this] {
        state_changed_cb_();

        std::lock_guard lock(mutex_);
        state_.decoded_frames--;
        UpdateState_Locked();
      },
      delay_us);

  return true;
}

DecoderFlowControl::State ThrottlingDecoderFlowControl::GetCurrentState()
    const {
  std::lock_guard lock(mutex_);
  return state_;
}

bool ThrottlingDecoderFlowControl::CanAcceptMore() {
  std::lock_guard lock(mutex_);
  if (state_.total_frames() < max_frames_) {
    return true;
  }

  return false;
}

void ThrottlingDecoderFlowControl::UpdateDecodingStat_Locked() {
  if (previous_decoding_times_us_.empty()) {
    return;
  }
  const auto [min_it, max_it] = std::minmax_element(
      previous_decoding_times_us_.cbegin(), previous_decoding_times_us_.cend());
  state_.min_decoding_time_us = *min_it;
  state_.max_decoding_time_us = *max_it;

  const int64_t sum = std::accumulate(previous_decoding_times_us_.cbegin(),
                                      previous_decoding_times_us_.cend(), 0LL);
  state_.avg_decoding_time_us = sum / previous_decoding_times_us_.size();
}

void ThrottlingDecoderFlowControl::UpdateState_Locked() {
  SB_CHECK_LE(state_.total_frames(), max_frames_);
  SB_CHECK_GE(state_.decoding_frames, 0);
  SB_CHECK_GE(state_.decoded_frames, 0);
}

void ThrottlingDecoderFlowControl::LogStateAndReschedule(
    int64_t log_interval_us) {
  SB_DCHECK(task_runner_.BelongsToCurrentThread());

  SB_LOG(INFO) << "DecoderFlowControl state: " << GetCurrentState();

  task_runner_.Schedule(
      [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
      log_interval_us);
}

class NoOpDecoderFlowControl : public DecoderFlowControl {
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
std::unique_ptr<DecoderFlowControl> DecoderFlowControl::CreateNoOp() {
  return std::make_unique<NoOpDecoderFlowControl>();
}

// static
std::unique_ptr<DecoderFlowControl> DecoderFlowControl::CreateThrottling(
    int max_frames,
    int64_t log_interval_us,
    StateChangedCB state_changed_cb) {
  return std::make_unique<ThrottlingDecoderFlowControl>(
      max_frames, log_interval_us, std::move(state_changed_cb));
}

std::ostream& operator<<(std::ostream& os,
                         const DecoderFlowControl::State& status) {
  os << "{decoding: " << status.decoding_frames
     << ", decoded: " << status.decoded_frames
     << ", min decoding(msec): " << status.min_decoding_time_us / 1'000
     << ", max decoding(msec): " << status.max_decoding_time_us / 1'000
     << ", avg decoding(msec): " << status.avg_decoding_time_us / 1'000 << "}";
  return os;
}

}  // namespace starboard
