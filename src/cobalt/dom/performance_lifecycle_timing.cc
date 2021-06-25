// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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


#include "cobalt/dom/performance_lifecycle_timing.h"
#include "cobalt/dom/performance.h"

namespace cobalt {
namespace dom {

PerformanceLifecycleTiming::PerformanceLifecycleTiming(
    const std::string& name, DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp end_time)
    : PerformanceEntry(name, start_time, end_time),
    current_state_("unstarted"),
    last_state_("unstarted") {}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_start() const {
  return PerformanceEntry::start_time();
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_blur() const {
  return PerformanceEntry::start_time();
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_focus() const {
  return PerformanceEntry::start_time();
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_conceal() const {
  return PerformanceEntry::start_time();
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_reveal() const {
  return PerformanceEntry::start_time();
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_freeze() const {
  return PerformanceEntry::start_time();
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_unfreeze() const {
  return PerformanceEntry::start_time();
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_stop() const {
  return PerformanceEntry::start_time();
}

std::string PerformanceLifecycleTiming::current_state() const {
  return current_state_;
}

std::string PerformanceLifecycleTiming::last_state() const {
  return last_state_;
}

}  // namespace dom
}  // namespace cobalt