/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef COBALT_BASE_C_VAL_TIME_INTERVAL_TIMER_H_
#define COBALT_BASE_C_VAL_TIME_INTERVAL_TIMER_H_

#include <string>

#include "base/time.h"
#include "cobalt/base/c_val_time_interval_entry_stats.h"

namespace base {

// This class is a wrapper around |CValTimeIntervalEntryStats|.
// |CValTimeIntervalTimer| calculates the time elapsed between calls to |Start|
// and |Stop| and adds it as an entry to |CValTimeIntervalEntryStats|.
class CValTimeIntervalTimer {
 public:
  CValTimeIntervalTimer(const std::string& name, int64 time_interval_in_ms);

  // Start the timer. If the timer is currently running, it is stopped and
  // re-started. If no time is provided, then |base::TimeTicks::Now()| is used.
  void Start();
  void Start(const base::TimeTicks& now);

  // Stop the timer. If it is not already started, then nothing happens. if no
  // time is provided, then |base::TimeTicks::Now()| is used.
  void Stop();
  void Stop(const base::TimeTicks& now);

 private:
  base::CValTimeIntervalEntryStats<int64> entry_stats_;
  base::TimeTicks start_time_;
};

}  // namespace base

#endif  // COBALT_BASE_C_VAL_TIME_INTERVAL_TIMER_H_
