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

#ifndef COBALT_WORKER_EXTENDABLE_EVENT_H_
#define COBALT_WORKER_EXTENDABLE_EVENT_H_

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "cobalt/base/token.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/v8c/native_promise.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/event.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "cobalt/worker/extendable_event_init.h"
#include "cobalt/worker/service_worker_global_scope.h"
#include "cobalt/worker/service_worker_jobs.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/service_worker_registration_object.h"

namespace cobalt {
namespace worker {

class ExtendableEvent : public web::Event {
 public:
  explicit ExtendableEvent(const std::string& type) : Event(type) {}
  explicit ExtendableEvent(base::Token type) : Event(type) {}
  ExtendableEvent(const std::string& type, const ExtendableEventInit& init_dict)
      : Event(type, init_dict) {}

  void WaitUntil(
      script::EnvironmentSettings* settings,
      std::unique_ptr<script::Promise<script::ValueHandle*>>& promise,
      script::ExceptionState* exception_state) {
    // Algorithm for waitUntil(), to add lifetime promise to event.
    //   https://w3c.github.io/ServiceWorker/#dom-extendableevent-waituntil

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
            base::BindOnce(&ExtendableEvent::StateChange,
                           base::Unretained(this), settings, promise.get()))));
    promise->AddStateChangeCallback(std::move(callback));
    promise.release();
  }

  void StateChange(script::EnvironmentSettings* settings,
                   const script::Promise<script::ValueHandle*>* promise) {
    // Implement the microtask called upon fulfillment or rejection of a
    // promise, as part of the algorithm for waitUntil().
    //   https://w3c.github.io/ServiceWorker/#dom-extendableevent-waituntil
    DCHECK(promise);
    has_rejected_promise_ |=
        promise->State() == script::PromiseState::kRejected;
    // 5.1. Decrement event’s pending promises count by one.
    --pending_promise_count_;
    // 5.2. If event’s pending promises count is 0, then:
    if (0 == pending_promise_count_) {
      web::Context* context =
          base::polymorphic_downcast<web::EnvironmentSettings*>(settings)
              ->context();
      ServiceWorkerJobs* jobs = context->service_worker_jobs();
      DCHECK(jobs);
      // 5.2.1. Let registration be the current global object's associated
      //        service worker's containing service worker registration.
      jobs->message_loop()->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&ServiceWorkerJobs::WaitUntilSubSteps,
                         base::Unretained(jobs),
                         base::Unretained(
                             context->GetWindowOrWorkerGlobalScope()
                                 ->AsServiceWorker()
                                 ->service_worker_object()
                                 ->containing_service_worker_registration())));
    }
    delete promise;
  }

  bool IsActive() {
    // An ExtendableEvent object is said to be active when its timed out flag
    // is unset and either its pending promises count is greater than zero or
    // its dispatch flag is set.
    //   https://w3c.github.io/ServiceWorker/#extendableevent-active
    return !timed_out_flag_ &&
           ((pending_promise_count_ > 0) || IsBeingDispatched());
  }

  DEFINE_WRAPPABLE_TYPE(ExtendableEvent);

 protected:
  ~ExtendableEvent() override {}

 private:
  // https://w3c.github.io/ServiceWorker/#extendableevent-extend-lifetime-promises
  // std::list<script::Promise<script::ValueHandle*>> extend_lifetime_promises_;
  int pending_promise_count_ = 0;
  bool has_rejected_promise_ = false;
  // https://w3c.github.io/ServiceWorker/#extendableevent-timed-out-flag
  bool timed_out_flag_ = false;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_EXTENDABLE_EVENT_H_
