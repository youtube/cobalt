// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/dom_task.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scheduler_post_task_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/core/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/core/scheduler/web_scheduling_task_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace blink {

namespace {

class TaskPromiseResolveHandler final
    : public ThenCallable<IDLAny, TaskPromiseResolveHandler> {
 public:
  explicit TaskPromiseResolveHandler(DOMTask* task) : task_(task) {}

  void Trace(Visitor* visitor) const override {
    ThenCallable<IDLAny, TaskPromiseResolveHandler>::Trace(visitor);
    visitor->Trace(task_);
  }

  void React(ScriptState*, ScriptValue) { task_->OnPendingPromiseSettled(); }

 private:
  Member<DOMTask> task_;
};

}  // namespace

DOMTask::DOMTask(ScriptPromiseResolver<IDLAny>* resolver,
                 V8SchedulerPostTaskCallback* callback,
                 AbortSignal* abort_source,
                 DOMTaskSignal* priority_source,
                 DOMScheduler::DOMTaskQueue* task_queue,
                 base::TimeDelta delay,
                 uint64_t task_id_for_tracing)
    : callback_(callback),
      resolver_(resolver),
      abort_source_(abort_source),
      priority_source_(priority_source),
      task_queue_(task_queue),
      delay_(delay),
      task_id_for_tracing_(task_id_for_tracing) {
  CHECK(task_queue_);
  CHECK(callback_);

  if (abort_source_ && abort_source_->CanAbort()) {
    abort_handle_ = abort_source_->AddAlgorithm(
        WTF::BindOnce(&DOMTask::OnAbort, WrapWeakPersistent(this)));
  }

  task_handle_ = PostDelayedCancellableTask(
      task_queue_->GetTaskRunner(), FROM_HERE,
      WTF::BindOnce(&DOMTask::Invoke, WrapPersistent(this)), delay);

  ScriptState* script_state =
      callback_->CallbackRelevantScriptStateOrReportError("DOMTask", "Create");
  DCHECK(script_state && script_state->ContextIsValid());

  if (script_state->World().IsMainWorld()) {
    if (auto* tracker = scheduler::TaskAttributionTracker::From(
            script_state->GetIsolate())) {
      parent_task_ = tracker->RunningTask();
    }
  }

  auto* context = ExecutionContext::From(script_state);
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "SchedulePostTaskCallback", inspector_scheduler_schedule_event::Data,
      context, task_id_for_tracing_, task_queue_->GetPriority(),
      delay_.InMillisecondsF());
  async_task_context_.Schedule(context, "postTask");
}

void DOMTask::Trace(Visitor* visitor) const {
  visitor->Trace(callback_);
  visitor->Trace(resolver_);
  visitor->Trace(abort_source_);
  visitor->Trace(priority_source_);
  visitor->Trace(abort_handle_);
  visitor->Trace(task_queue_);
  visitor->Trace(parent_task_);
}

void DOMTask::Invoke() {
  CHECK(callback_);
  CHECK_EQ(execution_state_, ExecutionState::kNotStarted);

  // Tasks are not runnable if the document associated with this task's
  // scheduler's global is not fully active, which happens if the
  // ExecutionContext is detached. Note that this context can be different
  // from the the callback's relevant context.
  ExecutionContext* scheduler_context = resolver_->GetExecutionContext();
  if (!scheduler_context || scheduler_context->IsContextDestroyed()) {
    RemoveAbortAlgorithm();
    execution_state_ = ExecutionState::kFinished;
    return;
  }

  ScriptState* script_state =
      callback_->CallbackRelevantScriptStateOrReportError("DOMTask", "Invoke");
  if (!script_state || !script_state->ContextIsValid()) {
    DCHECK(resolver_->GetExecutionContext() &&
           !resolver_->GetExecutionContext()->IsContextDestroyed());
    // The scheduler's context is still attached, but the task's callback's
    // relvant context is not. This happens, for example, if an attached main
    // frame's scheduler schedules a task that runs a callback defined in a
    // detached child frame. The callback's relvant context must be valid to run
    // the callback (enforced in the bindings layer). Since we can't run this
    // task, and therefore won't settle the associated promise, we need to clean
    // up the ScriptPromiseResolverBase since it is associated with a different
    // context.
    resolver_->Detach();
    RemoveAbortAlgorithm();
    execution_state_ = ExecutionState::kFinished;
    return;
  }

  InvokeInternal(script_state);
  if (execution_state_ == ExecutionState::kFinished) {
    RemoveAbortAlgorithm();
  }
  callback_.Release();
}

