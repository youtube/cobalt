#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_FRAME_TRACKER_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_FRAME_TRACKER_H_

#include <iosfwd>
#include <mutex>
#include <optional>
#include <queue>

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

  explicit FrameTracker(int max_frames);

  void AddFrame();
  void SetFrameDecoded();
  void ReleaseFrame();
  void ReleaseFrameAt(int64_t release_time);
  void Reset();

  State GetCurrentState() const;

  bool IsFull() const;
  void DeferInputBuffer(int buffer_index);
  std::optional<int> GetDeferredInputBuffer();

 private:
  void UpdateHighWaterMarks_Locked();

  const int max_frames_;
  int decoding_frames_ = 0;
  int decoded_frames_ = 0;
  int decoding_frames_high_water_mark_ = 0;
  int decoded_frames_high_water_mark_ = 0;
  int total_frames_high_water_mark_ = 0;
  std::queue<int> deferred_input_buffer_indices_;
  mutable std::mutex mutex_;
};

std::ostream& operator<<(std::ostream& os, const FrameTracker::State& status);

}  // namespace starboard::shared::starboard::media

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_FRAME_TRACKER_H_
