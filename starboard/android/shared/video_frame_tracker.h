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

#include "starboard/common/ref_counted.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace android {
namespace shared {

class VideoFrameTracker {
 public:
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;
  typedef ::starboard::shared::starboard::player::filter::MediaTimeProvider
      MediaTimeProvider;

  SbTime seek_to_time() const { return seek_to_time_; }

  void OnInputBufferEnqueued(const scoped_refptr<InputBuffer>& input_buffer);

  void Seek(SbTime seek_to_time);

  int GetDroppedFrames() const { return dropped_frames_; }

  void SetMediaTimeProvider(MediaTimeProvider* media_time_provider) {
    media_time_provider_ = media_time_provider;
  }

 private:
  MediaTimeProvider* media_time_provider_ = nullptr;
  std::atomic_int dropped_frames_ = {0};
  std::atomic_int64_t seek_to_time_ = {0};
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_FRAME_TRACKER_H_
