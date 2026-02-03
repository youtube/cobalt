// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_LINUX_TIME_ZONE_MONITOR_H_
#define STARBOARD_SHARED_LINUX_TIME_ZONE_MONITOR_H_

#include <atomic>

#include "starboard/common/ref_counted.h"
#include "starboard/common/thread.h"

namespace starboard {
namespace shared {
namespace linux {

// TimeZoneMonitor monitors system time zone changes on Linux by watching
// common configuration files. It dispatches a Starboard event when a change
// is detected.
class TimeZoneMonitor
    : public ::starboard::Thread,
      public ::starboard::RefCountedThreadSafe<TimeZoneMonitor> {
 public:
  static ::starboard::scoped_refptr<TimeZoneMonitor> Create();

  TimeZoneMonitor(const TimeZoneMonitor&) = delete;
  TimeZoneMonitor& operator=(const TimeZoneMonitor&) = delete;

 private:
  friend class ::starboard::RefCountedThreadSafe<TimeZoneMonitor>;

  TimeZoneMonitor();
  ~TimeZoneMonitor() override;

  // starboard::Thread overrides
  void Run() override;

  void UpdateWatches();

  int inotify_fd_ = -1;
  int stop_event_fd_ = -1;
  std::atomic<bool> stop_requested_{false};
};

}  // namespace linux
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_LINUX_TIME_ZONE_MONITOR_H_
