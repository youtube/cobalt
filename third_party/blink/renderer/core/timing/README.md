# renderer/core/timing

[Live version](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/timing/README.md)

The `renderer/core/timing` directory contains files related to various web performance APIs.

# High Resolution Time

The [HR-Time](https://w3c.github.io/hr-time/) specification introduces the Performance interface, which exposes a high
resolution monotonic clock via [performance.now()](https://w3c.github.io/hr-time/#now-method) and enables comparing timestamps
across various contexts via [performance.timeOrigin](https://w3c.github.io/hr-time/#timeorigin-attribute). This interface is
exposed on both Windows and Workers and implemented on the [Performance](./performance.h) file, with window and worker specific
code on [WindowPerformance](./window_performance.h) and (WorkerPerformance)(./worker_performance.h), respectively. The Performance
interface is tied to its DOM via [DOMWindowPerformance](./dom_window_performance.h) and
[WorkerGlobalScopePerformance](./worker_global_scope_performance.h).

Any new high resolution timestamps exposed through the web need to be set to the correct resolution to prevent privacy or security
issues, and hence should go through the MonotonicTimeTo* methods in [Performance](./performance.h), which will delegate the
rounding details to the [TimeClamper](./time_clamper.h).

# Performance Timeline

The [Performance-Timeline](https://w3c.github.io/performance-timeline/) specification enables exposing additional performance
measurements via two methods: directly to the Performance, which can be polled via getter methods, or to a PerformanceObserver,
which runs a callback upon receiving new entries. The latter interface is implemented in
[PerformanceObserver](./performance_observer.h), and is the recommended way to query entries. A single nugget of performance
information is encapsulated in a PerformanceEntry, which is the `base interface`. The type of the PerformanceEntry is codified
in its `entryType` attribute. Newer types of performance entries may not support polling queries via the Performance object,
whereas all must support callback queries via PerformanceObserver. This information is the `availableFromTimeline` bit in the
[registry](https://w3c.github.io/timing-entrytypes-registry/#registry). This registry also contains some additional useful
information that is entry-type specific. Note that all of the interfaces listed in that table have corresponding files in this
folder. While the objects exposing performance measurements via JS are concentrated in this folder, many of the computations
are located in other folders. For instance, the [Resource-Timing](https://w3c.github.io/resource-timing/) specification
measures timing on fetches, and that timing needs to occur at the specific locations in code where the fetching occurs and then
plumbed all the way to this folder.

# First Input Timing

The [PerformanceEventTiming](https://w3c.github.io/event-timing/) interface
provides timing information about the latency of the first discrete user
interaction, specifically one of `keydown`, `mousedown`, `click`, a
`pointerdown` followed by a `pointerup`. (Pointer down may be the start of
scrolling, which is not tracked.) This is a subset of the EventTiming API, and
provides key metrics to help measure and optimize the first impression on
responsiveness of web users.
[FirstInputStateMachine](First_input_state_machine.md) visualizes the state
machine logic of how the first input event timing entry got dispatched from a
pipeline of performance event entries.

# Event Timing - Interactions

The [PerformanceEventTiming](https://w3c.github.io/event-timing/) interface
exposes timing information for each non-continuous
event([fullList](https://w3c.github.io/event-timing/#sec-events-exposed)).
Certain events can be further grouped up as interactions by assigning the same
non-trivial [interactionId](https://www.w3.org/TR/2022/WD-event-timing-20220524/#dom-performanceeventtiming-interactionid).
Others will have interactionId with value 0. The purpose of defining an
interaction is to group events that fire during the same logical user gesture,
so further analysis like [INP](https://web.dev/inp/) can be done to better reflect the page
responsiveness on user interactions.

[PointerInteractionStateMachine](Pointer_interaction_state_machine.md)
visualizes the state machine logic of how the pointer related events
(`pointerdown`, `pointerup`, `pointercancel`, `click`) get grouped up as a
single interaction and get dispatched from the event timing pipeline.
