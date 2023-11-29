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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_DISCARD_DURATION_TRACKER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_DISCARD_DURATION_TRACKER_H_

#include <unordered_map>

#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// This class caches the discard durations of audio buffers to maintain a
// running total discard duration. This total can be used to adjust the media
// time of the playback when the audio buffers do not support partial audio,
// and prevent an A/V desync.
class AudioDiscardDurationTracker {
 public:
  void CacheDiscardDuration(const InputBuffer& input_buffer);
  void CacheMultipleDiscardDurations(const InputBuffers& input_buffers);
  void AddCachedDiscardDurationToTotal(SbTime timestamp,
                                       bool remove_timestamp_from_cache = true);
  SbTime AdjustTimeForTotalDiscardDuration(SbTime timestamp);
  void Reset() {
    discard_durations_by_timestamp_.clear();
    total_discard_duration_ = 0;
  }

  SbTime total_discard_duration() const { return total_discard_duration_; }

 private:
  std::unordered_map<SbTime, SbTime> discard_durations_by_timestamp_;
  SbTime total_discard_duration_ = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_DISCARD_DURATION_TRACKER_H_
