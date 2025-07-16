#include "starboard/shared/starboard/media/memory_tracker.h"

#include <mutex>

#include "starboard/common/log.h"

namespace starboard::shared::starboard::media {
namespace {
constexpr int64_t kUninitialized = -1;
constexpr int64_t kNoExpirationSet = 0;
}  // namespace

MemoryTracker::MemoryTracker(GetCurrentTimeUsCallback get_current_time_us_cb,
                             int max_frames)
    : get_current_time_us_cb_(get_current_time_us_cb),
      max_frames_(max_frames),
      frame_expirations_(max_frames, kUninitialized) {
  SB_DCHECK(get_current_time_us_cb_);
  SB_DCHECK(max_frames > 0);
}

bool MemoryTracker::AddNewFrame() {
  std::lock_guard<std::mutex> lock(mutex_);
  CleanUpExpiredFrames_Locked();
  for (auto& frame_expiration : frame_expirations_) {
    if (frame_expiration == kUninitialized) {
      frame_expiration = kNoExpirationSet;
      return true;
    }
  }
  return false;
}

bool MemoryTracker::SetFrameExpiration(int64_t expired_at_us) {
  std::lock_guard<std::mutex> lock(mutex_);
  CleanUpExpiredFrames_Locked();
  for (auto& frame_expiration : frame_expirations_) {
    if (frame_expiration == kNoExpirationSet) {
      frame_expiration = expired_at_us;
      return true;
    }
  }
  return false;
}

bool MemoryTracker::SetFrameExpirationNow() {
  return SetFrameExpiration(get_current_time_us_cb_());
}

int MemoryTracker::GetCurrentFrames() const {
  std::lock_guard<std::mutex> lock(mutex_);
  int count = 0;
  int64_t now_us = get_current_time_us_cb_();
  for (const auto& frame_expiration : frame_expirations_) {
    if (frame_expiration == kUninitialized) {
      continue;
    }
    if (frame_expiration != kNoExpirationSet && frame_expiration <= now_us) {
      continue;
    }
    count++;
  }
  return count;
}

void MemoryTracker::CleanUpExpiredFrames_Locked() {
  int64_t now_us = get_current_time_us_cb_();
  for (auto& frame_expiration : frame_expirations_) {
    if (frame_expiration != kUninitialized &&
        frame_expiration != kNoExpirationSet && frame_expiration <= now_us) {
      frame_expiration = kUninitialized;
    }
  }
}

}  // namespace starboard::shared::starboard::media
