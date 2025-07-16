#include "starboard/shared/starboard/media/memory_tracker.h"

#include "starboard/common/log.h"

namespace starboard::shared::starboard::media {

MemoryTracker::MemoryTracker(GetCurrentTimeUsCallback get_current_time_us_cb)
    : get_current_time_us_cb_(get_current_time_us_cb),
      frame_expirations_(kMaxFrames, kUninitialized) {
  SB_DCHECK(get_current_time_us_cb_);
}

bool MemoryTracker::AddNewFrame() {
  CleanUpExpiredFrames();
  for (auto& frame_expiration : frame_expirations_) {
    if (frame_expiration == kUninitialized) {
      frame_expiration = kNoExpirationSet;
      return true;
    }
  }
  return false;
}

bool MemoryTracker::SetFrameExpiration(int64_t expired_at_us) {
  CleanUpExpiredFrames();
  for (auto& frame_expiration : frame_expirations_) {
    if (frame_expiration == kNoExpirationSet) {
      frame_expiration = expired_at_us;
      return true;
    }
  }
  return false;
}

int MemoryTracker::GetCurrentFrames() const {
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

void MemoryTracker::CleanUpExpiredFrames() {
  int64_t now_us = get_current_time_us_cb_();
  for (auto& frame_expiration : frame_expirations_) {
    if (frame_expiration != kUninitialized &&
        frame_expiration != kNoExpirationSet && frame_expiration <= now_us) {
      frame_expiration = kUninitialized;
    }
  }
}

}  // namespace starboard::shared::starboard::media
