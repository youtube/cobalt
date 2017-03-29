// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/frame_rate_throttler.h"

#include "base/debug/trace_event.h"
#include "base/threading/platform_thread.h"

namespace cobalt {
namespace renderer {

void FrameRateThrottler::BeginInterval() {
  if (COBALT_MINIMUM_FRAME_TIME_IN_MILLISECONDS > 0) {
    begin_time_ = base::TimeTicks::HighResNow();
  }
}

void FrameRateThrottler::EndInterval() {
  // Throttle presentation of new frames if a minimum frame time is specified.
  if (COBALT_MINIMUM_FRAME_TIME_IN_MILLISECONDS > 0) {
    TRACE_EVENT0("cobalt::renderer", "FrameRateThrottler::EndInterval()");
    if (!begin_time_.is_null()) {
      base::TimeDelta elapsed = base::TimeTicks::HighResNow() - begin_time_;
      base::TimeDelta wait_time =
          base::TimeDelta::FromMillisecondsD(
              COBALT_MINIMUM_FRAME_TIME_IN_MILLISECONDS) -
          elapsed;
      if (wait_time > base::TimeDelta::FromMicroseconds(0)) {
        base::PlatformThread::Sleep(wait_time);
      }
    }
  }
}

}  // namespace renderer
}  // namespace cobalt
