// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_BASE_C_VAL_TIME_INTERVAL_TIMER_STATS_H_
#define COBALT_BASE_C_VAL_TIME_INTERVAL_TIMER_STATS_H_

#include <string>

#include "base/time/time.h"
#include "cobalt/base/c_val_time_interval_entry_stats.h"

namespace base {

// This class is a wrapper around |CValTimeIntervalEntryStats|.
// |CValTimeIntervalTimerStats| calculates the time elapsed between calls to
// Start() and Stop() and adds it as an entry to |CValTimeIntervalEntryStats|.
template <typename Visibility = CValDebug>
class CValTimeIntervalTimerStats {
 public:
  CValTimeIntervalTimerStats(const std::string& name, int64 time_interval_in_ms)
      : entry_stats_(name, time_interval_in_ms) {}

  // Start the timer. If the timer is currently running, it is stopped and
  // re-started. If no time is provided, then base::TimeTicks::Now() is used.
  void Start() {
    base::TimeTicks now = base::TimeTicks::Now();
    Start(now);
  }
  void Start(const base::TimeTicks& now) {
    Stop(now);
    start_time_ = now;
  }

  // Stop the timer. If it is not already started, then nothing happens. if no
  // time is provided, then base::TimeTicks::Now() is used.
  void Stop() {
    base::TimeTicks now = base::TimeTicks::Now();
    Stop(now);
  }
  void Stop(const base::TimeTicks& now) {
    if (start_time_.is_null()) {
      return;
    }

    entry_stats_.AddEntry(now - start_time_, now);
    start_time_ = base::TimeTicks();
  }

 private:
  base::CValTimeIntervalEntryStats<base::TimeDelta, Visibility> entry_stats_;
  base::TimeTicks start_time_;
};

}  // namespace base

#endif  // COBALT_BASE_C_VAL_TIME_INTERVAL_TIMER_STATS_H_
