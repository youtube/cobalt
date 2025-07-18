#include "starboard/shared/starboard/media/frame_tracker.h"

#include <algorithm>
#include <ostream>

#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace starboard::shared::starboard::media {
namespace {
constexpr int64_t kUnassigned = -1;
constexpr int64_t kDecoding = -2;
constexpr int64_t kDecoded = -3;

}  // namespace

std::ostream& operator<<(std::ostream& os, const FrameTracker::State& status) {
  os << "{decoding: " << status.decoding_frames
     << ", decoded: " << status.decoded_frames
     << ", decoding (hw): " << status.decoding_frames_high_water_mark
     << ", decoded (hw): " << status.decoded_frames_high_water_mark
     << ", total (hw): " << status.total_frames_high_water_mark << "}";
  return os;
}

FrameTracker::FrameTracker(int max_frames) : frames_(max_frames, kUnassigned) {}

bool FrameTracker::AddFrame() {
  std::lock_guard lock(mutex_);
  PurgeReleasedFrames_Locked();

  for (auto& frame : frames_) {
    if (frame == kUnassigned) {
      frame = kDecoding;
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
}

FrameTracker::State FrameTracker::GetCurrentState() {
  std::lock_guard lock(mutex_);
  PurgeReleasedFrames_Locked();
  auto [decoding_frames, decoded_frames] = UpdateHighWaterMarks_Locked();
  return {decoding_frames, decoded_frames, decoding_frames_high_water_mark_,
          decoded_frames_high_water_mark_, total_frames_high_water_mark_};
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

}  // namespace starboard::shared::starboard::media
