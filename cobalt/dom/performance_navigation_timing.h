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

#ifndef COBALT_DOM_PERFORMANCE_NAVIGATION_TIMING_H_
#define COBALT_DOM_PERFORMANCE_NAVIGATION_TIMING_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "cobalt/dom/navigation_type.h"
#include "cobalt/dom/performance_entry.h"
#include "cobalt/dom/performance_high_resolution_time.h"
#include "cobalt/dom/performance_resource_timing.h"

#include "cobalt/script/wrappable.h"
#include "net/base/load_timing_info.h"

namespace cobalt {
namespace dom {

class Document;
class Performance;

// Implements the Performance Navigation Timing IDL interface, as described
// here:
//   https://www.w3.org/TR/navigation-timing-2/
class PerformanceNavigationTiming : public PerformanceResourceTiming {
 public:
  PerformanceNavigationTiming(const net::LoadTimingInfo& timing_info,
                              const std::string& requested_url,
                              Performance* performance, Document* document,
                              base::TimeTicks time_origin);

  // Web API.
  DOMHighResTimeStamp unload_event_start() const;
  DOMHighResTimeStamp unload_event_end() const;
  DOMHighResTimeStamp dom_content_loaded_event_start() const;
  DOMHighResTimeStamp dom_content_loaded_event_end() const;
  DOMHighResTimeStamp dom_complete() const;
  DOMHighResTimeStamp load_event_start() const;
  DOMHighResTimeStamp load_event_end() const;
  NavigationType type() const;

  // PerformanceResourceTiming overrides.
  std::string initiator_type() const override;

  // PerformanceEntry overrides. Return a timestamp equal to the difference
  // between loadEventEnd and its startTime.
  DOMHighResTimeStamp duration() const override;

  std::string entry_type() const override { return "navigation"; }
  PerformanceEntryType EntryTypeEnum() const override {
    return PerformanceEntry::kNavigation;
  }

  DEFINE_WRAPPABLE_TYPE(PerformanceNavigationTiming);

 private:
  base::WeakPtr<Document> document_;
  base::TimeTicks time_origin_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceNavigationTiming);
};
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_NAVIGATION_TIMING_H_
