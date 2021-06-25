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

namespace {

  std::string TranslateApplicationStateToString(
      base::ApplicationState state) {
    switch (state) {
      case base::kApplicationStateBlurred:
        return "ApplicationStateBlurred";
      case base::kApplicationStateConcealed:
        return "ApplicationStateConcealed";
      case base::kApplicationStateFrozen:
        return "ApplicationStateFrozen";
      case base::kApplicationStateStarted:
        return "ApplicationStateStarted";
      case base::kApplicationStateStopped:
        return "ApplicationStateStopped";
    }

    NOTREACHED() << "state = " << state;
    return "INVALID_APPLICATION_STATE";
  }

  DOMHighResTimeStamp ConvertSbTimeMonotonicToDOMHiResTimeStamp(
      base::TimeTicks time_origin, SbTimeMonotonic monotonic_time) {
    SbTimeMonotonic time_delta = SbTimeGetNow() - SbTimeGetMonotonicNow();
    base::TimeTicks time_ticks =
        base::TimeTicks::FromInternalValue(time_delta + monotonic_time);
    return Performance::MonotonicTimeToDOMHighResTimeStamp(
        time_origin, time_ticks);
  }

}  // namespace

PerformanceLifecycleTiming::PerformanceLifecycleTiming(
    const std::string& name, base::TimeTicks time_origin)
    : PerformanceEntry(name, 0.0, 0.0), time_origin_(time_origin) {}


DOMHighResTimeStamp PerformanceLifecycleTiming::app_preload() const {
  return ReportDOMHighResTimeStamp(lifecycle_timing_info_.app_preload);
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_start() const {
  return ReportDOMHighResTimeStamp(lifecycle_timing_info_.app_start);
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_blur() const {
  return ReportDOMHighResTimeStamp(lifecycle_timing_info_.app_blur);
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_focus() const {
  return ReportDOMHighResTimeStamp(lifecycle_timing_info_.app_focus);
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_conceal() const {
  return ReportDOMHighResTimeStamp(lifecycle_timing_info_.app_conceal);
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_reveal() const {
  return ReportDOMHighResTimeStamp(lifecycle_timing_info_.app_reveal);
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_freeze() const {
  return ReportDOMHighResTimeStamp(lifecycle_timing_info_.app_freeze);
}

DOMHighResTimeStamp PerformanceLifecycleTiming::app_unfreeze() const {
  return ReportDOMHighResTimeStamp(lifecycle_timing_info_.app_unfreeze);
}

std::string PerformanceLifecycleTiming::current_state() const {
  return TranslateApplicationStateToString(
      lifecycle_timing_info_.current_state);
}

std::string PerformanceLifecycleTiming::last_state() const {
  return TranslateApplicationStateToString(
      lifecycle_timing_info_.last_state);
}

void PerformanceLifecycleTiming::SetApplicationState(
    base::ApplicationState state, SbTimeMonotonic timestamp) {
  switch (state) {
    case base::kApplicationStateBlurred:
      if (GetLastState() == base::kApplicationStateStarted) {
        lifecycle_timing_info_.app_blur = timestamp;
      } else if (GetLastState() == base::kApplicationStateConcealed) {
        lifecycle_timing_info_.app_reveal = timestamp;
      }
      break;
    case base::kApplicationStateConcealed:
      if (GetLastState() == base::kApplicationStateBlurred) {
        lifecycle_timing_info_.app_conceal = timestamp;
      } else if (GetLastState() == base::kApplicationStateFrozen) {
        lifecycle_timing_info_.app_unfreeze = timestamp;
      } else {
        NOTREACHED() << "Invalid application state transition.";
      }
      break;
    case base::kApplicationStateFrozen:
      lifecycle_timing_info_.app_freeze = timestamp;
      break;
    case base::kApplicationStateStarted:
      if (GetLastState() == base::kApplicationStateBlurred) {
        if (lifecycle_timing_info_.app_preload != 0) {
          lifecycle_timing_info_.app_start = timestamp;
        }
        lifecycle_timing_info_.app_focus = timestamp;
      }
      break;
    case base::kApplicationStateStopped:
      NOTREACHED() << "Not support the application stopped state.";
      break;
    default:
      NOTREACHED() << "Invalid application state = " << state;
      break;
  }
  SetLifecycleTimingInfoState(state);
}

base::ApplicationState PerformanceLifecycleTiming::GetLastState() const {
  return lifecycle_timing_info_.last_state;
}

void PerformanceLifecycleTiming::SetLifecycleTimingInfoState(
    base::ApplicationState state) {
  lifecycle_timing_info_.last_state =
      lifecycle_timing_info_.current_state;
  lifecycle_timing_info_.current_state = state;
}

DOMHighResTimeStamp
    PerformanceLifecycleTiming::ReportDOMHighResTimeStamp(
        SbTimeMonotonic timestamp) const {
  if (timestamp != 0) {
    return ConvertSbTimeMonotonicToDOMHiResTimeStamp(
        time_origin_, timestamp);
  }
  return PerformanceEntry::start_time();
}

}  // namespace dom
}  // namespace cobalt