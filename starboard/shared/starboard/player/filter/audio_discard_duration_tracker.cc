// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

void AudioDiscardDurationTracker::CacheMultipleDiscardDurations(
    const InputBuffers& input_buffers,
    int64_t buffer_duration_us) {
  ScopedLock lock(mutex_);
  for (const auto& input_buffer : input_buffers) {
    CacheDiscardDuration(input_buffer, buffer_duration_us);
  }
}

int64_t AudioDiscardDurationTracker::AdjustTimeForTotalDiscardDuration(
    int64_t timestamp_us) {
  ScopedLock lock(mutex_);
  SB_LOG_IF(WARNING, last_received_timestamp_us_ > timestamp_us)
      << "Last received timestamp " << last_received_timestamp_us_
      << " is greater than timestamp " << timestamp_us
      << ". AudioDiscardDurationTracker::AdjustTimeForTotalDiscardDuration() "
         "requires monotonically increasing timestamps.";
  last_received_timestamp_us_ = timestamp_us;

  // As a lot of time may have passed since the last call to
  // AdjustTimeForTotalDiscardDuration(), remove all AudioDiscardInfos that
  // have already been passed by the |timestamp|.
  while (discard_infos_.size() > 0 &&
         timestamp_us >= discard_infos_.front().discard_start_timestamp_us +
                             discard_infos_.front().discard_duration_us) {
    total_discard_duration_us_ += discard_infos_.front().discard_duration_us;
    discard_infos_.pop();
  }

  if (discard_infos_.size() > 0) {
    int64_t discard_start_timestamp_us =
        discard_infos_.front().discard_start_timestamp_us;
    int64_t discard_end_timestamp_us =
        discard_start_timestamp_us + discard_infos_.front().discard_duration_us;
    if (timestamp_us >= discard_start_timestamp_us &&
        timestamp_us < discard_end_timestamp_us) {
      // "Freeze" the timestamp at |discard_start_timestamp| if |timestamp|
      // falls within the discard period.
      return std::max(int64_t(0),
                      discard_start_timestamp_us - total_discard_duration_us_);
    }
  }

  return std::max(int64_t(0), timestamp_us - total_discard_duration_us_);
}

void AudioDiscardDurationTracker::CacheDiscardDuration(
    const scoped_refptr<InputBuffer>& input_buffer,
    int64_t buffer_duration_us) {
  mutex_.DCheckAcquired();
  int64_t discard_duration_from_front_us =
      input_buffer->audio_sample_info().discarded_duration_from_front;
  int64_t discard_duration_from_back_us =
      input_buffer->audio_sample_info().discarded_duration_from_back;

  if (discard_duration_from_front_us + discard_duration_from_back_us >=
      buffer_duration_us) {
    discard_infos_.push(
        AudioDiscardInfo{buffer_duration_us, input_buffer->timestamp()});
  } else {
    if (discard_duration_from_front_us > 0) {
      discard_infos_.push(AudioDiscardInfo{discard_duration_from_front_us,
                                           input_buffer->timestamp()});
    }
    if (discard_duration_from_back_us > 0) {
      discard_infos_.push(AudioDiscardInfo{discard_duration_from_back_us,
                                           input_buffer->timestamp() +
                                               buffer_duration_us -
                                               discard_duration_from_back_us});
    }
  }

  // DCHECK to prevent |discard_infos_| from growing arbitrarily large.
  SB_DCHECK(discard_infos_.size() <= kMaxNumberOfPendingAudioDiscardInfos);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
