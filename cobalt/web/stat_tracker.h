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

#ifndef COBALT_WEB_STAT_TRACKER_H_
#define COBALT_WEB_STAT_TRACKER_H_

#include <string>

#include "cobalt/base/c_val.h"

namespace cobalt {
namespace web {

// Tracks stats for web resources.
class StatTracker {
 public:
  explicit StatTracker(const std::string& name);
  ~StatTracker();

  void OnWindowTimersIntervalCreated() {
    ++count_window_timers_interval_created_;
  }
  void OnWindowTimersIntervalDestroyed() {
    ++count_window_timers_interval_destroyed_;
  }
  void OnWindowTimersTimeoutCreated() {
    ++count_window_timers_timeout_created_;
  }
  void OnWindowTimersTimeoutDestroyed() {
    ++count_window_timers_timeout_destroyed_;
  }

  void FlushPeriodicTracking();

 private:
  // Count cvals that are updated when the periodic tracking is flushed.
  base::CVal<int, base::CValPublic> count_window_timers_interval_;
  base::CVal<int, base::CValPublic> count_window_timers_timeout_;

  // Periodic counts. The counts are cleared after the CVals are updated in
  // |FlushPeriodicTracking|.
  int count_window_timers_interval_created_;
  int count_window_timers_interval_destroyed_;
  int count_window_timers_timeout_created_;
  int count_window_timers_timeout_destroyed_;
};
}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_STAT_TRACKER_H_
