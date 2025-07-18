#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_FRAME_TRACKER_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_FRAME_TRACKER_H_

#include <iosfwd>
#include <mutex>
#include <optional>
#include <queue>

#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard::shared::starboard::media {

class FrameTracker {
 public:
  struct State {
    int decoding_frames;
    int decoded_frames;
    int decoding_frames_high_water_mark;
    int decoded_frames_high_water_mark;
    int total_frames_high_water_mark;
  };

  FrameTracker(int max_frames, int64_t log_interval_us);
  ~FrameTracker();

  bool AddFrame();
  bool SetFrameDecoded();
  bool ReleaseFrame();
  bool ReleaseFrameAt(int64_t release_us);
  void Reset();

  State GetCurrentState();
  bool IsFull();

 private:
  std::pair<int, int> UpdateHighWaterMarks_Locked();
  void PurgeReleasedFrames_Locked();
  void LogStateAndReschedule(int64_t log_interval_us);

  std::vector<int64_t> frames_;
  mutable int decoding_frames_high_water_mark_ = 0;
  mutable int decoded_frames_high_water_mark_ = 0;
  mutable int total_frames_high_water_mark_ = 0;

  std::queue<int> deferred_input_buffer_indices_;
  mutable std::mutex mutex_;

  ::starboard::shared::starboard::player::JobThread task_runner_;
};

std::ostream& operator<<(std::ostream& os, const FrameTracker::State& status);

}  // namespace starboard::shared::starboard::media

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_FRAME_TRACKER_H_
