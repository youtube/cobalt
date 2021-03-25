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

#ifndef COBALT_DOM_PERFORMANCE_H_
#define COBALT_DOM_PERFORMANCE_H_

#include <string>

#include "cobalt/base/clock.h"
#include "cobalt/dom/performance_entry_buffer_impl.h"
#include "cobalt/dom/performance_high_resolution_time.h"
#include "cobalt/dom/performance_observer_entry_list.h"
#include "cobalt/dom/performance_timing.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class MemoryInfo;

// Implements the Performance IDL interface, an instance of which is created
// and owned by the Window object.
//   https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html#sec-window.performance-attribute
class Performance : public script::Wrappable {
 public:
  // Ensure that the timer resolution is at the lowest 20 microseconds in
  // order to mitigate potential Spectre-related attacks.  This is following
  // Mozilla's lead as described here:
  //   https://www.mozilla.org/en-US/security/advisories/mfsa2018-01/
  static constexpr int64_t kPerformanceTimerMinResolutionInMicroseconds = 20;

  explicit Performance(const scoped_refptr<base::BasicClock>& clock);

  // Web API: Performance
  scoped_refptr<PerformanceTiming> timing() const;
  scoped_refptr<MemoryInfo> memory() const;

  // https://www.w3.org/TR/hr-time-2/#now-method
  DOMHighResTimeStamp Now() const;

  // High Resolution Time Level 3 timeOrigin.
  //   https://www.w3.org/TR/hr-time-3/#dom-performance-timeorigin
  DOMHighResTimeStamp time_origin() const;

  // Internal getter method for the time origin value.
  base::TimeDelta get_time_origin() const {
    return time_origin_;
  }

  DEFINE_WRAPPABLE_TYPE(Performance);
  void TraceMembers(script::Tracer* tracer) override;

  // Performance Timeline extensions to the Performance interface.
  PerformanceEntryList GetEntries();
  PerformanceEntryList GetEntriesByType(const std::string& entry_type);
  PerformanceEntryList GetEntriesByName(
      const std::string& name, const base::StringPiece& type);
  PerformanceEntryList GetEntriesByName(const std::string& name);

 private:
  scoped_refptr<PerformanceTiming> timing_;
  scoped_refptr<MemoryInfo> memory_;

  // https://www.w3.org/TR/hr-time-3/#dfn-time-origin
  base::TimeDelta time_origin_;

  PerformanceEntryBuffer performance_entry_buffer_;

  DISALLOW_COPY_AND_ASSIGN(Performance);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_H_
