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

#include <vector>

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
  // Reset the class to its initial state.  In this state there is no frames
  // tracked and the playback frames is 0.
  void Reset();
  void AddFrames(int number_of_frames, double playback_rate);
  void RecordPlayedFrames(int number_of_frames);
  int64_t GetFutureFramesPlayedAdjustedToPlaybackRate(
      int number_of_frames) const;

 private:
  struct FrameRecord {
    int number_of_frames;
    double playback_rate;
  };

  // Usually there are very few elements, so std::vector<> is efficient enough.
  std::vector<FrameRecord> frame_records_;
  int64_t frames_played_adjusted_to_playback_rate_ = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_FRAME_TRACKER_H_
