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

#include <mutex>
#include <utility>

#include "starboard/common/log.h"

namespace starboard {

MediaTimeProviderImpl::MediaTimeProviderImpl(
    std::unique_ptr<MonotonicSystemTimeProvider> system_time_provider)
    : system_time_provider_(std::move(system_time_provider)) {
  SB_CHECK(system_time_provider_);
}

void MediaTimeProviderImpl::Play() {
  SB_CHECK(BelongsToCurrentThread());

  if (is_playing_) {
    return;
  }

  std::lock_guard scoped_lock(mutex_);
  seek_to_time_ = GetCurrentMediaTime_Locked(&seek_to_time_set_at_);
  is_playing_ = true;
}

void MediaTimeProviderImpl::Pause() {
  SB_CHECK(BelongsToCurrentThread());

  if (!is_playing_) {
    return;
  }

  std::lock_guard scoped_lock(mutex_);
  seek_to_time_ = GetCurrentMediaTime_Locked(&seek_to_time_set_at_);
  is_playing_ = false;
}

void MediaTimeProviderImpl::SetPlaybackRate(double playback_rate) {
  SB_CHECK(BelongsToCurrentThread());

  if (playback_rate_ == playback_rate) {
    return;
  }

  std::lock_guard scoped_lock(mutex_);
  seek_to_time_ = GetCurrentMediaTime_Locked(&seek_to_time_set_at_);
  playback_rate_ = playback_rate;
}

void MediaTimeProviderImpl::Seek(int64_t seek_to_time) {
  SB_CHECK(BelongsToCurrentThread());

  std::lock_guard scoped_lock(mutex_);

  seek_to_time_ = seek_to_time;
  seek_to_time_set_at_ = system_time_provider_->GetMonotonicNow();

  // This is unnecessary, but left it here just in case scheduled job is added
  // in future.
  CancelPendingJobs();
}

int64_t MediaTimeProviderImpl::GetCurrentMediaTime(bool* is_playing,
                                                   bool* is_eos_played,
                                                   bool* is_underflow,
                                                   double* playback_rate) {
  std::lock_guard scoped_lock(mutex_);

  int64_t current = GetCurrentMediaTime_Locked();

  *is_playing = is_playing_;
  *is_eos_played = false;
  *is_underflow = false;
  *playback_rate = playback_rate_;

  return current;
}

int64_t MediaTimeProviderImpl::GetCurrentMediaTime_Locked(
    int64_t* current_time /*= NULL*/) {
  int64_t now = system_time_provider_->GetMonotonicNow();

  if (!is_playing_ || playback_rate_ == 0.0) {
    if (current_time) {
      *current_time = now;
    }
    return seek_to_time_;
  }

  int64_t elapsed = (now - seek_to_time_set_at_) * playback_rate_;
  if (current_time) {
    *current_time = now;
  }
  return seek_to_time_ + elapsed;
}

}  // namespace starboard
