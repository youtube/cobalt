// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/audio_frame_tracker.h"

#include <queue>

#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

AudioFrameTracker::Impl::Impl() : frames_played_adjusted_to_playback_rate_(0) {}

void AudioFrameTracker::Impl::Reset() {
  while (!frame_records_.empty()) {
    frame_records_.pop();
  }
  frames_played_adjusted_to_playback_rate_ = 0;
}

void AudioFrameTracker::Impl::AddFrames(int number_of_frames,
                                        double playback_rate) {
  SB_DCHECK(playback_rate > 0);

  if (frame_records_.empty() ||
      frame_records_.back().playback_rate != playback_rate) {
    FrameRecord record = {number_of_frames, playback_rate};
    frame_records_.push(record);
  } else {
    frame_records_.back().number_of_frames += number_of_frames;
  }
}

void AudioFrameTracker::Impl::RecordPlayedFrames(int number_of_frames) {
  while (number_of_frames > 0 && !frame_records_.empty()) {
    FrameRecord& record = frame_records_.front();
    if (record.number_of_frames > number_of_frames) {
      frames_played_adjusted_to_playback_rate_ +=
          static_cast<int>(number_of_frames * record.playback_rate);
      record.number_of_frames -= number_of_frames;
      number_of_frames = 0;
    } else {
      number_of_frames -= record.number_of_frames;
      frames_played_adjusted_to_playback_rate_ +=
          static_cast<int>(record.number_of_frames * record.playback_rate);
      frame_records_.pop();
    }
  }
}

int64_t AudioFrameTracker::Impl::GetFramesPlayedAdjustedToPlaybackRate() const {
  return frames_played_adjusted_to_playback_rate_;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
