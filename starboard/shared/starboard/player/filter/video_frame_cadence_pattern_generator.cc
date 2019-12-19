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

#include "starboard/shared/starboard/player/filter/video_frame_cadence_pattern_generator.h"

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

void VideoFrameCadencePatternGenerator::UpdateRefreshRateAndMaybeReset(
    double refresh_rate) {
  SB_DCHECK(refresh_rate > 0);

  if (refresh_rate == refresh_rate_) {
    return;
  }

  SB_LOG(WARNING) << "Refresh rate updated from " << refresh_rate_ << " to "
                  << refresh_rate;

  refresh_rate_ = refresh_rate;
  frame_index_ = 0;
}

void VideoFrameCadencePatternGenerator::UpdateFrameRate(double frame_rate) {
  SB_DCHECK(frame_rate > 0);

  frame_rate_ = frame_rate;
}

int VideoFrameCadencePatternGenerator::GetNumberOfTimesCurrentFrameDisplays()
    const {
  SB_DCHECK(refresh_rate_ != kInvalidRefreshRate);
  SB_DCHECK(frame_rate_ != kInvalidFrameRate);

  int current_frame_display_times = 0;

  auto current_frame_time = frame_index_ / frame_rate_;
  auto next_frame_time = (frame_index_ + 1) / frame_rate_;

  int refresh_ticks =
      static_cast<int>(current_frame_time - 1 / refresh_rate_) * refresh_rate_;

  // The following loop should be able to terminate by itself without any
  // constraints on the maximum iterations it can run.  However, set a max
  // number of iteration just in case, to avoid it loops infinitely.
  const int kMaxFrameDisplayTimes = 120;
  for (int i = 0; i < kMaxFrameDisplayTimes; ++i) {
    double refresh_time = refresh_ticks / refresh_rate_;
    if (refresh_time >= next_frame_time) {
      break;
    }
    if (refresh_time >= current_frame_time) {
      ++current_frame_display_times;
    }
    ++refresh_ticks;
  }

  return current_frame_display_times;
}

void VideoFrameCadencePatternGenerator::AdvanceToNextFrame() {
  // It is possible that AdvanceToNextFrame() is called before refresh rate and
  // frame rate are set.  This can happen when the platform release the frame on
  // rendering.
  ++frame_index_;
}

void VideoFrameCadencePatternGenerator::Reset(double refresh_rate) {
  SB_DCHECK(refresh_rate > 0);

  refresh_rate_ = refresh_rate;
  frame_rate_ = kInvalidFrameRate;
  frame_index_ = 0;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
