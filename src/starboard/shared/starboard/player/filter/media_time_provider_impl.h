// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MEDIA_TIME_PROVIDER_IMPL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MEDIA_TIME_PROVIDER_IMPL_H_

#include "starboard/common/mutex.h"
#include "starboard/common/optional.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// This class provides the media playback time when there isn't an audio track.
class MediaTimeProviderImpl : public MediaTimeProvider,
                              private JobQueue::JobOwner {
 public:
  class MonotonicSystemTimeProvider {
   public:
    virtual ~MonotonicSystemTimeProvider() {}
    virtual SbTimeMonotonic GetMonotonicNow() const = 0;
  };

  explicit MediaTimeProviderImpl(
      scoped_ptr<MonotonicSystemTimeProvider> system_time_provider);

  void Play() override;
  void Pause() override;
  void SetPlaybackRate(double playback_rate) override;
  void Seek(SbTime seek_to_time) override;
  SbTime GetCurrentMediaTime(bool* is_playing,
                             bool* is_eos_played,
                             bool* is_underflow,
                             double* playback_rate) override;

 private:
  // When not NULL, |current_time| will be set to the current monotonic time
  // used to calculate the returned media time.  Note that it is possible that
  // |current_time| points to |seek_to_time_set_at_| and the implementation
  // should handle this properly.
  SbTime GetCurrentMediaTime_Locked(SbTimeMonotonic* current_time = NULL);

  scoped_ptr<MonotonicSystemTimeProvider> system_time_provider_;

  Mutex mutex_;

  double playback_rate_ = 0.0;
  bool is_playing_ = false;
  SbTime seek_to_time_ = 0;
  SbTimeMonotonic seek_to_time_set_at_ = SbTimeGetMonotonicNow();
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MEDIA_TIME_PROVIDER_IMPL_H_
