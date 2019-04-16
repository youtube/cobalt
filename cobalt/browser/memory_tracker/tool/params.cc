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

#include "cobalt/browser/memory_tracker/tool/params.h"

#include <sstream>

#include "base/basictypes.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

Params::Params(nb::analytics::MemoryTracker* memory_tracker,
               AbstractLogger* logger, base::Time start_time)
    : memory_tracker_(memory_tracker),
      finished_(false),
      logger_(logger),
      timer_(start_time) {}

bool Params::finished() const { return finished_; }

bool Params::wait_for_finish_signal(SbTime wait_us) {
  return finished_semaphore_.TakeWait(wait_us);
}

void Params::set_finished(bool val) {
  finished_ = val;
  finished_semaphore_.Put();
}

nb::analytics::MemoryTracker* Params::memory_tracker() const {
  return memory_tracker_;
}

cobalt::browser::memory_tracker::AbstractLogger* Params::logger() {
  return logger_.get();
}

base::TimeDelta Params::time_since_start() const {
  return base::Time::NowFromSystemTime() - timer_;
}

std::string Params::TimeInMinutesString() const {
  base::TimeDelta delta_t = time_since_start();
  int64 seconds = delta_t.InSeconds();
  float time_mins = static_cast<float>(seconds) / 60.f;
  std::stringstream ss;

  ss << time_mins;
  return ss.str();
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
