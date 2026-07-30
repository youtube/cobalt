# Swarm Cognitive Principles: Bottleneck Detection & Flow Analysis

This document defines the core reasoning principles used by the **SQL Trace
Analyzer Agent (SQL-TAA)** to identify, evaluate, and prioritize performance
bottlenecks in Chromium traces. These rules ensure the agent applies deep
architectural judgment rather than relying on naive duration sorting.

______________________________________________________________________

## Principle 1: Tiered Flow Analysis & Structural Redundancy

We do **not** ignore a method simply because its "self-time" is zero or
near-zero. If a high-level method (e.g., `RevertAll`, `Navigate`) has a
significant total duration (e.g., $> 20\\text{ ms}$ or $> 10%$ of the focus
slice), we must treat its entire subtree as a **Unified Performance Flow**.

When analyzing a Flow, the agent must look for three types of inefficiencies:

1. **Leaf Bottlenecks**: Atomic methods within the flow that are slow.
2. **Internal Gaps (Black Boxes)**: Methods within the flow that have high
   self-time (missing instrumentation).
3. **Structural Redundancy (Sequence Inefficiency)**: The same sub-operation
   being called multiple times across different branches of the *same* flow.
   This indicates that the flow's internal logic is triggering redundant
   layouts, updates, or lookups.

______________________________________________________________________

## Principle 2: The Absolute Self-Time Override (Locating Black Boxes)

While relative overhead ($V\_{ro} = \\text{dur}(Slice) / \\text{dur}(Root)$) is
useful for ranking, **absolute UI-thread blocking time is what directly impacts
the user experience.**

- **The Override Rule**: Regardless of the relative percentage or any static
  scoring formula, if a method has an **absolute uninstrumented self-time of $>
  5.0\\text{ ms}$ on the main UI thread (`CrBrowserMain`)**, it **MUST** be
  flagged as a bottleneck and a **Tracer Gap (Partial/Absolute Black Box)**.
- **Why?**: 5.0ms is a substantial portion of a single frame budget (16.6ms for
  60fps). Any silent block of this size is a candidate for instrumentation.

### Example: `OmniboxViewViews::SetWindowTextAndCaretPos`

In the `OpenMatch` trace, this method appears as:

```
* [  18.983 ms (  7.3%) | self:   12.719 ms (  4.9%)] OmniboxViewViews::SetWindowTextAndCaretPos
```

- **Analysis**: Its relative overhead is small (only 4.9% of the root), and a
  static script scoring threshold might filter it out.
- **Override Application**: Its absolute uninstrumented self-time is
  **`12.72 ms`** on the UI thread. This is a massive silent block that blocks
  the UI thread for almost a full frame.
- **Diagnosis**: Trivial UI updates like setting text should take $< 0.5\\text{
  ms}$. A 12.7ms gap indicates heavy, untraced operations under the hood (e.g.,
  complex text elision, font metrics calculation, or accessibility tree
  updates).
- **Action**: Flag immediately for `GAP_INSTRUMENTATION`.

______________________________________________________________________

## Principle 3: Semantic Simplicity vs. Time (Semantic Mismatch)

The agent must evaluate the **semantic intent** of a method name against its
**actual cost** (self-time or leaf duration).

- **The Mismatch Rule**: If a method's name implies a simple, low-overhead
  operation (e.g., `UpdateIcon`, `GetPrefs`, `RecordMetrics`, `DidStartLoading`,
  `IsReady`, `NotifyObservers`), but it takes $> 1.0\\text{ ms}$ (Release) or $>
  2.0\\text{ ms}$ (Developer/Debug builds):
  - This is a **Semantic Mismatch**.
  - It indicates the method is either performing heavy blocking work (e.g.,
    synchronous disk I/O or UI layout calculations on the main thread) or is
    missing internal traces for heavy helper functions.
  - *Action*: High-priority flag for instrumentation or refactoring.

______________________________________________________________________

## Principle 4: Low-Level vs. High-Level Redundancy

Redundancy must be analyzed at two different tiers of the architecture:

### A. Low-Level Utility Redundancy

- **Definition**: Lightweight utility or lookup methods called hundreds or
  thousands of times (e.g., `KeyedServiceFactory::GetServiceForContext` called
  1,196 times, taking `23.7 ms` cumulatively).
