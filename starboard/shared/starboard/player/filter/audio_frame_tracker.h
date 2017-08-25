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

#include "starboard/common/scoped_ptr.h"
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
  virtual ~AudioFrameTracker() {}

  // Reset the class to its initial state.  In this state there is no frames
  // tracked and the playback frames is 0.
  virtual void Reset() {
    while (!frame_records_.empty()) {
      frame_records_.pop();
    }
    frames_played_adjusted_to_playback_rate_ = 0;
  }

  virtual void AddFrames(int number_of_frames, double playback_rate) {
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

  virtual void RecordPlayedFrames(int number_of_frames) {
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
    SB_DCHECK(number_of_frames == 0)
        << number_of_frames << " " << frame_records_.size();
  }

  virtual SbMediaTime GetFramesPlayedAdjustedToPlaybackRate() const {
    return frames_played_adjusted_to_playback_rate_;
  }

 protected:
  struct FrameRecord {
    int number_of_frames;
    double playback_rate;
  };

  std::queue<FrameRecord> frame_records_;
  int frames_played_adjusted_to_playback_rate_;
};

// A specialization of |AudioFrameTracker| that can only be called from the
// thread that it was created on, and will assert this in debug builds.
class SingleThreadedAudioFrameTracker : public AudioFrameTracker {
 public:
  // Reset the class to its initial state.  In this state there is no frames
  // tracked and the playback frames is 0.
  void Reset() SB_OVERRIDE {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    AudioFrameTracker::Reset();
  }

  void AddFrames(int number_of_frames, double playback_rate) SB_OVERRIDE {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    AudioFrameTracker::AddFrames(number_of_frames, playback_rate);
  }

  void RecordPlayedFrames(int number_of_frames) SB_OVERRIDE {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    AudioFrameTracker::RecordPlayedFrames(number_of_frames);
  }

  SbMediaTime GetFramesPlayedAdjustedToPlaybackRate() const SB_OVERRIDE {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    return AudioFrameTracker::GetFramesPlayedAdjustedToPlaybackRate();
  }

 private:
  ThreadChecker thread_checker_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_FRAME_TRACKER_H_
