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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_FRAME_RATE_ESTIMATOR_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_FRAME_RATE_ESTIMATOR_H_

#include <list>

#include "starboard/common/optional.h"
#include "starboard/common/ref_counted.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// Use the timestamps of a series of video frames to estimate the frame rate of
// the video.
class VideoFrameRateEstimator {
 public:
  typedef std::list<scoped_refptr<VideoFrame>> Frames;

  static constexpr double kInvalidFrameRate = -1.;

  VideoFrameRateEstimator();

  // Called to update the frame rate.  Note that for the initial calculation to
  // work, it requires at least two video frames, which should be true under
  // most preroll conditions.  The caller should call this function every time a
  // frame is removed from |frames|.
  // This function can be called repeatedly and it will refine the frame rate
  // calculated.
  void Update(const Frames& frames);

  void Reset();
  double frame_rate() const { return frame_rate_; }

 private:
  // If the ratio of two frame estimated rates is not in the range
  // (1 - |kFrameDurationRatioEpsilon|, 1 + |kFrameDurationEpsilon|), then the
  // transition is treated like a discontinuity.
  static constexpr double kFrameDurationRatioEpsilon = 0.1;
  // If the difference between the calculated frame rate and the nearest integer
  // frame rate is less than the following value, the integer frame rate will be
  // used.
  static constexpr double kFrameRateEpsilon = 0.1;

  void CalculateInitialFrameRate(const Frames& frames,
                                 SbTime previous_frame_duration = 0);
  void RefineFrameRate(const Frames& frames);

  double frame_rate_ = kInvalidFrameRate;
  SbTime accumulated_frame_durations_;
  int number_of_frame_durations_accumulated_;
  SbTime last_checked_frame_timestamp_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_FRAME_RATE_ESTIMATOR_H_
