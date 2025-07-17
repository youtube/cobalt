#include "starboard/shared/starboard/media/frame_tracker.h"

#include <algorithm>
#include <ostream>

#include "starboard/common/log.h"

namespace starboard::shared::starboard::media {

std::ostream& operator<<(std::ostream& os, const FrameTracker::State& status) {
  os << "{decoding: " << status.decoding_frames
     << ", decoded: " << status.decoded_frames
     << ", decoding (hw): " << status.decoding_frames_high_water_mark
     << ", decoded (hw): " << status.decoded_frames_high_water_mark
     << ", total (hw): " << status.total_frames_high_water_mark << "}";
  return os;
}

FrameTracker::FrameTracker(int max_frames) : max_frames_(max_frames) {}

bool FrameTracker::AddFrame() {
  std::lock_guard lock(mutex_);
  if (decoding_frames_ + decoded_frames_ >= max_frames_) {
    return false;
  }
  ++decoding_frames_;
  UpdateHighWaterMarks_Locked();
  return true;
}

bool FrameTracker::SetFrameDecoded() {
  std::lock_guard lock(mutex_);
  if (decoding_frames_ == 0) {
    return false;
  }
  --decoding_frames_;
  ++decoded_frames_;
  UpdateHighWaterMarks_Locked();
  return true;
}

bool FrameTracker::ReleaseFrame() {
  std::lock_guard lock(mutex_);
  if (decoded_frames_ == 0) {
    return false;
  }
  --decoded_frames_;
  UpdateHighWaterMarks_Locked();
  return true;
}

void FrameTracker::ReleaseFrameAt(int64_t release_time) {
  // TODO(b/289330342): Implement this method.
}

void FrameTracker::Reset() {
  std::lock_guard lock(mutex_);
  decoding_frames_ = 0;
  decoded_frames_ = 0;
  decoding_frames_high_water_mark_ = 0;
  decoded_frames_high_water_mark_ = 0;
  total_frames_high_water_mark_ = 0;
  deferred_input_buffer_indices_ = {};
}

FrameTracker::State FrameTracker::GetCurrentState() const {
  std::lock_guard lock(mutex_);
  return {decoding_frames_, decoded_frames_, decoding_frames_high_water_mark_,
          decoded_frames_high_water_mark_, total_frames_high_water_mark_};
}

bool FrameTracker::IsFull() const {
  std::lock_guard lock(mutex_);
  return decoding_frames_ + decoded_frames_ >= max_frames_;
}

void FrameTracker::DeferInputBuffer(int buffer_index) {
  std::lock_guard lock(mutex_);
  deferred_input_buffer_indices_.push(buffer_index);
}

std::optional<int> FrameTracker::GetDeferredInputBuffer() {
  std::lock_guard lock(mutex_);
  if (deferred_input_buffer_indices_.empty()) {
    return std::nullopt;
  }
  int buffer_index = deferred_input_buffer_indices_.front();
  deferred_input_buffer_indices_.pop();
  return buffer_index;
}

void FrameTracker::UpdateHighWaterMarks_Locked() {
  decoding_frames_high_water_mark_ =
      std::max(decoding_frames_high_water_mark_, decoding_frames_);
  decoded_frames_high_water_mark_ =
      std::max(decoded_frames_high_water_mark_, decoded_frames_);
  total_frames_high_water_mark_ = std::max(total_frames_high_water_mark_,
                                           decoding_frames_ + decoded_frames_);
}

}  // namespace starboard::shared::starboard::media
