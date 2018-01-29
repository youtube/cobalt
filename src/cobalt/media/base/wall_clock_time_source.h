// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_WALL_CLOCK_TIME_SOURCE_H_
#define COBALT_MEDIA_BASE_WALL_CLOCK_TIME_SOURCE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/default_tick_clock.h"
#include "base/synchronization/lock.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/time_source.h"

namespace cobalt {
namespace media {

// A time source that uses interpolation based on the system clock.
class MEDIA_EXPORT WallClockTimeSource : public TimeSource {
 public:
  WallClockTimeSource();
  ~WallClockTimeSource() override;

  // TimeSource implementation.
  void StartTicking() override;
  void StopTicking() override;
  void SetPlaybackRate(double playback_rate) override;
  void SetMediaTime(base::TimeDelta time) override;
  base::TimeDelta CurrentMediaTime() override;
  bool GetWallClockTimes(
      const std::vector<base::TimeDelta>& media_timestamps,
      std::vector<base::TimeTicks>* wall_clock_times) override;

  void set_tick_clock_for_testing(base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 private:
  base::TimeDelta CurrentMediaTime_Locked();

  // Allow for an injectable tick clock for testing.
  base::DefaultTickClock default_tick_clock_;

  // If specified, used instead of |default_tick_clock_|.
  base::TickClock* tick_clock_;

  bool ticking_;

  // While ticking we can interpolate the current media time by measuring the
  // delta between our reference ticks and the current system ticks and scaling
  // that time by the playback rate.
  double playback_rate_;
  base::TimeDelta base_timestamp_;
  base::TimeTicks reference_time_;

  // TODO(scherkus): Remove internal locking from this class after access to
  // Renderer::CurrentMediaTime() is single threaded http://crbug.com/370634
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(WallClockTimeSource);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_WALL_CLOCK_TIME_SOURCE_H_
