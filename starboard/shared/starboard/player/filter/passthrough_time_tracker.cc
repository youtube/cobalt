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

#include "starboard/shared/starboard/player/filter/passthrough_time_tracker.h"

#include <algorithm>

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

void PassthroughTimeTracker::Reset(int64_t seek_to_time) {
  std::lock_guard lock(mutex_);
  buffer_queue_ = std::queue<BufferInfo>();
  seek_to_time_ = seek_to_time;
}

void PassthroughTimeTracker::OnBufferWritten(
    int64_t hardware_sample_offset,
    int64_t pts,
    int64_t duration_usec,
    int64_t discarded_duration_from_front_usec,
    int64_t discarded_duration_from_back_usec) {
  if (duration_usec <= 0) {
    SB_LOG(WARNING) << "PassthroughTimeTracker ignored buffer with duration "
                    << duration_usec;
    return;
  }

  std::lock_guard lock(mutex_);
  // Estimate sample count from duration if needed, or caller passes exact
  // offset. We assume duration_usec is proportional to samples written for this
  // buffer.
  buffer_queue_.push({
      hardware_sample_offset,
      /* hardware_sample_end = */ hardware_sample_offset +
          (duration_usec * 48'000LL /
           1'000'000LL),  // Will be adjusted by actual sample rate in
                          // GetCurrentMediaTime if exact end needed, but we use
                          // start + samples
      pts,
      duration_usec,
      std::max<int64_t>(0, discarded_duration_from_front_usec),
      std::max<int64_t>(0, discarded_duration_from_back_usec),
  });
}

int64_t PassthroughTimeTracker::GetCurrentMediaTime(
    int64_t playback_head_sample_position,
    int sample_rate,
    int64_t fallback_start_time) const {
  std::lock_guard lock(mutex_);

  if (sample_rate <= 0) {
    return buffer_queue_.empty() ? fallback_start_time
                                 : buffer_queue_.front().pts;
  }

  // Evict any buffers from the front that the hardware playhead has completely
  // finished playing out of the speaker.
  while (!buffer_queue_.empty()) {
    const BufferInfo& front = buffer_queue_.front();
    int64_t buffer_samples = front.duration_usec * sample_rate / 1'000'000LL;
    if (buffer_samples <= 0) {
      buffer_queue_.pop();
      continue;
    }
    if (playback_head_sample_position >=
        front.hardware_sample_start + buffer_samples) {
      buffer_queue_.pop();
    } else {
      break;
    }
  }

  if (buffer_queue_.empty()) {
    return fallback_start_time;
  }

  const BufferInfo& active = buffer_queue_.front();
  int64_t samples_into_buffer = std::max<int64_t>(
      0, playback_head_sample_position - active.hardware_sample_start);
  int64_t time_into_buffer = samples_into_buffer * 1'000'000LL / sample_rate;

  // Handle front discard: while the trimmed preamble plays out of the speaker,
  // clamp media time at the buffer's PTS.
  if (time_into_buffer < active.discard_front_usec) {
    return active.pts;
  }
  time_into_buffer -= active.discard_front_usec;

  // Handle back discard: do not let media time overshoot the valid buffer
  // duration.
  int64_t valid_duration = active.duration_usec - active.discard_front_usec -
                           active.discard_back_usec;
  if (valid_duration < 0) {
    valid_duration = 0;
  }
  if (time_into_buffer > valid_duration) {
    time_into_buffer = valid_duration;
  }

  return active.pts + time_into_buffer;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
