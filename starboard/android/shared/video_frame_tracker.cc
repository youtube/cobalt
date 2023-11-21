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

#include "starboard/android/shared/video_frame_tracker.h"

#include "starboard/common/log.h"
#include "starboard/time.h"

namespace starboard {
namespace android {
namespace shared {

void VideoFrameTracker::OnInputBufferEnqueued(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(input_buffer);
  SB_DCHECK(media_time_provider_);

  if (input_buffer->timestamp() < seek_to_time_) {
    return;
  }

  bool is_playing = true;
  bool is_eos_played = true;
  bool is_underflow = true;
  double playback_rate = -1.0;
  SbTime media_time = media_time_provider_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);

  if (input_buffer->timestamp() < media_time) {
    dropped_frames_++;
  }
}

void VideoFrameTracker::Seek(SbTime seek_to_time) {
  seek_to_time_ = seek_to_time;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
