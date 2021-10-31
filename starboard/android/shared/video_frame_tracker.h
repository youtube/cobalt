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

#include <list>
#include <vector>

#include "starboard/common/mutex.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/time.h"

namespace starboard {
namespace android {
namespace shared {

class VideoFrameTracker {
 public:
  explicit VideoFrameTracker(int max_pending_frames_size)
      : max_pending_frames_size_(max_pending_frames_size) {}

  SbTime seek_to_time() const;

  void OnInputBuffer(SbTime timestamp);

  void OnFrameRendered(int64_t frame_timestamp);

  void Seek(SbTime seek_to_time);

  int UpdateAndGetDroppedFrames();

 private:
  void UpdateDroppedFrames();

  ::starboard::shared::starboard::ThreadChecker thread_checker_;

  std::list<SbTime> frames_to_be_rendered_;

  const int max_pending_frames_size_;
  int dropped_frames_ = 0;
  SbTime seek_to_time_ = 0;

  Mutex rendered_frames_mutex_;
  std::vector<SbTime> rendered_frames_on_tracker_thread_;
  std::vector<SbTime> rendered_frames_on_decoder_thread_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_FRAME_TRACKER_H_
