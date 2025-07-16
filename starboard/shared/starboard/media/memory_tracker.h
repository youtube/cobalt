#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MEMORY_TRACKER_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MEMORY_TRACKER_H_

#include <functional>
#include <vector>

#include <mutex>

#include "starboard/common/time.h"
#include "starboard/shared/internal_only.h"

namespace starboard::shared::starboard::media {

// Tracks how many frames are alive in the memory.
class MemoryTracker {
 public:
  using GetCurrentTimeUsCallback = std::function<int64_t()>;

  explicit MemoryTracker(GetCurrentTimeUsCallback get_current_time_us_cb,
                         int max_frames = 6);
  ~MemoryTracker() = default;

  // Return true, when new frame is added in the tracker. False, it cannot
  // accept any more frames.
  bool AddNewFrame();

  // Sets the expiration time for one of the active frames.
  // Returns true on success, false if there are no active frames to set an
  // expiration for.
  bool SetFrameExpiration(int64_t expired_at_us);

  // Sets the expiration time for one of the active frames to the current time.
  bool SetFrameExpirationNow();

  // Returns current number of frames that are alive in the memory now.
  int GetCurrentFrames() const;

 private:
  void CleanUpExpiredFrames_Locked();

  mutable std::mutex mutex_;
  GetCurrentTimeUsCallback get_current_time_us_cb_;
  const int max_frames_;

  // Keeps track of the lifespan of the frame.
  std::vector<int64_t> frame_expirations_;
};

}  // namespace starboard::shared::starboard::media

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEMORY_TRACKER_H_
