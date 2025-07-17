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

void FrameTracker::AddFrame() {
  std::lock_guard lock(mutex_);
  ++decoding_frames_;
  UpdateHighWaterMarks_Locked();
}

void FrameTracker::SetFrameDecoded() {
  std::lock_guard lock(mutex_);
  --decoding_frames_;
  ++decoded_frames_;
  UpdateHighWaterMarks_Locked();
}

void FrameTracker::ReleaseFrame() {
  std::lock_guard lock(mutex_);
  --decoded_frames_;
  UpdateHighWaterMarks_Locked();
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

void FrameTracker::OnInputBufferAvailable(int buffer_index) {
  std::lock_guard lock(mutex_);
  deferred_input_buffer_indices_.push(buffer_index);
}

int FrameTracker::GetDeferredInputBuffer() {
  std::lock_guard lock(mutex_);
  if (deferred_input_buffer_indices_.empty()) {
    return -1;
  }
  int buffer_index = deferred_input_buffer_indices_.front();
  deferred_input_buffer_indices_.pop();
  return buffer_index;
}

bool FrameTracker::HasDeferredInputBuffers() const {
  std::lock_guard lock(mutex_);
  return !deferred_input_buffer_indices_.empty();
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
