// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_

#include <memory>

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Event;

// Event timing collects and records the event start time, processing start time
// and processing end time of long-latency events, providing a tool to evaluate
// input latency.
// See also: https://github.com/wicg/event-timing
class CORE_EXPORT EventTiming final {
  USING_FAST_MALLOC(EventTiming);

 public:
  // Processes an event that will be dispatched. Notifies the
  // InteractiveDetector if it needs to be logged into input delay histograms.
  // Returns an object only if the event is relevant for the EventTiming API.
  // This object should be constructed before the event is dispatched and
  // destructed after dispatch so that we can calculate the input delay and
  // other latency values correctly.
  static std::unique_ptr<EventTiming> Create(LocalDOMWindow*, const Event&);

  explicit EventTiming(base::TimeTicks processing_start,
                       WindowPerformance* performance,
                       const Event& event);
  ~EventTiming();
  EventTiming(const EventTiming&) = delete;
  EventTiming& operator=(const EventTiming&) = delete;

  static void HandleInputDelay(LocalDOMWindow* window,
                               const Event& event,
                               base::TimeTicks processing_start);
  // The caller owns the |clock| which must outlive the EventTiming.
  static void SetTickClockForTesting(const base::TickClock* clock);

  // Returns true when the type of the event is included in the EventTiming.
  static bool IsEventTypeForEventTiming(const Event& event);

 private:
  // The time the first event handler or default action started to execute.
  base::TimeTicks processing_start_;
  // The event timestamp to be used in EventTiming and in histograms.
  base::TimeTicks event_timestamp_;

  Persistent<WindowPerformance> performance_;

  Persistent<const Event> event_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_
