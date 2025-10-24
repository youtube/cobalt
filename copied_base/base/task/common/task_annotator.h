// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_COMMON_TASK_ANNOTATOR_H_
#define BASE_TASK_COMMON_TASK_ANNOTATOR_H_

#include <stdint.h>

#include "base/auto_reset.h"
#include "base/base_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/pending_task.h"
#include "base/strings/string_piece.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/base_tracing.h"

namespace base {

// Constant used to measure which long-running tasks should be traced.
constexpr TimeDelta kMaxTaskDurationTimeDelta = Milliseconds(4);

// Implements common debug annotations for posted tasks. This includes data
// such as task origins, IPC message contexts, queueing durations and memory
// usage.
class BASE_EXPORT TaskAnnotator {
 public:
  class ObserverForTesting {
   public:
    // Invoked just before RunTask() in the scope in which the task is about to
    // be executed.
    virtual void BeforeRunTask(const PendingTask* pending_task) = 0;
  };

  // This is used to set the |ipc_hash| field for PendingTasks. It is intended
  // to be used only from within generated IPC handler dispatch code.
  class ScopedSetIpcHash;

  // This is used to track long-running browser-UI tasks. It is intended to
  // be used for low-overhead logging to produce longer traces, particularly to
  // help the scroll jank reduction effort.
  class LongTaskTracker;

  static const PendingTask* CurrentTaskForThread();

  static void OnIPCReceived(const char* interface_name,
                            uint32_t (*method_info)(),
                            bool is_response);

  static void MarkCurrentTaskAsInterestingForTracing();

  TaskAnnotator();

  TaskAnnotator(const TaskAnnotator&) = delete;
  TaskAnnotator& operator=(const TaskAnnotator&) = delete;

  ~TaskAnnotator();

  // Called to indicate that a task is about to be queued to run in the future,
  // giving one last chance for this TaskAnnotator to add metadata to
  // |pending_task| before it is moved into the queue.
  void WillQueueTask(perfetto::StaticString trace_event_name,
                     PendingTask* pending_task);

  // Creates a process-wide unique ID to represent this task in trace events.
  // This will be mangled with a Process ID hash to reduce the likelyhood of
  // colliding with TaskAnnotator pointers on other processes. Callers may use
  // this when generating their own flow events (i.e. when passing
  // |queue_function == nullptr| in above methods).
  uint64_t GetTaskTraceID(const PendingTask& task) const;

  // Run the given task, emitting the toplevel trace event and additional
  // trace event arguments. Like for TRACE_EVENT macros, all of the arguments
  // are used (i.e. lambdas are invoked) before this function exits, so it's
  // safe to pass reference-capturing lambdas here.
  template <typename... Args>
  void RunTask(perfetto::StaticString event_name,
               PendingTask& pending_task,
               Args&&... args) {
    TRACE_EVENT(
        "toplevel", event_name,
        [&](perfetto::EventContext& ctx) {
          EmitTaskLocation(ctx, pending_task);
          MaybeEmitIncomingTaskFlow(ctx, pending_task);
          MaybeEmitIPCHashAndDelay(ctx, pending_task);
        },
        std::forward<Args>(args)...);
    RunTaskImpl(pending_task);
  }

 private:
  friend class TaskAnnotatorBacktraceIntegrationTest;

  // Run a previously queued task.
  NOT_TAIL_CALLED void RunTaskImpl(PendingTask& pending_task);

  // Registers an ObserverForTesting that will be invoked by all TaskAnnotators'
  // RunTask(). This registration and the implementation of BeforeRunTask() are
  // responsible to ensure thread-safety.
  static void RegisterObserverForTesting(ObserverForTesting* observer);
  static void ClearObserverForTesting();

#if BUILDFLAG(ENABLE_BASE_TRACING)
  // TRACE_EVENT argument helper, writing the task location data into
  // EventContext.
  static void EmitTaskLocation(perfetto::EventContext& ctx,
                               const PendingTask& task);

  // TRACE_EVENT argument helper, writing the incoming task flow information
  // into EventContext if toplevel.flow category is enabled.
  void MaybeEmitIncomingTaskFlow(perfetto::EventContext& ctx,
                                 const PendingTask& task) const;

  void MaybeEmitIPCHashAndDelay(perfetto::EventContext& ctx,
                                const PendingTask& task) const;
#endif  //  BUILDFLAG(ENABLE_BASE_TRACING)
};

class BASE_EXPORT [[maybe_unused, nodiscard]] TaskAnnotator::ScopedSetIpcHash {
 public:
  explicit ScopedSetIpcHash(uint32_t ipc_hash);

  // Compile-time-const string identifying the current IPC context. Not always
  // available due to binary size constraints, so IPC hash might be set instead.
  explicit ScopedSetIpcHash(const char* ipc_interface_name);

  ScopedSetIpcHash(const ScopedSetIpcHash&) = delete;
  ScopedSetIpcHash& operator=(const ScopedSetIpcHash&) = delete;

  ~ScopedSetIpcHash();

  uint32_t GetIpcHash() const { return ipc_hash_; }
  const char* GetIpcInterfaceName() const { return ipc_interface_name_; }

  static uint32_t MD5HashMetricName(base::StringPiece name);

 private:
  ScopedSetIpcHash(uint32_t ipc_hash, const char* ipc_interface_name);

  const AutoReset<ScopedSetIpcHash*> resetter_;
  uint32_t ipc_hash_;
  const char* ipc_interface_name_;
};

class BASE_EXPORT [[maybe_unused, nodiscard]] TaskAnnotator::LongTaskTracker {
 public:
  explicit LongTaskTracker(const TickClock* tick_clock,
                           PendingTask& pending_task,
                           TaskAnnotator* task_annotator);

  LongTaskTracker(const LongTaskTracker&) = delete;

  ~LongTaskTracker();

  void SetIpcDetails(const char* interface_name,
                     uint32_t (*method_info)(),
                     bool is_response);

  void MaybeTraceInterestingTaskDetails();

  // In long-task tracking, not every task (including its queue time) will be
  // recorded in a trace. If a particular task + queue time needs to be
  // recorded, flag it explicitly. For example, input tasks are required for
  // calculating scroll jank metrics.
  bool is_interesting_task = false;

 private:
  void EmitReceivedIPCDetails(perfetto::EventContext& ctx);

  const AutoReset<LongTaskTracker*> resetter_;

  // For tracking task duration
  raw_ptr<const TickClock> tick_clock_;  // Not owned.
  TimeTicks task_start_time_;
  TimeTicks task_end_time_;

  // Tracing variables.

  // Use this to ensure that tracing and NowTicks() are not called
  // unnecessarily.
  bool is_tracing_;
  const char* ipc_interface_name_ = nullptr;
  uint32_t ipc_hash_ = 0;

  // IPC method info to retrieve IPC hash and method address from trace, if
  // known. Note that this will not compile in the Native client.
  uint32_t (*ipc_method_info_)();
  bool is_response_ = false;
  [[maybe_unused]] const raw_ref<PendingTask> pending_task_;
  [[maybe_unused]] raw_ptr<TaskAnnotator> task_annotator_;
};

}  // namespace base

#endif  // BASE_TASK_COMMON_TASK_ANNOTATOR_H_
