#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_FRAME_TRACKER_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_FRAME_TRACKER_H_

#include <functional>
#include <iosfwd>
#include <mutex>
#include <optional>
#include <queue>

#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard::shared::starboard::media {

class FrameTracker {
 public:
  using FrameReleasedCB = std::function<void()>;

  struct State {
    int decoding_frames;
    int decoded_frames;
    int decoding_frames_high_water_mark;
    int decoded_frames_high_water_mark;
    int total_frames_high_water_mark;
    int64_t min_decoding_time_us;
    int64_t max_decoding_time_us;
    int64_t avg_decoding_time_us;
  };

  FrameTracker(int max_frames,
               int64_t log_interval_us,
               FrameReleasedCB frame_released_cb);
  ~FrameTracker();

  bool AddFrame();
  bool SetFrameDecoded();
  bool ReleaseFrame();
  bool ReleaseFrameAt(int64_t release_us);

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

  std::deque<int64_t> decoding_start_times_us_;
  std::deque<int64_t> previous_decoding_times_us_;

  const FrameReleasedCB frame_released_cb_;

  ::starboard::shared::starboard::player::JobThread task_runner_;
};

std::ostream& operator<<(std::ostream& os, const FrameTracker::State& status);

}  // namespace starboard::shared::starboard::media

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_FRAME_TRACKER_H_
