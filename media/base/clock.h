// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CLOCK_H_
#define MEDIA_BASE_CLOCK_H_

#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/time.h"

namespace media {

// A clock represents a single source of time to allow audio and video streams
// to synchronize with each other.  Clock essentially tracks the media time with
// respect to some other source of time, whether that may be the system clock or
// updates via SetTime(). Clock uses linear interpolation to calculate the
// current media time since the last time SetTime() was called.
//
// Clocks start off paused with a playback rate of 1.0f and a media time of 0.
//
// Clock is not thread-safe and must be externally locked.
//
// TODO(scherkus): Clock will some day be responsible for executing callbacks
// given a media time.  This will be used primarily by video renderers.  For now
// we'll keep using a poll-and-sleep solution.
class Clock {
 public:
  // Type for a static function pointer that acts as a time source.
  typedef base::Time(TimeProvider)();

  Clock(TimeProvider* time_provider);
  ~Clock();

  // Starts the clock and returns the current media time, which will increase
  // with respect to the current playback rate.
  base::TimeDelta Play();

  // Stops the clock and returns the current media time, which will remain
  // constant until Play() is called.
  base::TimeDelta Pause();

  // Sets a new playback rate.  The rate at which the media time will increase
  // will now change.
  void SetPlaybackRate(float playback_rate);

  // Forcefully sets the media time to the given time.  This should only be used
  // where a discontinuity in the media is found (i.e., seeking).
  void SetTime(const base::TimeDelta& time);

  // Returns the current elapsed media time.
  base::TimeDelta Elapsed() const;

 private:
  // Returns the current media time treating the given time as the latest
  // value as returned by |time_provider_|.
  base::TimeDelta ElapsedViaProvidedTime(const base::Time& time) const;

  base::Time GetTimeFromProvider() const;

  // Function returning current time in base::Time units.
  TimeProvider* time_provider_;

  // Whether the clock is running.
  bool playing_;

  // The system clock time when this clock last starting playing or had its
  // time set via SetTime().
  base::Time reference_;

  // Current accumulated amount of media time.  The remaining portion must be
  // calculated by comparing the system time to the reference time.
  base::TimeDelta media_time_;

  // Current playback rate.
  float playback_rate_;

  DISALLOW_COPY_AND_ASSIGN(Clock);
};

}  // namespace media

#endif  // MEDIA_BASE_CLOCK_H_
