#include "starboard/shared/starboard/media/frame_tracker.h"

#include <algorithm>
#include <numeric>
#include <ostream>

#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace starboard::shared::starboard::media {
namespace {
constexpr int64_t kUnassigned = -1;
constexpr int64_t kDecoding = -2;
constexpr int64_t kDecoded = -3;

constexpr int kMaxDecodingHistory = 30;

}  // namespace

std::ostream& operator<<(std::ostream& os, const FrameTracker::State& status) {
  os << "{decoding: " << status.decoding_frames
     << ", decoded: " << status.decoded_frames
     << ", decoding (hw): " << status.decoding_frames_high_water_mark
     << ", decoded (hw): " << status.decoded_frames_high_water_mark
     << ", total (hw): " << status.total_frames_high_water_mark
     << ", min decoding time: " << status.min_decoding_time_us
     << ", max decoding time: " << status.max_decoding_time_us
     << ", avg decoding time: " << status.avg_decoding_time_us << "}";
  return os;
}

FrameTracker::FrameTracker(int max_frames, int64_t log_interval_us)
    : frames_(max_frames, kUnassigned), task_runner_("frame_tracker") {
  if (log_interval_us > 0) {
    task_runner_.Schedule(
        [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
        log_interval_us);
  }
}

FrameTracker::~FrameTracker() = default;

bool FrameTracker::AddFrame() {
  std::lock_guard lock(mutex_);
  PurgeReleasedFrames_Locked();

  for (auto& frame : frames_) {
    if (frame == kUnassigned) {
      frame = kDecoding;
      decoding_start_times_us_.push_back(CurrentMonotonicTime());
      UpdateHighWaterMarks_Locked();
      return true;
    }
  }

  return false;
}

bool FrameTracker::SetFrameDecoded() {
  std::lock_guard lock(mutex_);
  for (auto& frame : frames_) {
    if (frame == kDecoding) {
      frame = kDecoded;
      if (!decoding_start_times_us_.empty()) {
        auto start_time = decoding_start_times_us_.front();
        decoding_start_times_us_.pop_front();
        auto decoding_time = CurrentMonotonicTime() - start_time;
        last_30_decoding_times_us_.push_back(decoding_time);
        if (last_30_decoding_times_us_.size() > kMaxDecodingHistory) {
          last_30_decoding_times_us_.pop_front();
        }
      }
      UpdateHighWaterMarks_Locked();
      return true;
    }
  }
  return false;
}

bool FrameTracker::ReleaseFrame() {
  return ReleaseFrameAt(CurrentMonotonicTime());
}

bool FrameTracker::ReleaseFrameAt(int64_t release_time) {
  std::lock_guard lock(mutex_);
  for (auto& frame : frames_) {
    if (frame == kDecoded) {
      frame = release_time;
      UpdateHighWaterMarks_Locked();
      return true;
    }
  }
  return false;
}

void FrameTracker::Reset() {
  std::lock_guard lock(mutex_);
  decoding_frames_high_water_mark_ = 0;
  decoded_frames_high_water_mark_ = 0;
  total_frames_high_water_mark_ = 0;
  deferred_input_buffer_indices_ = {};
  decoding_start_times_us_.clear();
  last_30_decoding_times_us_.clear();
}

FrameTracker::State FrameTracker::GetCurrentState() {
  std::lock_guard lock(mutex_);
  PurgeReleasedFrames_Locked();
  auto [decoding_frames, decoded_frames] = UpdateHighWaterMarks_Locked();
  int64_t min_decoding_time_us = 0;
  int64_t max_decoding_time_us = 0;
  int64_t avg_decoding_time_us = 0;

  if (!last_30_decoding_times_us_.empty()) {
    min_decoding_time_us = last_30_decoding_times_us_[0];
    max_decoding_time_us = last_30_decoding_times_us_[0];
    int64_t sum = 0;
    for (auto time : last_30_decoding_times_us_) {
      min_decoding_time_us = std::min(min_decoding_time_us, time);
      max_decoding_time_us = std::max(max_decoding_time_us, time);
      sum += time;
    }
    avg_decoding_time_us = sum / last_30_decoding_times_us_.size();
  }

  return {decoding_frames,
          decoded_frames,
          decoding_frames_high_water_mark_,
          decoded_frames_high_water_mark_,
          total_frames_high_water_mark_,
          min_decoding_time_us,
          max_decoding_time_us,
          avg_decoding_time_us};
}

bool FrameTracker::IsFull() {
  std::lock_guard lock(mutex_);
  PurgeReleasedFrames_Locked();
  return std::find(frames_.cbegin(), frames_.cend(), kUnassigned) ==
         frames_.cend();
}

std::pair<int, int> FrameTracker::UpdateHighWaterMarks_Locked() {
  int decoding_frames = 0;
  int decoded_frames = 0;

  for (auto& frame : frames_) {
    if (frame == kUnassigned) {
      continue;
    } else if (frame == kDecoding) {
      decoding_frames++;
      continue;
    }
    decoded_frames++;
  }

  decoding_frames_high_water_mark_ =
      std::max(decoding_frames_high_water_mark_, decoding_frames);
  decoded_frames_high_water_mark_ =
      std::max(decoded_frames_high_water_mark_, decoded_frames);
  total_frames_high_water_mark_ =
      std::max(total_frames_high_water_mark_, decoding_frames + decoded_frames);
  return {decoding_frames, decoded_frames};
}

void FrameTracker::PurgeReleasedFrames_Locked() {
  int64_t now_us = CurrentMonotonicTime();
  for (auto& frame : frames_) {
    if (0 <= frame && frame <= now_us) {
      frame = kUnassigned;
    }
  }
}

void FrameTracker::LogStateAndReschedule(int64_t log_interval_us) {
  SB_DCHECK(task_runner_.BelongsToCurrentThread());

  SB_LOG(INFO) << "FrameTracker status: " << GetCurrentState();

  task_runner_.Schedule(
      [this, log_interval_us]() { LogStateAndReschedule(log_interval_us); },
      log_interval_us);
}

}  // namespace starboard::shared::starboard::media
