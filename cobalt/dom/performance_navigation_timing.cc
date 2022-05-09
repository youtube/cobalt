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

#include "cobalt/dom/performance_navigation_timing.h"
#include "cobalt/dom/performance.h"

#include "cobalt/dom/document.h"

namespace cobalt {
namespace dom {

namespace {
//  https://www.w3.org/TR/resource-timing-2/#dom-performanceresourcetiming-initiatortype
const char kPerformanceNavigationInitiatorType[] = "navigation";
}  // namespace

PerformanceNavigationTiming::PerformanceNavigationTiming(
    const net::LoadTimingInfo& timing_info, const std::string& requested_url,
    Performance* performance, Document* document, base::TimeTicks time_origin)
    : PerformanceResourceTiming(timing_info,
                                kPerformanceNavigationInitiatorType,
                                requested_url, performance, time_origin),
      document_(base::AsWeakPtr(document)),
      time_origin_(time_origin) {}

std::string PerformanceNavigationTiming::initiator_type() const {
  return kPerformanceNavigationInitiatorType;
}

DOMHighResTimeStamp PerformanceNavigationTiming::unload_event_start() const {
  base::TimeTicks start_time = document_->GetDocumentUnloadEventStartTime();
  if (start_time.is_null()) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(time_origin_,
                                                         start_time);
}

DOMHighResTimeStamp PerformanceNavigationTiming::unload_event_end() const {
  base::TimeTicks end_time = document_->GetDocumentUnloadEventEndTime();
  if (end_time.is_null()) {
    return 0.0;
  }

  return Performance::MonotonicTimeToDOMHighResTimeStamp(time_origin_,
                                                         end_time);
}

DOMHighResTimeStamp
PerformanceNavigationTiming::dom_content_loaded_event_start() const {
  return document_->GetDocumentContentLoadedEventStartTime();
}

DOMHighResTimeStamp PerformanceNavigationTiming::dom_content_loaded_event_end()
    const {
  return document_->GetDocumentContentLoadedEventEndTime();
}

DOMHighResTimeStamp PerformanceNavigationTiming::dom_complete() const {
  return document_->GetDocumentDomCompleteTime();
}

DOMHighResTimeStamp PerformanceNavigationTiming::load_event_start() const {
  return document_->GetDocumentLoadEventStartTime();
}

DOMHighResTimeStamp PerformanceNavigationTiming::load_event_end() const {
  return document_->GetDocumentLoadEventEndTime();
}

NavigationType PerformanceNavigationTiming::type() const {
  return document_->GetNavigationType();
}

DOMHighResTimeStamp PerformanceNavigationTiming::duration() const {
  return load_event_end() - start_time();
}

}  // namespace dom
}  // namespace cobalt
