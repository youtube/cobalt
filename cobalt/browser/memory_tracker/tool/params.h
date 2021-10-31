// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_PARAMS_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_PARAMS_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "nb/analytics/memory_tracker.h"
#include "starboard/common/semaphore.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

class AbstractLogger;

class Params {
 public:
  Params(nb::analytics::MemoryTracker* memory_tracker, AbstractLogger* logger,
         base::Time start_time);
  bool finished() const;
  bool wait_for_finish_signal(SbTime wait_us);
  void set_finished(bool val);

  nb::analytics::MemoryTracker* memory_tracker() const;
  AbstractLogger* logger();
  base::TimeDelta time_since_start() const;
  std::string TimeInMinutesString() const;

 private:
  nb::analytics::MemoryTracker* memory_tracker_;
  bool finished_;
  std::unique_ptr<AbstractLogger> logger_;
  base::Time timer_;
  starboard::Semaphore finished_semaphore_;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_PARAMS_H_
