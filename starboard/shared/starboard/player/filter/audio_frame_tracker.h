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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_FRAME_TRACKER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_FRAME_TRACKER_H_

#include <queue>

#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// This class helps on tracking how many audio frames have been played with
// playback rate taking into account.
//
// For example, when playback rate is set to 2.0 and 20 frames have been played,
// the adjusted played frame will be 40.
class AudioFrameTracker {
 public:
  AudioFrameTracker() { Reset(); }

  // Reset the class to its initial state.  In this state there is no frames
  // tracked and the playback frames is 0.
  void Reset() {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    while (!frame_records_.empty()) {
      frame_records_.pop();
    }
    frames_played_adjusted_to_playback_rate_ = 0;
  }

  void AddFrames(int number_of_frames, double playback_rate) {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    if (number_of_frames == 0) {
      return;
    }
    SB_DCHECK(playback_rate > 0);

    if (frame_records_.empty() ||
        frame_records_.back().playback_rate != playback_rate) {
      FrameRecord record = {number_of_frames, playback_rate};
      frame_records_.push(record);
    } else {
      frame_records_.back().number_of_frames += number_of_frames;
    }
  }

  void RecordPlayedFrames(int number_of_frames) {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    while (number_of_frames > 0 && !frame_records_.empty()) {
      FrameRecord& record = frame_records_.front();
      if (record.number_of_frames > number_of_frames) {
        frames_played_adjusted_to_playback_rate_ +=
            number_of_frames * record.playback_rate;
        record.number_of_frames -= number_of_frames;
        number_of_frames = 0;
      } else {
        number_of_frames -= record.number_of_frames;
        frames_played_adjusted_to_playback_rate_ +=
            record.number_of_frames * record.playback_rate;
        frame_records_.pop();
      }
    }
    SB_DCHECK(number_of_frames == 0)
        << number_of_frames << " " << frame_records_.size();
  }

  SbMediaTime GetFramePlayedAdjustedToPlaybackRate() const {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    return frames_played_adjusted_to_playback_rate_;
  }

 private:
  struct FrameRecord {
    int number_of_frames;
    double playback_rate;
  };

  ThreadChecker thread_checker_;
  std::queue<FrameRecord> frame_records_;
  int frames_played_adjusted_to_playback_rate_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_FRAME_TRACKER_H_