void DOMTask::InvokeInternal(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);
  v8::TryCatch try_catch(isolate);

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context);
  DEVTOOLS_TIMELINE_TRACE_EVENT(
      "RunPostTaskCallback", inspector_scheduler_run_event::Data, context,
      task_id_for_tracing_, task_queue_->GetPriority(),
      delay_.InMillisecondsF());
  probe::AsyncTask async_task(context, &async_task_context_);

  std::optional<scheduler::TaskAttributionTracker::TaskScope>
      task_attribution_scope;
  // For the main thread (tracker exists), create the task scope with the signal
  // to set up propagation. On workers, set the current context here since there
  // is no tracker.
  if (auto* tracker =
          scheduler::TaskAttributionTracker::From(script_state->GetIsolate())) {
    task_attribution_scope = tracker->CreateTaskScope(
        script_state, parent_task_,
        scheduler::TaskAttributionTracker::TaskScopeType::kSchedulerPostTask,
        abort_source_, priority_source_);
  } else if (RuntimeEnabledFeatures::SchedulerYieldEnabled(
                 ExecutionContext::From(script_state))) {
    auto* task_state = MakeGarbageCollected<WebSchedulingTaskState>(
        /*TaskAttributionInfo=*/nullptr, abort_source_, priority_source_);
    ScriptWrappableTaskState::SetCurrent(
        script_state,
        MakeGarbageCollected<ScriptWrappableTaskState>(task_state));
  }

  execution_state_ = ExecutionState::kRunningSync;
  ScriptValue result;
  ScriptPromise<IDLAny> pending_result;
  if (callback_->Invoke(nullptr).To(&result)) {
    v8::Local<v8::Value> v8_result = result.V8Value();
    if (v8_result->IsPromise()) {
      auto promise = v8_result.As<v8::Promise>();
      if (promise->State() == v8::Promise::PromiseState::kPending) {
        pending_result = ScriptPromise<IDLAny>::FromV8Promise(isolate, promise);
        auto* handler = MakeGarbageCollected<TaskPromiseResolveHandler>(this);
        pending_result.Then(script_state, handler, handler);
      }
    }
    resolver_->Resolve(result);
  } else if (try_catch.HasCaught()) {
    resolver_->Reject(try_catch.Exception());
  }
  execution_state_ = pending_result.IsEmpty() ? ExecutionState::kFinished
                                              : ExecutionState::kRunningAsync;
}

void DOMTask::OnAbort() {
  // If the task finished, `RemoveAbortAlgorithm()` should have been called.
  CHECK_NE(execution_state_, ExecutionState::kFinished);

  task_handle_.Cancel();
  async_task_context_.Cancel();

  DCHECK(resolver_);

  ScriptState* const resolver_script_state = resolver_->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                     resolver_script_state)) {
    return;
  }

  auto* context = ExecutionContext::From(resolver_script_state);
  CHECK(context);

  if (execution_state_ == ExecutionState::kRunningAsync) {
    UseCounter::Count(*context, WebFeature::kSchedulerPostTaskAsyncAbort);
    // The abort won't have an effect because the promise has already been
    // resolved.
    return;
  }
  UseCounter::Count(*context,
                    execution_state_ == ExecutionState::kRunningSync
                        ? WebFeature::kSchedulerPostTaskSelfAbort
                        : WebFeature::kSchedulerPostTaskAbortBeforeRunning);

  // Switch to the resolver's context to let DOMException pick up the resolver's
  // JS stack.
  ScriptState::Scope script_state_scope(resolver_script_state);

  DEVTOOLS_TIMELINE_TRACE_EVENT("AbortPostTaskCallback",
                                inspector_scheduler_abort_event::Data, context,
                                task_id_for_tracing_);

  // TODO(crbug.com/1293949): Add an error message.
  resolver_->Reject(abort_source_->reason(resolver_script_state)
                        .V8ValueFor(resolver_script_state));
}

void DOMTask::RemoveAbortAlgorithm() {
  if (abort_handle_) {
    CHECK(abort_source_);
    abort_source_->RemoveAlgorithm(abort_handle_);
    abort_handle_ = nullptr;
  }
}

void DOMTask::OnPendingPromiseSettled() {
  execution_state_ = ExecutionState::kFinished;
  RemoveAbortAlgorithm();
}

}  // namespace blink
