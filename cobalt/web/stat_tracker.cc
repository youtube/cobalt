// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/web/stat_tracker.h"

#include "base/strings/stringprintf.h"

namespace cobalt {
namespace web {
StatTracker::StatTracker(const std::string& name)
    : count_window_timers_interval_(
          base::StringPrintf("Count.%s.DOM.WindowTimers.Interval",
                             name.c_str()),
          0, "Number of active WindowTimer Intervals."),
      count_window_timers_timeout_(
          base::StringPrintf("Count.%s.DOM.WindowTimers.Timeout", name.c_str()),
          0, "Number of active WindowTimer Timeouts."),
      count_window_timers_interval_created_(0),
      count_window_timers_interval_destroyed_(0),
      count_window_timers_timeout_created_(0),
      count_window_timers_timeout_destroyed_(0) {}

StatTracker::~StatTracker() {
  FlushPeriodicTracking();

  // Verify that all of the elements were removed from the document and
  // destroyed.
  DCHECK_EQ(count_window_timers_interval_, 0);
  DCHECK_EQ(count_window_timers_timeout_, 0);
}

void StatTracker::FlushPeriodicTracking() {
  // Update the CVals before clearing the periodic values.
  count_window_timers_interval_ += count_window_timers_interval_created_ -
                                   count_window_timers_interval_destroyed_;
  count_window_timers_timeout_ += count_window_timers_timeout_created_ -
                                  count_window_timers_timeout_destroyed_;

  // Now clear the values.
  count_window_timers_interval_created_ = 0;
  count_window_timers_interval_destroyed_ = 0;
  count_window_timers_timeout_created_ = 0;
  count_window_timers_timeout_destroyed_ = 0;
}
}  // namespace web
}  // namespace cobalt
