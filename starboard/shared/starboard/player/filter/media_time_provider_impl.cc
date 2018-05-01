// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "starboard/log.h"

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

void MediaTimeProviderImpl::Initialize(const ErrorCB& error_cb,
                                       const PrerolledCB& prerolled_cb,
                                       const EndedCB& ended_cb) {
  SB_UNREFERENCED_PARAMETER(error_cb);
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(prerolled_cb);
  SB_DCHECK(ended_cb);
  SB_DCHECK(!prerolled_cb_);
  SB_DCHECK(!ended_cb_);

  prerolled_cb_ = prerolled_cb;
  ended_cb_ = ended_cb;
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
  SB_DCHECK(prerolled_cb_);

  ScopedLock scoped_lock(mutex_);

  seek_to_time_ = seek_to_time;
  seek_to_time_set_at_ = system_time_provider_->GetMonotonicNow();
  video_duration_ = nullopt;
  is_video_end_of_stream_reached_ = false;

  CancelPendingJobs();
  Schedule(prerolled_cb_);
}

SbTime MediaTimeProviderImpl::GetCurrentMediaTime(bool* is_playing,
                                                  bool* is_eos_played) {
  SB_DCHECK(ended_cb_);

  ScopedLock scoped_lock(mutex_);

  SbTime current = GetCurrentMediaTime_Locked();

  *is_playing = is_playing_;
  *is_eos_played =
      is_video_end_of_stream_reached_ &&
      (!video_duration_.has_engaged() || current >= video_duration_.value());

  Schedule(ended_cb_);
  return current;
}

void MediaTimeProviderImpl::UpdateVideoDuration(
    optional<SbTime> video_duration) {
  ScopedLock scoped_lock(mutex_);
  video_duration_ = video_duration;
}

void MediaTimeProviderImpl::VideoEndOfStreamReached() {
  ScopedLock scoped_lock(mutex_);
  is_video_end_of_stream_reached_ = true;
  if (!video_duration_.has_engaged()) {
    video_duration_ = seek_to_time_;
  }
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
