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

#include "cobalt/base/clock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/performance_entry_list_impl.h"
#include "cobalt/dom/performance_high_resolution_time.h"
#include "cobalt/dom/performance_observer.h"
#include "cobalt/dom/performance_observer_callback_options.h"
#include "cobalt/dom/performance_observer_init.h"
#include "cobalt/dom/performance_resource_timing.h"
#include "cobalt/dom/performance_timing.h"
#include "cobalt/script/environment_settings.h"

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
  static constexpr int64_t kPerformanceTimerMinResolutionInMicroseconds = 20;
  //   https://www.w3.org/TR/2021/WD-resource-timing-2-20210414/#sec-extensions-performance-interface
  static constexpr unsigned long kMaxResourceTimingBufferSize = 250;

  Performance(script::EnvironmentSettings* settings,
              const scoped_refptr<base::BasicClock>& clock);
  ~Performance();

  // Web API: Performance
  //   https://www.w3.org/TR/hr-time-2/#sec-performance
  scoped_refptr<PerformanceTiming> timing() const;
  scoped_refptr<MemoryInfo> memory() const;
  DOMHighResTimeStamp Now() const;
  DOMHighResTimeStamp time_origin() const;

  // Web API: Performance Timeline extensions to the Performance.
  //   https://www.w3.org/TR/performance-timeline-2/#extensions-to-the-performance-interface
  PerformanceEntryList GetEntries();
  PerformanceEntryList GetEntriesByType(const std::string& entry_type);
  PerformanceEntryList GetEntriesByName(const std::string& name,
                                        const base::StringPiece& type);

  // Web API: Performance Resource Timing extensions to Performance.
  //   https://w3c.github.io/resource-timing/#sec-extensions-performance-interface
  void ClearResourceTimings();
  void SetResourceTimingBufferSize(unsigned long max_size);
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
                                       const std::string& requested_url,
                                       const std::string& cache_mode);
  // Custom, not in any spec.
  base::TimeDelta get_time_origin() const { return time_origin_; }

  // Register and unregisterthe performance observer.
  void UnregisterPerformanceObserver(PerformanceObserver* old_observer);
  void RegisterPerformanceObserver(PerformanceObserver* observer,
                                   const PerformanceObserverInit& options);

  // Replace and update registered performance observer options list.
  void ReplaceRegisteredPerformanceObserverOptionsList(
      PerformanceObserver* observer, const PerformanceObserverInit& options);
  void UpdateRegisteredPerformanceObserverOptionsList(
      PerformanceObserver* observer, const PerformanceObserverInit& options);

  void TraceMembers(script::Tracer* tracer) override;
  DEFINE_WRAPPABLE_TYPE(Performance);

 private:
  unsigned long GetDroppedEntriesCount(const std::string& entry_type);

  scoped_refptr<PerformanceTiming> timing_;
  scoped_refptr<MemoryInfo> memory_;
  base::TimeDelta time_origin_;

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
  unsigned long resource_timing_secondary_buffer_current_size_;
  std::deque<scoped_refptr<PerformanceResourceTiming>>
      resource_timing_secondary_buffer_;

  bool performance_observer_task_queued_flag_;
  bool add_to_performance_entry_buffer_flag_;

  // The thread created and owned by this Performance.
  // All queuing functions should be run on this thread.
  base::Thread thread_;

  // Specifies the priority of the Performance queuing thread.
  // This is the thread that is responsible for queuing PerformanceEntries.
  // This thread should have lower priority(BACKGROUND).
  base::ThreadPriority thread_priority = base::ThreadPriority::BACKGROUND;

  // The message loop this object is running on.
  base::MessageLoop* message_loop() const { return thread_.message_loop(); }

  DISALLOW_COPY_AND_ASSIGN(Performance);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_H_
