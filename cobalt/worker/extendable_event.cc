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
    ServiceWorkerJobs* jobs = context->service_worker_jobs();
    DCHECK(jobs);
    // 5.2.1. Let registration be the current global object's associated
    //        service worker's containing service worker registration.
    jobs->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ServiceWorkerJobs::WaitUntilSubSteps, base::Unretained(jobs),
            base::Unretained(context->GetWindowOrWorkerGlobalScope()
                                 ->AsServiceWorker()
                                 ->service_worker_object()
                                 ->containing_service_worker_registration())));
  }
  delete promise;
}

}  // namespace worker
}  // namespace cobalt
