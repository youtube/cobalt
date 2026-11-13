// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/aaudio_presented_frame_tracker.h"

#include <algorithm>

#include "starboard/common/check_op.h"

namespace starboard {

AaudioPresentedFrameTracker::AaudioPresentedFrameTracker(int sample_rate)
    : sample_rate_(sample_rate) {
  SB_CHECK_GT(sample_rate_, 0);
}

void AaudioPresentedFrameTracker::Reset() {
  segments_.clear();
  frames_in_flight_ = 0;
}

void AaudioPresentedFrameTracker::OnFramesCopied(int64_t stream_position,
                                                 int frames) {
  if (frames <= 0) {
    return;
  }
  if (!segments_.empty()) {
    const Segment& last = segments_.back();
    SB_DCHECK_GE(stream_position, last.start + last.frames);
    if (last.start + last.frames == stream_position) {
      segments_.back().frames += frames;
      frames_in_flight_ += frames;
      return;
    }
  }
  segments_.push_back({stream_position, frames});
  frames_in_flight_ += frames;
}

int AaudioPresentedFrameTracker::OnTimestamp(int64_t timestamp_position,
                                             int64_t timestamp_us,
                                             int64_t now_us,
                                             int64_t written_position) {
  const int64_t timestamp_age_us = std::max<int64_t>(0, now_us - timestamp_us);
  const int64_t presented_position = std::min<int64_t>(
      written_position,
      timestamp_position + timestamp_age_us * sample_rate_ / 1'000'000);

  int presented = 0;
  while (!segments_.empty()) {
    Segment& front = segments_.front();
    if (front.start >= presented_position) {
      break;
    }
    const int frames = static_cast<int>(
        std::min<int64_t>(front.frames, presented_position - front.start));
    presented += frames;
    front.start += frames;
    front.frames -= frames;
    if (front.frames > 0) {
      break;
    }
    segments_.pop_front();
  }
  frames_in_flight_ -= presented;
  return presented;
}

}  // namespace starboard
