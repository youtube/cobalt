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
    int decoding_frames = 0;
    int decoded_frames = 0;
    int decoding_frames_high_water_mark = 0;
    int decoded_frames_high_water_mark = 0;
    int total_frames_high_water_mark = 0;
    int64_t min_decoding_time_us = 0;
    int64_t max_decoding_time_us = 0;
    int64_t avg_decoding_time_us = 0;

    int total_frames() const { return decoding_frames + decoded_frames; }
  };

  FrameTracker(int max_frames,
               int64_t log_interval_us,
               FrameReleasedCB frame_released_cb);
  ~FrameTracker();

  bool AddFrame();
  bool SetFrameDecoded();
  bool ReleaseFrame();
  bool ReleaseFrameAt(int64_t release_us);

  State GetCurrentState() const;
  bool IsFull();

 private:
  void UpdateDecodingStat_Locked();
  void UpdateState_Locked();
  void LogStateAndReschedule(int64_t log_interval_us);

  const int max_frames_;
  const FrameReleasedCB frame_released_cb_;

  mutable std::mutex mutex_;
  State state_;  // GUARDED_BY mutex_;

  std::deque<int64_t> decoding_start_times_us_;
  std::deque<int64_t> previous_decoding_times_us_;

  ::starboard::shared::starboard::player::JobThread task_runner_;
};

std::ostream& operator<<(std::ostream& os, const FrameTracker::State& status);

}  // namespace starboard::shared::starboard::media

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_FRAME_TRACKER_H_
