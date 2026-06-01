// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_FRAME_TRACKER_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_FRAME_TRACKER_H_

#include <atomic>
#include <mutex>
#include <vector>

namespace starboard {

class VideoFrameTracker {
 public:
  explicit VideoFrameTracker(int max_tracked_frames);

  int64_t seek_to_time() const;

  void OnInputBuffer(int64_t timestamp);

  void OnFrameRendered(int64_t frame_timestamp);

  void Seek(int64_t seek_to_time);

  int UpdateAndGetDroppedFrames();

 private:
  void UpdateDroppedFrames_Locked();  // Requires |state_mutex_|.

  std::mutex state_mutex_;
  // NOTE: std::vector is used to avoid heap allocations during playback.
  std::vector<int64_t> frames_to_be_rendered_;  // Guarded by |state_mutex_|.

  const int max_tracked_frames_;
  int dropped_frames_ = 0;                // Guarded by |state_mutex_|.
  std::atomic<int64_t> seek_to_time_{0};  // microseconds

  std::vector<int64_t>
      rendered_frames_on_tracker_thread_;  // Guarded by |state_mutex_|
                                           // (microseconds).

  std::mutex rendered_frames_mutex_;
  std::vector<int64_t>
      rendered_frames_on_decoder_thread_;  // Guarded by
                                           // |rendered_frames_mutex_|
                                           // (microseconds).
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_FRAME_TRACKER_H_
