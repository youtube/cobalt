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

#include "starboard/shared/starboard/player/filter/video_frame_rate_estimator.h"

#include <cmath>

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

VideoFrameRateEstimator::VideoFrameRateEstimator() {
  Reset();
}

void VideoFrameRateEstimator::Update(const Frames& frames) {
  if (frame_rate_ == kInvalidFrameRate) {
    if (frames.size() < 2) {
      return;
    }
    if (frames.back()->is_end_of_stream() && frames.size() < 3) {
      return;
    }
  }

  if (frame_rate_ == kInvalidFrameRate) {
    CalculateInitialFrameRate(frames);
    return;
  }

  if (!frames.empty()) {
    RefineFrameRate(frames);
  }
}

void VideoFrameRateEstimator::Reset() {
  frame_rate_ = kInvalidFrameRate;
}

void VideoFrameRateEstimator::CalculateInitialFrameRate(
    const Frames& frames,
    SbTime previous_frame_duration) {
  SB_DCHECK(frame_rate_ == kInvalidFrameRate);
  SB_DCHECK(!frames.empty());
  SB_DCHECK(frames.size() >= 2 || previous_frame_duration > 0);

  if (previous_frame_duration > 0) {
    accumulated_frame_durations_ = previous_frame_duration;
    number_of_frame_durations_accumulated_ = 1;
    last_checked_frame_timestamp_ = frames.front()->timestamp();
  } else {
    number_of_frame_durations_accumulated_ = 0;
  }

  for (auto current = frames.begin(); current != frames.end(); ++current) {
    auto next = current;
    ++next;
    if (next == frames.end() || (*next)->is_end_of_stream()) {
      break;
    }

    auto current_frame_duration =
        (*next)->timestamp() - (*current)->timestamp();
    SB_DCHECK(current_frame_duration > 0);

    if (number_of_frame_durations_accumulated_ == 0) {
      accumulated_frame_durations_ = current_frame_duration;
      number_of_frame_durations_accumulated_ = 1;
      last_checked_frame_timestamp_ = (*next)->timestamp();
      continue;
    }

    auto average_frame_duration =
        accumulated_frame_durations_ / number_of_frame_durations_accumulated_;
    auto ratio =
        static_cast<double>(current_frame_duration) / average_frame_duration;
    if (std::fabs(1.0 - ratio) > kFrameDurationRatioEpsilon) {
      // We've encountered discontinuity, which is theoretically possible but
      // should never happen.  We handle this just in case.
      SB_LOG(WARNING) << "Frame rate discontinuity detected, "
                      << "current frame duration: " << current_frame_duration
                      << ", average frame duration: " << average_frame_duration;
      break;
    }
    ++number_of_frame_durations_accumulated_;
    accumulated_frame_durations_ += current_frame_duration;
  }
  auto average_frame_duration =
      accumulated_frame_durations_ / number_of_frame_durations_accumulated_;
  frame_rate_ = static_cast<double>(kSbTimeSecond) / average_frame_duration;

  // Snap the frame rate to the nearest integer, so 29.97 will become 30.
  if (frame_rate_ - std::floor(frame_rate_) < kFrameRateEpsilon) {
    frame_rate_ = std::floor(frame_rate_);
  } else if (std::ceil(frame_rate_) - frame_rate_ < kFrameRateEpsilon) {
    frame_rate_ = std::ceil(frame_rate_);
  }
}

void VideoFrameRateEstimator::RefineFrameRate(const Frames& frames) {
  SB_DCHECK(frame_rate_ != kInvalidFrameRate);
  SB_DCHECK(!frames.empty());

  if (frames.front()->is_end_of_stream()) {
    return;
  }
  auto current_timestamp = frames.front()->timestamp();
  if (current_timestamp <= last_checked_frame_timestamp_) {
    return;
  }

  auto last_frame_duration = current_timestamp - last_checked_frame_timestamp_;
  auto average_frame_duration =
      accumulated_frame_durations_ / number_of_frame_durations_accumulated_;
  auto ratio =
      static_cast<double>(last_frame_duration) / average_frame_duration;
  if (std::fabs(1.0 - ratio) <= kFrameDurationRatioEpsilon) {
    last_checked_frame_timestamp_ = current_timestamp;
    ++number_of_frame_durations_accumulated_;
    accumulated_frame_durations_ += last_frame_duration;
    return;
  }
  // We've encountered discontinuity, which is theoretically possible but should
  // never happen.  We handle this by recalculate the frame rate from scratch.
  SB_LOG(WARNING) << "Frame rate discontinuity detected, "
                  << "current frame duration: " << last_frame_duration
                  << ", average frame duration: " << average_frame_duration;
  Reset();
  CalculateInitialFrameRate(frames, last_frame_duration);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
