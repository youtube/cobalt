// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PASSTHROUGH_TIME_TRACKER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PASSTHROUGH_TIME_TRACKER_H_

#include <mutex>
#include <queue>

#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// Cross-platform helper for tracking media time and handling partial frame
// discards (discarded_duration_from_front / discarded_duration_from_back) in
// passthrough audio renderers, where compressed syncframes cannot be sliced
// before sending to the audio sink.
class PassthroughTimeTracker {
 public:
  void Reset(int64_t seek_to_time = 0);

  // Called whenever the platform-specific renderer writes a syncframe to the
  // audio sink. |hardware_sample_offset| is the total number of hardware
  // samples sent to the sink before this buffer was written.
  void OnBufferWritten(int64_t hardware_sample_offset,
                       int64_t pts,
                       int64_t duration_usec,
                       int64_t discarded_duration_from_front_usec,
                       int64_t discarded_duration_from_back_usec);

  // Called by the platform renderer inside GetCurrentMediaTime().
  // Takes the monotonically increasing hardware sample playhead from the audio
  // sink and returns the exact adjusted media presentation time.
  int64_t GetCurrentMediaTime(int64_t playback_head_sample_position,
                              int sample_rate,
                              int64_t fallback_start_time) const;

 private:
  struct BufferInfo {
    int64_t hardware_sample_start;
    int64_t hardware_sample_end;
    int64_t pts;
    int64_t duration_usec;
    int64_t discard_front_usec;
    int64_t discard_back_usec;
  };

  mutable std::mutex mutex_;
  mutable std::queue<BufferInfo> buffer_queue_;
  int64_t seek_to_time_ = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PASSTHROUGH_TIME_TRACKER_H_
