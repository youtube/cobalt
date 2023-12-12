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

#include <queue>

#include "starboard/common/mutex.h"
#include "starboard/common/ref_counted.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

using ::starboard::scoped_refptr;

// This class caches the discard durations of audio buffers to maintain a
// running total discard duration. This total can be used to adjust the media
// time of the playback when the audio buffers do not support partial audio.
class AudioDiscardDurationTracker {
 public:
  void CacheMultipleDiscardDurations(const InputBuffers& input_buffers,
                                     SbTime buffer_duration);
  // Adjusts the given timestamp by taking the total duration of discarded audio
  // into account. When |timestamp| falls within the current AudioDiscardInfo,
  // AdjustTimeForTotalDiscardDuration() will return a timestamp based on the
  // start timestamp of the discard duration. When |timestamp| is beyond the end
  // of the discard duration, the discard duration will be added to the running
  // total. This way AdjustTimeForTotalDiscardDuration() will "freeze" the
  // timestamp until the end of the discard duration.
  //
  // Ex. Given a |timestamp| 28 ms with |total_discard_duration| == 0 and
  // current AudioDiscardInfo == { discard_duration: 10 ms,
  // discard_start_timestamp: 22 ms}, AdjustTimeForTotalDiscardDuration()
  // returns discard_start_timestamp - |total_discard_duration| == 22 ms. If a
  // |timestamp| 33 ms is passed later, discard_duration is added to
  // |total_discard_duration|, and the AudioDiscardInfo is removed.
  //
  // Timestamps passed to AdjustTimeForTotalDiscardDuration() must be
  // monotonically increasing for the algorithm to run correctly.
  SbTime AdjustTimeForTotalDiscardDuration(SbTime timestamp);
  void Reset() {
    ScopedLock lock(mutex_);
    discard_infos_ = std::queue<AudioDiscardInfo>();
    total_discard_duration_ = 0;
    last_received_timestamp_ = 0;
  }

 private:
  struct AudioDiscardInfo {
    SbTime discard_duration;
    SbTime discard_start_timestamp;
  };

  // Stores the timestamps and audio discard durations of an InputBuffer
  // in an AudioDiscardInfo. |buffer_duration| is given to determine the
  // timestamp of when audio begins discarding from the back of an InputBuffer.
  // Each audio discard duration creates a separate AudioDiscardInfo, so there
  // may be two AudioDiscardInfos for one InputBuffer if audio is discarded from
  // both the front and back.
  // When audio is discarded from the front,
  // discard_start_timestamp == input_buffer->timestamp().
  // When audio is discarded from the back,
  // discard_start_timestamp == input_buffer->timestamp() + |buffer_duration|
  // - <audio discarded from the back>.
  // The timestamp of the end of the discard period is the sum of the
  // discard_start_timestamp and discard_duration.
  // When the sum of audio discarded from the front and back matches or exceeds
  // the duration of the buffer, a single AudioDiscardInfo is created to discard
  // the entire duration of the buffer.
  void CacheDiscardDuration(const scoped_refptr<InputBuffer>& input_buffer,
                            SbTime buffer_duration);

  Mutex mutex_;
  std::queue<AudioDiscardInfo> discard_infos_;
  SbTime total_discard_duration_ = 0;
  // Used to check for timestamp regressions in
  // AdjustTimeForTotalDiscardDuration().
  SbTime last_received_timestamp_ = 0;

  static constexpr size_t kMaxNumberOfPendingAudioDiscardInfos = 128;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_DISCARD_DURATION_TRACKER_H_
