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

#include "starboard/shared/starboard/player/filter/media_time_provider_impl.h"

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

MediaTimeProviderImpl::MediaTimeProviderImpl(
    scoped_ptr<MonotonicSystemTimeProvider> system_time_provider)
    : system_time_provider_(system_time_provider.Pass()) {
  SB_DCHECK(system_time_provider_);
}

void MediaTimeProviderImpl::Play() {
  SB_DCHECK(BelongsToCurrentThread());

  if (is_playing_) {
    return;
  }

  ScopedLock scoped_lock(mutex_);
  seek_to_time_ = GetCurrentMediaTime_Locked(&seek_to_time_set_at_);
  is_playing_ = true;
}

void MediaTimeProviderImpl::Pause() {
  SB_DCHECK(BelongsToCurrentThread());

  if (!is_playing_) {
    return;
  }

  ScopedLock scoped_lock(mutex_);
  seek_to_time_ = GetCurrentMediaTime_Locked(&seek_to_time_set_at_);
  is_playing_ = false;
}

void MediaTimeProviderImpl::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(BelongsToCurrentThread());

  if (playback_rate_ == playback_rate) {
    return;
  }

  ScopedLock scoped_lock(mutex_);
  seek_to_time_ = GetCurrentMediaTime_Locked(&seek_to_time_set_at_);
  playback_rate_ = playback_rate;
}

void MediaTimeProviderImpl::Seek(SbTime seek_to_time) {
  SB_DCHECK(BelongsToCurrentThread());

  ScopedLock scoped_lock(mutex_);

  seek_to_time_ = seek_to_time;
  seek_to_time_set_at_ = system_time_provider_->GetMonotonicNow();

  // This is unnecessary, but left it here just in case scheduled job is added
  // in future.
  CancelPendingJobs();
}

SbTime MediaTimeProviderImpl::GetCurrentMediaTime(bool* is_playing,
                                                  bool* is_eos_played,
                                                  bool* is_underflow,
                                                  double* playback_rate) {
  ScopedLock scoped_lock(mutex_);

  SbTime current = GetCurrentMediaTime_Locked();

  *is_playing = is_playing_;
  *is_eos_played = false;
  *is_underflow = false;
  *playback_rate = playback_rate_;

  return current;
}

SbTime MediaTimeProviderImpl::GetCurrentMediaTime_Locked(
    SbTimeMonotonic* current_time /*= NULL*/) {
  mutex_.DCheckAcquired();

  SbTimeMonotonic now = system_time_provider_->GetMonotonicNow();

  if (!is_playing_ || playback_rate_ == 0.0) {
    if (current_time) {
      *current_time = now;
    }
    return seek_to_time_;
  }

  SbTimeMonotonic elapsed = (now - seek_to_time_set_at_) * playback_rate_;
  if (current_time) {
    *current_time = now;
  }
  return seek_to_time_ + elapsed;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