- **Optimization Strategy**: Recommend **Local Caching**. The calling classes
  should cache the retrieved pointer in a member variable instead of repeatedly
  querying the global context factory.

### B. High-Level Flow Redundancy

- **Definition**: Heavy, complex architectural flows called multiple times ($N
  \\ge 2$) across the overall trace (e.g., `RevertAll` called 3 times globally,
  costing `111.5 ms`).
- **Optimization Strategy**: Recommend **Call-Site Coordination**. The
  Orchestrator/Codebase Agent must inspect why the parent event triggers are
  firing repeatedly (e.g., redundant event listeners or lack of state-change
  filtering) and batch them.

______________________________________________________________________

## Principle 5: Aggregated vs. Single-Instance Analysis (Chronic vs. Acute Latency)

When analyzing user journeys containing multiple occurrences of the focus
entrypoint slice, the agent must evaluate latency using both aggregated and
single-instance perspectives:

### A. Acute Latency (Single-Instance Spikes)

- **Characteristics**: Large, conditional blocks of time (e.g.
  `RenderProcessHostImpl::Init` taking 12ms but called only 1 time across 8
  navigations).
- **Impact**: Causes a single, highly noticeable frame drop or navigation
  freeze.
- **Optimization Strategy**: Usually requires moving the heavy operation off the
  critical path entirely (e.g. deferring via task runners or running
  asynchronously).

### B. Chronic Latency (Aggregated Overhead)

- **Characteristics**: Medium-sized overheads called consistently on *every*
  invocation of the focus slice (e.g. `ShouldSwapBrowsingInstancesForNavigation`
  called 8 times, taking 1.7ms average per call, totaling 13.5ms cumulative).
- **Impact**: Steadily degrades performance across the entire user journey.
- **Optimization Strategy**:
  1. Apply the **Absolute Self-Time Override** (Principle 2) to the **cumulative
     self-time** of aggregated nodes.
  2. If the call count matches the number of target slice invocations, check for
     uninstrumented helper loops or heavy calculations that can be cached,
     optimized, or skipped when the state has not changed.

______________________________________________________________________

## Principle 6: Browser Logic Focus vs. Infrastructure Noise (Causation vs. Correlation)

When profiling user journeys and performance regressions, the primary goal is to
optimize **browser application logic** rather than debugging or modifying the
system's scheduling infrastructure (e.g., ThreadPool scheduling, Mojo IPC
internals, Tracing overhead).

Generic scheduling and waiting slices often dominate duration deltas in reports,
creating noise. The agent must apply the following guidelines to focus on
browser logic:

### A. Ignore Infrastructure Noise on the First Pass

In the first run, ignore generic infrastructure slices representing scheduling,
waiting, or tracing mechanics:

- **Task Schedulers**: `WorkerThread active`, `ThreadPool_RunTask`,
  `ThreadController active`, `ThreadControllerImpl::RunTask`.
- **IPC/Mojo Mechanics**: `Receive mojo::...`, `Closed mojo endpoint`,
  `FusePipes`, `Connector::Accept`.
- **Tracing/Profiling Overhead**: `StartupTracingController::Start`,
  `perfetto::...`, `TracingControllerImpl::...`.
- **Synchronous Wait Primitives**: `ScopedBlockingCall`, `WaitableEvent::Wait`,
  `ScopedAllowBaseSyncPrimitives...`.

### B. Perform Careful Analysis if Investigating Infrastructure

If you must work on an infrastructure bottleneck, perform a careful validation
(e.g. comparing the bottleneck's presence vs. absence across multiple individual
runs) to confirm whether the block is a stable cause of the regression or merely
statistical noise.

### C. Focus on Browser Application Logic Categories

Direct your primary investigation to application logic under these key
categories:

- **Initialization & Startup**: Thread/Process creation, profile initialization,
  windowing, UI setup.
- **Navigation & Loading**: URL request dispatch, network fetching, parsing,
  script execution.
- **Layout & Rendering**: Paint invalidation, style calculation, layout,
  compositing, rasterization.

### D. Final Report Classification

In the final analysis report, all infrastructure-related findings or
recommendations **MUST** be presented as **Optional/Secondary**. Focus the
primary recommendations on browser application logic.
