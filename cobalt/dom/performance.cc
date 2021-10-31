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

#include <string>

#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "cobalt/browser/stack_size_constants.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/memory_info.h"
#include "cobalt/dom/performance_entry.h"
#include "cobalt/dom/performance_mark.h"
#include "cobalt/dom/performance_measure.h"

namespace cobalt {
namespace dom {

namespace {

base::TimeDelta GetUnixAtZeroMonotonic(const base::Clock* clock,
                                       const base::TickClock* tick_clock) {
  base::TimeDelta unix_time_now = clock->Now() - base::Time::UnixEpoch();
  base::TimeDelta time_since_origin = tick_clock->NowTicks().since_origin();
  return unix_time_now - time_since_origin;
}

bool IsNamePerformanceTimingAttribute(const std::string& name) {
  return name == "navigationStart";
}

DOMHighResTimeStamp ConvertNameToTimestamp(
    const std::string& name, script::ExceptionState* exception_state) {
  // The algorithm of ConvertNameToTimestamp() follows these steps:
  //   https://www.w3.org/TR/user-timing/#convert-a-name-to-a-timestamp
  // 1. If the global object is not a Window object, throw a SyntaxError.
  // 2. If name is navigationStart, return 0.
  if (name == "navigationStart") {
    return 0.0;
  }

  // 3. Let startTime be the value of navigationStart in the PerformanceTiming
  // interface.
  // 4. Let endTime be the value of name in the PerformanceTiming interface.
  // 5. If endTime is 0, throw an InvalidAccessError.
  // 6. Return result of subtracting startTime from endTime.

  // Note that we only support navigationStart in the PerformanceTiming
  // interface. We return 0.0 instead of the result of subtracting
  // startTime from endTime.
  dom::DOMException::Raise(dom::DOMException::kSyntaxErr,
                           "Cannot convert a name that is not a public "
                           "attribute of PerformanceTiming to a timestamp",
                           exception_state);
  return 0.0;
}

}  // namespace

Performance::Performance(script::EnvironmentSettings* settings,
                         const scoped_refptr<base::BasicClock>& clock)
    : EventTarget(settings),
      time_origin_(base::TimeTicks::Now()),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      timing_(new PerformanceTiming(clock, time_origin_)),
      memory_(new MemoryInfo()),
      resource_timing_buffer_size_limit_(
          Performance::kMaxResourceTimingBufferSize),
      resource_timing_buffer_current_size_(0),
      resource_timing_buffer_full_event_pending_flag_(false),
      resource_timing_secondary_buffer_current_size_(0),
      performance_observer_task_queued_flag_(false),
      add_to_performance_entry_buffer_flag_(false) {
  unix_at_zero_monotonic_ =
      GetUnixAtZeroMonotonic(base::DefaultClock::GetInstance(), tick_clock_);
  lifecycle_timing_ = base::MakeRefCounted<PerformanceLifecycleTiming>(
      "lifecycle timing", time_origin());
  // Queue lifecycle timing.
  QueuePerformanceEntry(lifecycle_timing_);
  // Add lifecycle timing to the performance entry buffer.
  performance_entry_buffer_.push_back(lifecycle_timing_);
}

// static
DOMHighResTimeStamp Performance::MonotonicTimeToDOMHighResTimeStamp(
    base::TimeTicks time_origin, base::TimeTicks monotonic_time) {
  if (monotonic_time.is_null() || time_origin.is_null()) return 0.0;
  DOMHighResTimeStamp clamped_time =
      ClampTimeStampMinimumResolution(
          monotonic_time,
          Performance::kPerformanceTimerMinResolutionInMicroseconds) -
      ClampTimeStampMinimumResolution(
          time_origin,
          Performance::kPerformanceTimerMinResolutionInMicroseconds);

  return clamped_time;
}

DOMHighResTimeStamp Performance::MonotonicTimeToDOMHighResTimeStamp(
    base::TimeTicks monotonic_time) const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(time_origin_,
                                                         monotonic_time);
}

DOMHighResTimeStamp Performance::Now() const {
  // Now stores the current high resolution time.
  //   https://www.w3.org/TR/2019/REC-hr-time-2-20191121/#dfn-current-high-resolution-time
  return MonotonicTimeToDOMHighResTimeStamp(tick_clock_->NowTicks());
}

scoped_refptr<PerformanceTiming> Performance::timing() const { return timing_; }

scoped_refptr<MemoryInfo> Performance::memory() const { return memory_; }

DOMHighResTimeStamp Performance::time_origin() const {
  // The algorithm for calculating time origin timestamp.
  //   https://www.w3.org/TR/2019/REC-hr-time-2-20191121/#dfn-time-origin-timestamp
  // Assert that global's time origin is not undefined.
  DCHECK(!time_origin_.is_null());

  // Let t1 be the DOMHighResTimeStamp representing the high resolution
  // time at which the global monotonic clock is zero.
  base::TimeDelta t1 = unix_at_zero_monotonic_;

  // Let t2 be the DOMHighResTimeStamp representing the high resolution
  // time value of the global monotonic clock at global's time origin.
  base::TimeDelta t2 = time_origin_ - base::TimeTicks();

  // Return the sum of t1 and t2.
  return ClampTimeStampMinimumResolution(
      t1 + t2, Performance::kPerformanceTimerMinResolutionInMicroseconds);
}

void Performance::Mark(const std::string& mark_name,
                       script::ExceptionState* exception_state) {
  // The algorithm for mark() follows these steps:
  //   https://www.w3.org/TR/2019/REC-user-timing-2-20190226/#mark-method
  // 1. If the global object is a Window object and markName uses the same name
  // as a read only attribute in the PerformanceTiming interface, throw a
  // SyntaxError.
  if (IsNamePerformanceTimingAttribute(mark_name)) {
    dom::DOMException::Raise(
        dom::DOMException::kSyntaxErr,
        "Cannot create a mark with the same name as a read-only attribute in "
        "the PerformanceTiming interface",
        exception_state);
  }

  // 2. Create a new PerformanceMark object (entry).
  // 3. Set entry's name attribute to markName.
  // 4. Set entry's entryType attribute to DOMString "mark".
  // 5. Set entry's startTime attribute to the value that would be returned by
  // the Performance object's now() method.
  // 6. Set entry's duration attribute to 0.
  scoped_refptr<PerformanceMark> entry =
      base::MakeRefCounted<PerformanceMark>(mark_name, Now());

  // 7. Queue entry.
  QueuePerformanceEntry(entry);

  // 8. Add entry to the performance entry buffer.
  performance_entry_buffer_.push_back(entry);

  // 9. Return undefined
}

void Performance::ClearMarks(const std::string& mark_name) {
  // The algorithm for clearMarks follows these steps:
  //   https://www.w3.org/TR/2019/REC-user-timing-2-20190226/#clearmarks-method
  // 1. If markName is omitted, remove all PerformanceMark objects from the
  // performance entry buffer.
  // 2. Otherwise, remove all PerformanceMark objects listed in the performance
  // entry buffer whose name matches markName.
  PerformanceEntryList retained_performance_entry_buffer;
  for (const auto& entry : performance_entry_buffer_) {
    bool should_remove_entry =
        PerformanceEntry::ToEntryTypeEnum(entry->entry_type()) ==
            PerformanceEntry::kMark &&
        (mark_name.empty() || entry->name() == mark_name);
    if (!should_remove_entry) {
      retained_performance_entry_buffer.push_back(entry);
    }
  }
  performance_entry_buffer_.swap(retained_performance_entry_buffer);

  // 3. Return undefined.
}

void Performance::Measure(const std::string& measure_name,
                          const std::string& start_mark,
                          const std::string& end_mark,
                          script::ExceptionState* exception_state) {
  // The algorithm for measure() follows these steps:
  //   https://www.w3.org/TR/2019/REC-user-timing-2-20190226/#measure-method
  // 1. Let end time be 0.
  DOMHighResTimeStamp end_time = 0.0;

  // 2. If endMark is omitted, let end time be the value that would be returned
  // by the Performance object's now() method.
  if (end_mark.empty()) {
    end_time = Now();
  } else if (IsNamePerformanceTimingAttribute(end_mark)) {
    // 2.1. Otherwise, if endMark has the same name as a read only attribute in
    // the PerformanceTiming interface, let end time be the value returned by
    // running the convert a name to a timestamp algorithm with name set to the
    // value of endMark.
    end_time = ConvertNameToTimestamp(end_mark, exception_state);
  } else {
    // 2.2. Otherwise let end time be the value of the startTime attribute from
    // the most recent occurrence of a PerformanceMark object in the performance
    // entry buffer whose name matches the value of endMark. If no matching
    // entry is found, throw a SyntaxError.
    PerformanceEntryList list = GetEntriesByName(end_mark, "mark");
    if (list.empty()) {
      dom::DOMException::Raise(
          dom::DOMException::kSyntaxErr,
          "Cannot create measure; no mark found with name: " + end_mark + ".",
          exception_state);
      return;
    }
    end_time = list.at(list.size() - 1)->start_time();
  }

  DOMHighResTimeStamp start_time;
  // 3. If startMark is omitted, let start time be 0.
  if (start_mark.empty()) {
    start_time = 0.0;
  } else if (IsNamePerformanceTimingAttribute(start_mark)) {
    // 3.1. If startMark has the same name as a read only attribute in the
    // PerformanceTiming interface, let start time be the value returned by
    // running the convert a name to a timestamp algorithm with name set to
    // startMark.
    start_time = ConvertNameToTimestamp(start_mark, exception_state);
  } else {
    // 3.2. Otherwise let start time be the value of the startTime attribute
    // from the most recent occurrence of a PerformanceMark object in the
    // performance entry buffer whose name matches the value of startMark. If no
    // matching entry is found, throw a SyntaxError.
    PerformanceEntryList list = GetEntriesByName(start_mark, "mark");
    if (list.empty()) {
      dom::DOMException::Raise(
          dom::DOMException::kSyntaxErr,
          "Cannot create measure; no mark found with name: " + start_mark + ".",
          exception_state);
      return;
    }
    start_time = list.at(list.size() - 1)->start_time();
  }

  // 4. Create a new PerformanceMeasure object (entry).
  // 5. Set entry's name attribute to measureName.
  // 6. Set entry's entryType attribute to DOMString "measure".
  // 7. Set entry's startTime attribute to start time.
  // 8. Set entry's duration attribute to the duration from start time to end
  // time. The resulting duration value MAY be negative.
  scoped_refptr<PerformanceMeasure> entry =
      base::MakeRefCounted<PerformanceMeasure>(measure_name, start_time,
                                               end_time);

  // 9. Queue entry.
  QueuePerformanceEntry(entry);

  // 10. Add entry to the performance entry buffer.
  performance_entry_buffer_.push_back(entry);

  // 11. Return undefined.
}

void Performance::ClearMeasures(const std::string& measure_name) {
  // The algorithm for clearMeasures follows these steps:
  //   https://www.w3.org/TR/2019/REC-user-timing-2-20190226/#clearmeasures-method
  // 1. If measureName is omitted, remove all PerformanceMeasure objects in the
  // performance entry buffer.
  // 2. Otherwise remove all PerformanceMeasure objects listed in the
  // performance entry buffer whose name matches measureName.
  PerformanceEntryList performance_entry_buffer;
  for (const auto& entry : performance_entry_buffer_) {
    bool shouldRemoveEntry =
        PerformanceEntry::ToEntryTypeEnum(entry->entry_type()) ==
            PerformanceEntry::kMeasure &&
        (measure_name.empty() || entry->name() == measure_name);
    if (!shouldRemoveEntry) {
      performance_entry_buffer.push_back(entry);
    }
  }
  performance_entry_buffer_.swap(performance_entry_buffer);

  // 3. Return undefined.
}

void Performance::UnregisterPerformanceObserver(
    const scoped_refptr<PerformanceObserver>& old_observer) {
  auto iter = registered_performance_observers_.begin();
  while (iter != registered_performance_observers_.end()) {
    if (iter->observer == old_observer) {
      iter = registered_performance_observers_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void Performance::RegisterPerformanceObserver(
    const scoped_refptr<PerformanceObserver>& observer,
    const PerformanceObserverInit& options) {
  std::list<PerformanceObserverInit> options_list;
  options_list.push_back(options);
  registered_performance_observers_.emplace_back(observer, options_list);
}

void Performance::ReplaceRegisteredPerformanceObserverOptionsList(
    const scoped_refptr<PerformanceObserver>& observer,
    const PerformanceObserverInit& options) {
  auto iter = registered_performance_observers_.begin();
  while (iter != registered_performance_observers_.end()) {
    if (iter->observer == observer) {
      iter->options_list.clear();
      iter->options_list.push_back(options);
    }
    ++iter;
  }
}

void Performance::UpdateRegisteredPerformanceObserverOptionsList(
    const scoped_refptr<PerformanceObserver>& observer,
    const PerformanceObserverInit& options) {
  auto iter = registered_performance_observers_.begin();
  while (iter != registered_performance_observers_.end()) {
    if (iter->observer == observer) {
      bool is_replaced = false;
      for (auto& registered_options : iter->options_list) {
        if (registered_options.type() == options.type()) {
          registered_options = options;
          is_replaced = true;
        }
      }
      if (!is_replaced) iter->options_list.push_back(options);
    }
    ++iter;
  }
}

void Performance::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(timing_);
  tracer->Trace(memory_);
  tracer->Trace(lifecycle_timing_);
}

PerformanceEntryList Performance::GetEntries() {
  return PerformanceEntryListImpl::GetEntries(performance_entry_buffer_);
}

PerformanceEntryList Performance::GetEntriesByType(
    const std::string& entry_type) {
  return PerformanceEntryListImpl::GetEntriesByType(performance_entry_buffer_,
                                                    entry_type);
}

PerformanceEntryList Performance::GetEntriesByName(
    const std::string& name, const base::StringPiece& type) {
  return PerformanceEntryListImpl::GetEntriesByName(performance_entry_buffer_,
                                                    name, type);
}

void Performance::ClearResourceTimings() {
  // The method clearResourceTimings runs the following steps:
  //   https://www.w3.org/TR/2021/WD-resource-timing-2-20210414/#dom-performance-clearresourcetimings
  // 1. Remove all PerformanceResourceTiming objects in the performance
  // entry buffer.
  PerformanceEntryList performance_entry_buffer;
  for (const auto& entry : performance_entry_buffer_) {
    bool should_be_removed =
        PerformanceEntry::ToEntryTypeEnum(entry->entry_type()) ==
        PerformanceEntry::kResource;
    if (!should_be_removed) {
      performance_entry_buffer.push_back(entry);
    }
  }
  performance_entry_buffer_.swap(performance_entry_buffer);

  // 2. Set resource timing buffer current size to 0.
  resource_timing_buffer_current_size_ = 0;
}

void Performance::SetResourceTimingBufferSize(
    unsigned long max_size) {  // NOLINT(runtime/int)
  // The method runs the following steps:
  //   https://www.w3.org/TR/2021/WD-resource-timing-2-20210414/#dom-performance-setresourcetimingbuffersize
  // 1. Set resource timing buffer size limit to the maxSize parameter.
  // If the maxSize parameter is less than resource timing buffer current
  // size, no PerformanceResourceTiming objects are to be removed from
  // the performance entry buffer.
  resource_timing_buffer_size_limit_ = max_size;
}

bool Performance::CanAddResourceTimingEntry() {
  // THe method runs the following steps:
  //   https://www.w3.org/TR/2021/WD-resource-timing-2-20210414/#dfn-can-add-resource-timing-entry
  // 1. If resource timing buffer current size is smaller than resource
  // timing buffer size limit, return true.
  // 2. Return false.
  return resource_timing_buffer_current_size_ <
         resource_timing_buffer_size_limit_;
}

void Performance::CopySecondaryBuffer() {
  //   https://www.w3.org/TR/2021/WD-resource-timing-2-20210414/#dfn-copy-secondary-buffer
  // While resource timing secondary buffer is not empty and can add
  // resource timing entry returns true, run the following substeps:
  PerformanceEntryList entry_list;
  while (!resource_timing_secondary_buffer_.empty() &&
         CanAddResourceTimingEntry()) {
    // 1. Let entry be the oldest PerformanceResourceTiming in resource timing
    // secondary buffer.
    scoped_refptr<PerformanceResourceTiming> entry =
        resource_timing_secondary_buffer_.front();
    // 2. Add entry to the end of performance entry buffer.
    DCHECK(entry);
    performance_entry_buffer_.push_back(entry);
    // 3. Increment resource timing buffer current size by 1.
    resource_timing_buffer_current_size_++;
    // 4. Remove entry from resource timing secondary buffer.
    resource_timing_secondary_buffer_.pop_front();
    // 5. Decrement resource timing secondary buffer current size by 1.
    resource_timing_secondary_buffer_current_size_--;
  }
}

void Performance::set_onresourcetimingbufferfull(
    const EventTarget::EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::resourcetimingbufferfull(),
                            event_listener);
}

const EventTarget::EventListenerScriptValue*
Performance::onresourcetimingbufferfull() const {
  return GetAttributeEventListener(base::Tokens::resourcetimingbufferfull());
}

void Performance::FireResourceTimingBufferFullEvent() {
  //   https://www.w3.org/TR/2021/WD-resource-timing-2-20210414/#dfn-fire-a-buffer-full-event
  // 1. While resource timing secondary buffer is not empty, run the
  // following substeps:
  while (!resource_timing_secondary_buffer_.empty()) {
    // 1.1 Let number of excess entries before be resource timing secondary
    // buffer current size.
    unsigned long excess_entries_before =  // NOLINT(runtime/int)
        resource_timing_secondary_buffer_current_size_;
    // 1.2 If can add resource timing entry returns false, then fire an event
    // named resourcetimingbufferfull at the Performance object.
    if (!CanAddResourceTimingEntry()) {
      DispatchEvent(new Event(base::Tokens::resourcetimingbufferfull()));
    }
    // 1.3 Run copy secondary buffer.
    CopySecondaryBuffer();
    // 1.4 Let number of excess entries after be resource timing secondary
    // buffer current size.
    unsigned long excess_entries_after =  // NOLINT(runtime/int)
        resource_timing_secondary_buffer_current_size_;
    // 1.5 If number of excess entries before is lower than or equals number of
    // excess entries after, then remove all entries from resource timing
    // secondary buffer, set resource timing secondary buffer current size to 0,
    // and abort these steps.
    if (excess_entries_before <= excess_entries_after) {
      resource_timing_secondary_buffer_.clear();
      resource_timing_secondary_buffer_current_size_ = 0;
      break;
    }
  }

  // 2. Set resource timing buffer full event pending flag to false.
  resource_timing_buffer_full_event_pending_flag_ = false;
}

void Performance::AddPerformanceResourceTimingEntry(
    const scoped_refptr<PerformanceResourceTiming>& resource_timing_entry) {
  //   https://www.w3.org/TR/2021/WD-resource-timing-2-20210414/#dfn-add-a-performanceresourcetiming-entry
  // To add a PerformanceResourceTiming entry into the performance entry buffer,
  // run the following steps:
  // 1. Let new entry be the input PerformanceEntry to be added.
  // 2. If can add resource timing entry returns true and resource timing buffer
  // full event pending flag is false, run the following substeps:
  if (CanAddResourceTimingEntry() &&
      !resource_timing_buffer_full_event_pending_flag_) {
    // 2.1 Add new entry to the performance entry buffer.
    performance_entry_buffer_.push_back(resource_timing_entry);
    // 2.2 Increase resource timing buffer current size by 1.
    resource_timing_buffer_current_size_++;
    // 2.3 Return.
    return;
  }

  // 3. If resource timing buffer full event pending flag is false, run the
  // following substeps:
  if (!resource_timing_buffer_full_event_pending_flag_) {
    // 3.1 Set resource timing buffer full event pending flag to true.
    resource_timing_buffer_full_event_pending_flag_ = true;
    // 3.2 Queue a task on the performance timeline task source to run fire
    // a buffer full event.
    FireResourceTimingBufferFullEvent();
  }
  // 4. Add new entry to the resource timing secondary buffer.
  resource_timing_secondary_buffer_.push_back(resource_timing_entry);
  // 5. Increase resource timing secondary buffer current size by 1.
  resource_timing_secondary_buffer_current_size_++;
  DCHECK_EQ(resource_timing_secondary_buffer_current_size_,
            resource_timing_secondary_buffer_.size());
}

void Performance::QueuePerformanceEntry(
    const scoped_refptr<PerformanceEntry>& entry) {
  // To queue a PerformanceEntry (new entry) with an optional add to
  // performance entry buffer flag, which is unset by default, run these steps:
  //   https://www.w3.org/TR/2019/WD-performance-timeline-2-20191024/#queue-a-performanceentry
  // 1. Let interested observers be an initially empty set of
  // PerformanceObserver objects.
  std::list<scoped_refptr<PerformanceObserver>> interested_observers;
  // 2. Let entryType be new entry’s entryType value.
  const std::string entry_type = entry->entry_type();
  // 3. For each registered performance observer (regObs):
  for (const auto& reg_obs : registered_performance_observers_) {
    // 3.1 If regObs's options list contains a PerformanceObserverInit item
    // whose entryTypes member include entryType or whose type member equals to
    // entryType, append regObs's observer to interested observers.
    for (const auto& item : reg_obs.options_list) {
      if (item.has_type() && item.type() == entry_type) {
        interested_observers.push_back(reg_obs.observer);
      }
      if (item.has_entry_types()) {
        for (const auto& type : item.entry_types()) {
          if (type == entry_type) {
            interested_observers.push_back(reg_obs.observer);
          }
        }
      }
    }
  }
  // 4. For each observer in interested observers:
  for (const auto& observer : interested_observers) {
    // 4.1 Append new entry to observer's observer buffer.
    observer->EnqueuePerformanceEntry(entry);
  }
  // 5. If the add to performance entry buffer flag is set, add new entry to the
  // performance entry buffer.
  if (add_to_performance_entry_buffer_flag_) {
    performance_entry_buffer_.push_back(entry);
  }
  // 6. If the performance observer task queued flag is set, terminate these
  // steps.
  if (performance_observer_task_queued_flag_) return;
  // 7. Set performance observer task queued flag.
  performance_observer_task_queued_flag_ = true;
  // 8. Queue a task that consists of running the following substeps.
  // The task source for the queued task is the performance timeline task
  // source.
  QueuePerformanceTimelineTask();
}

void Performance::QueuePerformanceTimelineTask() {
  // 8.1 Unset performance observer task queued flag for the relevant global
  // object.
  performance_observer_task_queued_flag_ = false;
  // 8.2 Let notify list be a copy of relevant global object's list of
  // registered performance observer objects.
  RegisteredPerformanceObserverList notify_list =
      registered_performance_observers_;
  // 8.3 For each PerformanceObserver object po in notify list, run these steps:
  for (const auto& reg_obs : notify_list) {
    // 8.3.1 Let entries be a copy of po’s observer buffer.
    scoped_refptr<PerformanceObserver> po = reg_obs.observer;
    PerformanceEntryList entries = po->GetObserverBuffer();
    // 8.3.2 Empty po’s observer buffer.
    po->EmptyObserverBuffer();
    // If entries is non-empty, call po’s callback with entries as first
    // argument and po as the second argument and callback this value. If this
    // throws an exception, report the exception.
    if (!entries.empty()) {
      scoped_refptr<PerformanceObserverEntryList> observer_entry_list(
          new PerformanceObserverEntryList(entries));
      po->GetPerformanceObserverCallback()->RunCallback(observer_entry_list,
                                                        po);
    }
  }
}

void Performance::CreatePerformanceResourceTiming(
    const net::LoadTimingInfo& timing_info, const std::string& initiator_type,
    const std::string& requested_url) {
  // To mark resource timing given a fetch timing info timingInfo, a DOMString
  // requestedURL, a DOMString initiatorType a global object global, and a
  // string cacheMode, perform the following steps:
  //   https://www.w3.org/TR/2021/WD-resource-timing-2-20210414/#marking-resource-timing
  // 1. Create a PerformanceResourceTiming object entry in global's realm.
  // 2.Setup the resource timing entry for entry, given initiatorType,
  // requestedURL, timingInfo, and cacheMode.
  scoped_refptr<PerformanceResourceTiming> resource_timing(
      new PerformanceResourceTiming(timing_info, initiator_type, requested_url,
                                    this, time_origin_));
  // 2. Queue entry.
  QueuePerformanceEntry(resource_timing);
  // 3. Add entry to global's performance entry buffer.
  AddPerformanceResourceTimingEntry(resource_timing);
}

void Performance::SetApplicationState(base::ApplicationState state,
                                      SbTimeMonotonic timestamp) {
  lifecycle_timing_->SetApplicationState(state, timestamp);
}

void Performance::SetApplicationStartOrPreloadTimestamp(
    bool is_preload, SbTimeMonotonic timestamp) {
  lifecycle_timing_->SetApplicationStartOrPreloadTimestamp(is_preload,
                                                           timestamp);
}

void Performance::SetDeepLinkTimestamp(SbTimeMonotonic timestamp) {
  lifecycle_timing_->SetDeepLinkTimestamp(timestamp);
}

}  // namespace dom
}  // namespace cobalt
