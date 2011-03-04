// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/clock.h"

#include "base/logging.h"
#include "media/base/buffers.h"

namespace media {

Clock::Clock(TimeProvider* time_provider)
    : time_provider_(time_provider),
      playing_(false),
      playback_rate_(1.0f) {
}

Clock::~Clock() {}

base::TimeDelta Clock::Play() {
  DCHECK(!playing_);
  reference_ = GetTimeFromProvider();
  playing_ = true;
  return media_time_;
}

base::TimeDelta Clock::Pause() {
  DCHECK(playing_);
  // Save our new accumulated amount of media time.
  media_time_ = Elapsed();
  playing_ = false;
  return media_time_;
}

void Clock::SetTime(const base::TimeDelta& time) {
  if (time == kNoTimestamp) {
    NOTREACHED();
    return;
  }
  if (playing_) {
    reference_ = GetTimeFromProvider();
  }
  media_time_ = time;
}

void Clock::SetPlaybackRate(float playback_rate) {
  if (playing_) {
    base::Time time = GetTimeFromProvider();
    media_time_ = ElapsedViaProvidedTime(time);
    reference_ = time;
  }
  playback_rate_ = playback_rate;
}

base::TimeDelta Clock::Elapsed() const {
  if (!playing_) {
    return media_time_;
  }
  return ElapsedViaProvidedTime(GetTimeFromProvider());
}

base::TimeDelta Clock::ElapsedViaProvidedTime(const base::Time& time) const {
  // TODO(scherkus): floating point badness scaling time by playback rate.
  int64 now_us = (time - reference_).InMicroseconds();
  now_us = static_cast<int64>(now_us * playback_rate_);
  return media_time_ + base::TimeDelta::FromMicroseconds(now_us);
}

base::Time Clock::GetTimeFromProvider() const {
  if (time_provider_) {
    return time_provider_();
  }
  return base::Time();
}

}  // namespace media
