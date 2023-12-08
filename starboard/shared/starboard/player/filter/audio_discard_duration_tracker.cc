// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/audio_discard_duration_tracker.h"

#include <algorithm>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
using shared::starboard::media::AudioSampleInfo;

void AudioDiscardDurationTracker::CacheDiscardDuration(
    const scoped_refptr<InputBuffer>& input_buffer,
    SbTime buffer_duration) {
  SbTime discard_duration_from_front =
      input_buffer->audio_sample_info().discarded_duration_from_front;
  SbTime discard_duration_from_back =
      input_buffer->audio_sample_info().discarded_duration_from_back;

  ScopedLock lock(mutex_);
  if (discard_duration_from_front + discard_duration_from_back >=
      buffer_duration) {
    discard_infos_.push(
        AudioDiscardInfo{buffer_duration, input_buffer->timestamp(),
                         input_buffer->timestamp() + buffer_duration});
  } else {
    if (discard_duration_from_front > 0) {
      discard_infos_.push(AudioDiscardInfo{
          discard_duration_from_front, input_buffer->timestamp(),
          input_buffer->timestamp() + discard_duration_from_front});
    }
    if (discard_duration_from_back > 0) {
      discard_infos_.push(
          AudioDiscardInfo{discard_duration_from_back,
                           input_buffer->timestamp() + buffer_duration -
                               discard_duration_from_back,
                           input_buffer->timestamp() + buffer_duration});
    }
  }
}

void AudioDiscardDurationTracker::CacheMultipleDiscardDurations(
    const InputBuffers& input_buffers,
    SbTime buffer_duration) {
  for (const auto& input_buffer : input_buffers) {
    CacheDiscardDuration(input_buffer, buffer_duration);
  }
}

SbTime AudioDiscardDurationTracker::AdjustTimeForTotalDiscardDuration(
    SbTime timestamp) {
  SB_LOG_IF(WARNING, last_received_timestamp_ > timestamp)
      << "Last received timestamp " << last_received_timestamp_
      << " is greater than timestamp " << timestamp
      << ". AudioDiscardDurationTracker::AdjustTimeForTotalDiscardDuration() "
         "requires monotonically increasing timestamps.";
  last_received_timestamp_ = timestamp;

  ScopedLock lock(mutex_);
  // As a lot of time may have passed since the last call to
  // AdjustTimeForTotalDiscardDuration(), remove all AudioDiscardInfos that
  // have already been passed by the |timestamp|.
  while (discard_infos_.size() > 0 &&
         timestamp >= discard_infos_.front().discard_end_timestamp) {
    total_discard_duration_ += discard_infos_.front().discard_duration;
    discard_infos_.pop();
  }

  if (discard_infos_.size() > 0) {
    SbTime discard_start_timestamp =
        discard_infos_.front().discard_start_timestamp;
    SbTime discard_end_timestamp = discard_infos_.front().discard_end_timestamp;
    if (timestamp >= discard_start_timestamp &&
        timestamp < discard_end_timestamp) {
      // "Freeze" the timestamp at |discard_start_timestamp| if |timestamp|
      // falls within the discard period.
      return std::max(SbTime(0),
                      discard_start_timestamp - total_discard_duration_);
    }
  }

  return std::max(SbTime(0), timestamp - total_discard_duration_);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
