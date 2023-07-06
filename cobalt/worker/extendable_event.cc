// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/worker/extendable_event.h"

namespace cobalt {
namespace worker {

void ExtendableEvent::OnEnvironmentSettingsChanged(bool context_valid) {
  if (!context_valid) {
    while (traced_global_promises_.size() > 0) {
      LessenLifetime(traced_global_promises_.begin()->first);
    }
  }
}

void ExtendableEvent::WaitUntil(
    script::EnvironmentSettings* settings,
    std::unique_ptr<script::Promise<script::ValueHandle*>>& promise,
    script::ExceptionState* exception_state) {
  // Algorithm for waitUntil(), to add lifetime promise to event.
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dom-extendableevent-waituntil

  // 1. If event’s isTrusted attribute is false, throw an "InvalidStateError"
  //    DOMException.
  // 2. If event is not active, throw an "InvalidStateError" DOMException.
  if (!IsActive()) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  // 3. Add promise to event’s extend lifetime promises.
  ExtendLifetime(promise.get());
  // 4. Increment event’s pending promises count by one.
  ++pending_promise_count_;
  // 5. Upon fulfillment or rejection of promise, queue a microtask to run
  //    these substeps:
  std::unique_ptr<base::OnceCallback<void()>> callback(
      new base::OnceCallback<void()>(std::move(
          base::BindOnce(&ExtendableEvent::StateChange, base::Unretained(this),
                         settings, promise.get()))));
  promise->AddStateChangeCallback(std::move(callback));
  promise.release();
}

void ExtendableEvent::StateChange(
    script::EnvironmentSettings* settings,
    const script::Promise<script::ValueHandle*>* promise) {
  // Implement the microtask called upon fulfillment or rejection of a
  // promise, as part of the algorithm for waitUntil().
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dom-extendableevent-waituntil
  DCHECK(promise);
  has_rejected_promise_ |= promise->State() == script::PromiseState::kRejected;
  // 5.1. Decrement event’s pending promises count by one.
  --pending_promise_count_;
  // 5.2. If event’s pending promises count is 0, then:
  if (0 == pending_promise_count_) {
    if (done_callback_) {
      std::move(done_callback_).Run(has_rejected_promise_);
    }
    web::Context* context =
        base::polymorphic_downcast<web::EnvironmentSettings*>(settings)
            ->context();
    ServiceWorkerContext* worker_context = context->service_worker_context();
    DCHECK(worker_context);
    // 5.2.1. Let registration be the current global object's associated
    //        service worker's containing service worker registration.
    worker_context->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ServiceWorkerContext::WaitUntilSubSteps,
            base::Unretained(worker_context),
            base::Unretained(context->GetWindowOrWorkerGlobalScope()
                                 ->AsServiceWorker()
                                 ->service_worker_object()
                                 ->containing_service_worker_registration())));
  }
  LessenLifetime(promise);
  delete promise;
}

void ExtendableEvent::ExtendLifetime(
    const script::Promise<script::ValueHandle*>* promise) {
  auto heap_tracer =
      script::v8c::V8cEngine::GetFromIsolate(isolate_)->heap_tracer();
  if (traced_global_promises_.size() == 0) {
    heap_tracer->AddRoot(this);
  }
  auto* traced_global =
      new v8::TracedGlobal<v8::Value>(isolate_, promise->promise());
  traced_global_promises_[promise] = traced_global;
  heap_tracer->AddRoot(traced_global);
}

void ExtendableEvent::LessenLifetime(
    const script::Promise<script::ValueHandle*>* promise) {
  auto heap_tracer =
      script::v8c::V8cEngine::GetFromIsolate(isolate_)->heap_tracer();
  if (traced_global_promises_.count(promise) == 1) {
    auto* traced_global = traced_global_promises_[promise];
    heap_tracer->RemoveRoot(traced_global);
    traced_global_promises_.erase(promise);
    delete traced_global;
  }
  if (traced_global_promises_.size() == 0) {
    heap_tracer->RemoveRoot(this);
  }
}

void ExtendableEvent::InitializeEnvironmentSettingsChangeObserver(
    script::EnvironmentSettings* settings) {
  web::Context* context = web::get_context(settings);
  context->AddEnvironmentSettingsChangeObserver(this);
  remove_environment_settings_change_observer_ =
      base::BindOnce(&web::Context::RemoveEnvironmentSettingsChangeObserver,
                     base::Unretained(context), base::Unretained(this));
}

}  // namespace worker
}  // namespace cobalt
