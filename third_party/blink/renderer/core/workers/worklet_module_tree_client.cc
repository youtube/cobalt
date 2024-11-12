// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worklet_module_tree_client.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

WorkletModuleTreeClient::WorkletModuleTreeClient(
    ScriptState* script_state,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks)
    : script_state_(script_state),
      outside_settings_task_runner_(std::move(outside_settings_task_runner)),
      pending_tasks_(pending_tasks) {}

// Implementation of the second half of the "fetch and invoke a worklet script"
// algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
void WorkletModuleTreeClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  // TODO(nhiroki): Call reporting proxy functions appropriately (e.g.,
  // DidFailToFetchModuleScript(), WillEvaluateModuleScript()).

  // "Note: Specifically, if a script fails to parse or fails to load over the
  // network, it will reject the promise. If the script throws an error while
  // first evaluating the promise it will resolve as classes may have been
  // registered correctly."
  // https://drafts.css-houdini.org/worklets/#fetch-a-worklet-script
  //
  // When a network failure happens, |module_script| should be nullptr, and the
  // case will be handled by the step 3.
  // When a parse failure happens, |module_script| has an error to rethrow, and
  // the case will be handled by the step 4.

  // Step 3: "If script is null, then queue a task on outsideSettings's
  // responsible event loop to run these steps:"
  if (!module_script) {
    // Null |error_to_rethrow| will be replaced with AbortError.
    PostCrossThreadTask(
        *outside_settings_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&WorkletPendingTasks::Abort,
                            WrapCrossThreadPersistent(pending_tasks_.Get()),
                            /*error_to_rethrow=*/nullptr));
    return;
  }

  // Step 4: "If script's error to rethrow is not null, then queue a task on
  // outsideSettings's responsible event loop given script's error to rethrow to
  // run these steps:
  ScriptState::Scope scope(script_state_);
  if (module_script->HasErrorToRethrow()) {
    // TODO(crbug.com/1204965): SerializedScriptValue always assumes that the
    // default microtask queue is used, so we have to put an explicit scope on
    // the stack here. Ideally, all V8 bindings would understand non-default
    // microtask queues.
    v8::MicrotasksScope microtasks_scope(
        script_state_->GetIsolate(), ToMicrotaskQueue(script_state_),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    PostCrossThreadTask(
        *outside_settings_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &WorkletPendingTasks::Abort,
            WrapCrossThreadPersistent(pending_tasks_.Get()),
            SerializedScriptValue::SerializeAndSwallowExceptions(
                script_state_->GetIsolate(),
                module_script->CreateErrorToRethrow().V8Value())));
    return;
  }

  // Step 5: "Run a module script given script."
  ScriptEvaluationResult result =
      module_script->RunScriptOnScriptStateAndReturnValue(script_state_);

  auto* global_scope =
      To<WorkletGlobalScope>(ExecutionContext::From(script_state_));

  global_scope->ReportingProxy().DidEvaluateTopLevelScript(
      result.GetResultType() == ScriptEvaluationResult::ResultType::kSuccess);

  // Step 6: "Queue a task on outsideSettings's responsible event loop to run
  // these steps:"
  // The steps are implemented in WorkletPendingTasks::DecrementCounter().
  PostCrossThreadTask(
      *outside_settings_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WorkletPendingTasks::DecrementCounter,
                          WrapCrossThreadPersistent(pending_tasks_.Get())));
}

void WorkletModuleTreeClient::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace blink
