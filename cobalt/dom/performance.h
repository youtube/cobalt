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

#include <deque>
#include <list>
#include <map>
#include <string>

#include "base/time/default_tick_clock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/application_state.h"
#include "cobalt/base/clock.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/performance_entry_list_impl.h"
#include "cobalt/dom/performance_high_resolution_time.h"
#include "cobalt/dom/performance_lifecycle_timing.h"
#include "cobalt/dom/performance_observer.h"
#include "cobalt/dom/performance_observer_callback_options.h"
#include "cobalt/dom/performance_observer_init.h"
#include "cobalt/dom/performance_resource_timing.h"
#include "cobalt/dom/performance_timing.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "net/base/load_timing_info.h"

namespace cobalt {
namespace dom {

class MemoryInfo;

// Implements the Performance IDL interface, an instance of which is created
// and owned by the Window object.
//   https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html#sec-window.performance-attribute
class Performance : public EventTarget {
 public:
  // Ensure that the timer resolution is at the lowest 20 microseconds in
  // order to mitigate potential Spectre-related attacks.  This is following
  // Mozilla's lead as described here:
  //   https://www.mozilla.org/en-US/security/advisories/mfsa2018-01/
  // NOLINT(runtime/int)
  static constexpr int64_t kPerformanceTimerMinResolutionInMicroseconds = 20;
  //   https://www.w3.org/TR/2021/WD-resource-timing-2-20210414/#sec-extensions-performance-interface
  static constexpr unsigned long  // NOLINT(runtime/int)
      kMaxResourceTimingBufferSize = 250;

  Performance(script::EnvironmentSettings* settings,
              const scoped_refptr<base::BasicClock>& clock);

  // Web API: Performance
  //   https://www.w3.org/TR/hr-time-2/#sec-performance
  scoped_refptr<PerformanceTiming> timing() const;
  scoped_refptr<MemoryInfo> memory() const;
  DOMHighResTimeStamp Now() const;
  DOMHighResTimeStamp time_origin() const;
  DOMHighResTimeStamp MonotonicTimeToDOMHighResTimeStamp(
      base::TimeTicks monotonic_time) const;

  static DOMHighResTimeStamp MonotonicTimeToDOMHighResTimeStamp(
      base::TimeTicks time_origin,
      base::TimeTicks monotonic_time);

  // Web API: Performance Timeline extensions to the Performance.
  //   https://www.w3.org/TR/performance-timeline-2/#extensions-to-the-performance-interface
  PerformanceEntryList GetEntries();
  PerformanceEntryList GetEntriesByType(const std::string& entry_type);
  PerformanceEntryList GetEntriesByName(const std::string& name,
                                        const base::StringPiece& type);

  // Web API: User Timing extensions to Performance
  //   https://www.w3.org/TR/2019/REC-user-timing-2-20190226/#extensions-performance-interface
  void Mark(const std::string& mark_name,
            script::ExceptionState* exception_state);
  void ClearMarks(const std::string& mark_name);
  void Measure(const std::string& measure_name, const std::string& start_mark,
               const std::string& end_mark,
               script::ExceptionState* exception_state);
  void ClearMeasures(const std::string& measure_name);

  // Web API: Performance Resource Timing extensions to Performance.
  //   https://w3c.github.io/resource-timing/#sec-extensions-performance-interface
  void ClearResourceTimings();
  // NOLINT(runtime/int)
  void SetResourceTimingBufferSize(
      unsigned long max_size);  // NOLINT(runtime/int)
  bool CanAddResourceTimingEntry();
  void CopySecondaryBuffer();
  void set_onresourcetimingbufferfull(
      const EventTarget::EventListenerScriptValue& event_listener);
  const EventTarget::EventListenerScriptValue* onresourcetimingbufferfull()
      const;
  void FireResourceTimingBufferFullEvent();
  void AddPerformanceResourceTimingEntry(
      const scoped_refptr<PerformanceResourceTiming>& resource_timing_entry);
  void QueuePerformanceEntry(const scoped_refptr<PerformanceEntry>& entry);
  void QueuePerformanceTimelineTask();
  void CreatePerformanceResourceTiming(const net::LoadTimingInfo& timing_info,
                                       const std::string& initiator_type,
                                       const std::string& requested_url);
  void CreatePerformanceLifecycleTiming();
  // Custom, not in any spec.
  // Internal getter method for the time origin value.
  base::TimeTicks GetTimeOrigin() const {
      return time_origin_;
  }
  // Register and unregisterthe performance observer.
  void UnregisterPerformanceObserver(
      const scoped_refptr<PerformanceObserver>& observer);
  void RegisterPerformanceObserver(
      const scoped_refptr<PerformanceObserver>& observer,
      const PerformanceObserverInit& options);

  // Replace and update registered performance observer options list.
  void ReplaceRegisteredPerformanceObserverOptionsList(
      const scoped_refptr<PerformanceObserver>& observer,
      const PerformanceObserverInit& options);
  void UpdateRegisteredPerformanceObserverOptionsList(
      const scoped_refptr<PerformanceObserver>& observer,
      const PerformanceObserverInit& options);

  void SetApplicationState(base::ApplicationState state,
                           SbTimeMonotonic timestamp);

  void SetApplicationStartOrPreloadTimestamp(
      bool is_preload, SbTimeMonotonic timestamp);

  void SetDeepLinkTimestamp(SbTimeMonotonic timestamp);

  void TraceMembers(script::Tracer* tracer) override;
  DEFINE_WRAPPABLE_TYPE(Performance);

 private:
  unsigned long GetDroppedEntriesCount(const std::string& entry_type);

  base::TimeTicks time_origin_;
  const base::TickClock* tick_clock_;
  scoped_refptr<PerformanceTiming> timing_;
  scoped_refptr<MemoryInfo> memory_;
  scoped_refptr<PerformanceLifecycleTiming> lifecycle_timing_;
  base::TimeDelta unix_at_zero_monotonic_;

  PerformanceEntryList performance_entry_buffer_;
  struct RegisteredPerformanceObserver : public script::Traceable {
    RegisteredPerformanceObserver(
        const scoped_refptr<PerformanceObserver>& observer_init,
        const std::list<PerformanceObserverInit>& options_list_init)
        : observer(observer_init), options_list(options_list_init) {}

    void TraceMembers(script::Tracer* tracer) override {
      tracer->Trace(observer);
    }

    scoped_refptr<PerformanceObserver> observer;
    std::list<PerformanceObserverInit> options_list;
  };

  typedef std::list<RegisteredPerformanceObserver>
      RegisteredPerformanceObserverList;
  RegisteredPerformanceObserverList registered_performance_observers_;

  unsigned long resource_timing_buffer_size_limit_;
  unsigned long resource_timing_buffer_current_size_;
  bool resource_timing_buffer_full_event_pending_flag_;
  unsigned long
      resource_timing_secondary_buffer_current_size_;
  std::deque<scoped_refptr<PerformanceResourceTiming>>
      resource_timing_secondary_buffer_;

  bool performance_observer_task_queued_flag_;
  bool add_to_performance_entry_buffer_flag_;

  DISALLOW_COPY_AND_ASSIGN(Performance);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_H_
