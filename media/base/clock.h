// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A clock represent a single source of time to allow audio and video streams
// to synchronize with each other.  Clocks essentially track the media time with
// respect to some other source of time, whether that may be the system clock,
// audio hardware or some other OS-level API.
//
// Clocks start off paused with a playback rate of 1.0f and a media time of 0.
//
// TODO(scherkus): Clocks will some day be responsible for executing callbacks
// given a media time.  This will be used primarily by video renderers.  For now
// we'll keep using a poll-and-sleep solution.

#ifndef MEDIA_BASE_CLOCK_H_
#define MEDIA_BASE_CLOCK_H_

#include "base/time.h"

namespace media {

class Clock {
 public:
  // Starts the clock and returns the current media time, which will increase
  // with respect to the current playback rate.
  virtual base::TimeDelta Play() = 0;

  // Stops the clock and returns the current media time, which will remain
  // constant until Play() is called.
  virtual base::TimeDelta Pause() = 0;

  // Sets a new playback rate.  The rate at which the media time will increase
  // will now change.
  virtual void SetPlaybackRate(float playback_rate) = 0;

  // Forcefully sets the media time to the given time.  This should only be used
  // where a discontinuity in the media is found (i.e., seeking).
  virtual void SetTime(const base::TimeDelta& time) = 0;

  // Returns the current elapsed media time.
  virtual base::TimeDelta Elapsed() const = 0;

 protected:
  virtual ~Clock() {}
};

}  // namespace media

#endif  // MEDIA_BASE_CLOCK_H_
