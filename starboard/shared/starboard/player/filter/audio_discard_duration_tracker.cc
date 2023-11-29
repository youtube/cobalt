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
#include <unordered_map>

#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
using shared::starboard::media::AudioSampleInfo;

void AudioDiscardDurationTracker::CacheDiscardDuration(
    const InputBuffer& input_buffer) {
  SB_DCHECK(input_buffer.sample_type() == kSbMediaTypeAudio);
  const AudioSampleInfo audio_sample_info = input_buffer.audio_sample_info();
  discard_durations_by_timestamp_[input_buffer.timestamp()] =
      audio_sample_info.discarded_duration_from_front +
      audio_sample_info.discarded_duration_from_back;
}

void AudioDiscardDurationTracker::CacheMultipleDiscardDurations(
    const InputBuffers& input_buffers) {
  for (const auto& input_buffer : input_buffers) {
    CacheDiscardDuration(*input_buffer.get());
  }
}

void AudioDiscardDurationTracker::AddCachedDiscardDurationToTotal(
    SbTime timestamp,
    bool remove_timestamp_from_cache) {
  if (discard_durations_by_timestamp_.find(timestamp) ==
      discard_durations_by_timestamp_.end()) {
    SB_LOG(INFO) << "Discarded duration for timestamp: " << timestamp
                 << " is not cached.";
    return;
  }

  if (discard_durations_by_timestamp_.find(timestamp) !=
      discard_durations_by_timestamp_.end()) {
    total_discard_duration_ += discard_durations_by_timestamp_[timestamp];
    if (remove_timestamp_from_cache) {
      discard_durations_by_timestamp_.erase(timestamp);
    }
  }
}

SbTime AudioDiscardDurationTracker::AdjustTimeForTotalDiscardDuration(
    SbTime timestamp) {
  SbTime adjusted_timestamp = timestamp - total_discard_duration_;
  return std::max(SbTime(0), adjusted_timestamp);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
