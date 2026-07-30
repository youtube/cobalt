// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {

// TODO: b/507074472 - add tests for AudioFrameTracker
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

  int64_t GetOverflowedFrames() const;
  int64_t GetTotalOriginalFrames() const;

  // Calculates the adjusted frame position after |number_of_frames| additional
  // frames have been played. |playback_rate| is set to the playback rate of
  // the last frame played.
  int64_t GetFutureFramesPlayedAdjustedToPlaybackRate(
      int number_of_frames,
      double* playback_rate) const;

  // Converts a position in the original media timeline (in frames) to the
  // adjusted timeline that accounts for playback rate changes.
  int64_t ConvertOriginalFramePositionToAdjustedFrames(
      int64_t frame_position) const;

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  void DumpDebugInfo() const;
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

 private:
  struct FrameRecord {
    int64_t number_of_frames;
    int64_t number_of_played_frames;
    double playback_rate;
  };

  // Usually there are very few elements, so std::vector<> is efficient enough.
  std::vector<FrameRecord> frame_records_;
  double last_playback_rate_ = 1.0;
  int64_t overflowed_frames_ = 0;

  // Cumulative positions in the original and adjusted timelines, used to
  // handle records that have been fully played and removed from
  // |frame_records_|.
  int64_t current_original_frame_head_position_ = 0;
  int64_t current_adjusted_frame_head_position_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_FRAME_TRACKER_H_
