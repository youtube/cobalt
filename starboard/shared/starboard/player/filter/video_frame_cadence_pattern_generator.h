// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_FRAME_CADENCE_PATTERN_GENERATOR_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_FRAME_CADENCE_PATTERN_GENERATOR_H_

#include "starboard/shared/internal_only.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// Generate the cadence pattern according to the graphics refresh rate and the
// video frame rate.  For example, for 30 fps video on 60 fps graphics, each
// frame should be rendered across 2 graphic updates (vsyncs).
class VideoFrameCadencePatternGenerator {
 public:
  // Update the refresh rate.  Note that the refresh rate should be changed
  // rarely, and if this is ever changed, the existing cadence will be reset.
  void UpdateRefreshRateAndMaybeReset(double refresh_rate);
  // Update the frame rate.  This can be called multiple times as the frame rate
  // can be refined during video playback.  The existing cadence won't be reset.
  void UpdateFrameRate(double frame_rate);

  bool has_cadence() const {
    return refresh_rate_ != kInvalidRefreshRate &&
           frame_rate_ != kInvalidFrameRate;
  }

  // Get the number of times current frame is going to be displayed under the
  // current refresh rate and video frame rate.  For example, the first frame
  // of a 24 fps video should be displayed 3 times if the refresh rate is 60.
  int GetNumberOfTimesCurrentFrameDisplays() const;
  void AdvanceToNextFrame();

  void Reset(double refresh_rate);

 private:
  static constexpr double kInvalidRefreshRate = -1.;
  static constexpr double kInvalidFrameRate = -1.;

  double refresh_rate_ = kInvalidRefreshRate;
  double frame_rate_ = kInvalidFrameRate;

  int64_t frame_index_ = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_FRAME_CADENCE_PATTERN_GENERATOR_H_
