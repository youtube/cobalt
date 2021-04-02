// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/performance.h"

#include "base/time/time.h"
#include "cobalt/dom/memory_info.h"
#include "cobalt/dom/performance_entry.h"

namespace cobalt {
namespace dom {

Performance::Performance(const scoped_refptr<base::BasicClock>& clock)
    : timing_(new PerformanceTiming(clock)),
      memory_(new MemoryInfo()),
      time_origin_(base::Time::Now() - base::Time::UnixEpoch()) {}

DOMHighResTimeStamp Performance::Now() const {
  base::TimeDelta now = base::Time::Now() - base::Time::UnixEpoch();
  return ConvertTimeDeltaToDOMHighResTimeStamp(now - time_origin_,
      Performance::kPerformanceTimerMinResolutionInMicroseconds);
}

scoped_refptr<PerformanceTiming> Performance::timing() const { return timing_; }

scoped_refptr<MemoryInfo> Performance::memory() const { return memory_; }

DOMHighResTimeStamp Performance::time_origin() const {
  // The algorithm for calculating time origin timestamp.
  //   https://www.w3.org/TR/2019/REC-hr-time-2-20191121/#dfn-time-origin-timestamp
  // Assert that global's time origin is not undefined.
  DCHECK(!time_origin_.is_zero());

  // Let t1 be the DOMHighResTimeStamp representing the high resolution
  // time at which the global monotonic clock is zero.
  base::TimeDelta t1 =
      base::Time::UnixEpoch().ToDeltaSinceWindowsEpoch();

  // Let t2 be the DOMHighResTimeStamp representing the high resolution
  // time value of the global monotonic clock at global's time origin.
  base::TimeDelta t2 = time_origin_;

  // Return the sum of t1 and t2.
  return ConvertTimeDeltaToDOMHighResTimeStamp(t1 + t2,
      Performance::kPerformanceTimerMinResolutionInMicroseconds);
}

void Performance::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(timing_);
  tracer->Trace(memory_);
}

PerformanceEntryList Performance::GetEntries() {
  return PerformanceEntryBufferImpl::GetEntries(performance_entry_buffer_);
}

PerformanceEntryList Performance::GetEntriesByType(
    const std::string& entry_type) {
  return PerformanceEntryBufferImpl::GetEntriesByType(
      performance_entry_buffer_, entry_type);
}

PerformanceEntryList Performance::GetEntriesByName(
    const std::string& name, const base::StringPiece& type) {
  return PerformanceEntryBufferImpl::GetEntriesByName(
      performance_entry_buffer_, name, type);
}

}  // namespace dom
}  // namespace cobalt
