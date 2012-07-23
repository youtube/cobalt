// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/clock.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/buffers.h"

namespace media {

Clock::Clock(TimeProvider* time_provider)
    : time_provider_(time_provider) {
  Reset();
}

Clock::~Clock() {}

bool Clock::IsPlaying() const {
  return playing_;
}

base::TimeDelta Clock::Play() {
  DCHECK(!playing_);
  UpdateReferencePoints();
  playing_ = true;
  return media_time_;
}

base::TimeDelta Clock::Pause() {
  DCHECK(playing_);
  UpdateReferencePoints();
  playing_ = false;
  return media_time_;
}

void Clock::SetPlaybackRate(float playback_rate) {
  UpdateReferencePoints();
  playback_rate_ = playback_rate;
}

void Clock::SetTime(base::TimeDelta current_time, base::TimeDelta max_time) {
  DCHECK(current_time <= max_time);
  DCHECK(current_time != kNoTimestamp());

  UpdateReferencePoints(current_time);
  max_time_ = ClampToValidTimeRange(max_time);
  underflow_ = false;
}

base::TimeDelta Clock::Elapsed() {
  if (duration_ == kNoTimestamp())
    return base::TimeDelta();

  // The clock is not advancing, so return the last recorded time.
  if (!playing_ || underflow_)
    return media_time_;

  base::TimeDelta elapsed = EstimatedElapsedTime();
  if (max_time_ != kNoTimestamp() && elapsed > max_time_) {
    UpdateReferencePoints(max_time_);
    underflow_ = true;
    elapsed = max_time_;
  }

  return elapsed;
}

void Clock::SetMaxTime(base::TimeDelta max_time) {
  DCHECK(max_time != kNoTimestamp());

  UpdateReferencePoints();
  max_time_ = ClampToValidTimeRange(max_time);

  underflow_ = media_time_ > max_time_;
  if (underflow_)
    media_time_ = max_time_;
}

void Clock::SetDuration(base::TimeDelta duration) {
  DCHECK(duration_ == kNoTimestamp() || duration_ == kInfiniteDuration());
  DCHECK(duration > base::TimeDelta());
  duration_ = duration;
}

base::TimeDelta Clock::ElapsedViaProvidedTime(const base::Time& time) const {
  // TODO(scherkus): floating point badness scaling time by playback rate.
  int64 now_us = (time - reference_).InMicroseconds();
  now_us = static_cast<int64>(now_us * playback_rate_);
  return media_time_ + base::TimeDelta::FromMicroseconds(now_us);
}

base::Time Clock::GetTimeFromProvider() const {
  if (time_provider_)
    return time_provider_();
  return base::Time();
}

base::TimeDelta Clock::ClampToValidTimeRange(base::TimeDelta time) const {
  if (duration_ == kNoTimestamp())
    return base::TimeDelta();
  return std::max(std::min(time, duration_), base::TimeDelta());
}

void Clock::EndOfStream() {
  Pause();
  SetTime(Duration(), Duration());
}

base::TimeDelta Clock::Duration() const {
  if (duration_ == kNoTimestamp())
    return base::TimeDelta();
  return duration_;
}

void Clock::UpdateReferencePoints() {
  UpdateReferencePoints(Elapsed());
}

void Clock::UpdateReferencePoints(base::TimeDelta current_time) {
  media_time_ = ClampToValidTimeRange(current_time);
  reference_ = GetTimeFromProvider();
}

base::TimeDelta Clock::EstimatedElapsedTime() {
  return ClampToValidTimeRange(
      ElapsedViaProvidedTime(GetTimeFromProvider()));
}

void Clock::Reset() {
  playing_ = false;
  playback_rate_ = 1.0f;
  max_time_ = kNoTimestamp();
  duration_ = kNoTimestamp();
  media_time_ = base::TimeDelta();
  reference_ = base::Time();
  underflow_ = false;
}

}  // namespace media
