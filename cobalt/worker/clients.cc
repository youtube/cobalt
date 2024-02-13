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

#include "cobalt/worker/clients.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/task/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/client.h"
#include "cobalt/worker/service_worker_context.h"
#include "cobalt/worker/service_worker_global_scope.h"
#include "cobalt/worker/service_worker_object.h"

namespace cobalt {
namespace worker {

namespace {
ServiceWorkerObject* GetAssociatedServiceWorker(
    web::EnvironmentSettings* settings) {
  // Ensure this runs in the right thread to dereferences the WeakPtr.
  DCHECK_EQ(settings->context()->message_loop(), base::MessageLoop::current());
  auto* global_scope = settings->context()->GetWindowOrWorkerGlobalScope();
  DCHECK(global_scope->IsServiceWorker());

  return global_scope->AsServiceWorker()->service_worker_object();
}
}  // namespace

Clients::Clients(script::EnvironmentSettings* settings)
    : settings_(
          base::polymorphic_downcast<web::EnvironmentSettings*>(settings)) {}

script::HandlePromiseWrappable Clients::Get(const std::string& id) {
  TRACE_EVENT0("cobalt::worker", "Clients::Get()");
  DCHECK_EQ(base::MessageLoop::current(), settings_->context()->message_loop());
  // Algorithm for get(id):
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#clients-get
  // 1. Let promise be a new promise.
  script::HandlePromiseWrappable promise =
      settings_->context()
          ->global_environment()
          ->script_value_factory()
          ->CreateInterfacePromise<scoped_refptr<Client>>();
  std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference(
      new script::ValuePromiseWrappable::Reference(
          settings_->context()->GetWindowOrWorkerGlobalScope(), promise));

  // 2. Run these substeps in parallel:
  ServiceWorkerContext* context =
      settings_->context()->service_worker_context();
  DCHECK(context);
  context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContext::ClientsGetSubSteps,
                     base::Unretained(context),
                     base::Unretained(settings_->context()),
                     base::Unretained(GetAssociatedServiceWorker(settings_)),
                     std::move(promise_reference), id));

  // 3. Return promise.
  return promise;
}

script::HandlePromiseSequenceWrappable Clients::MatchAll(
    const ClientQueryOptions& options) {
  TRACE_EVENT0("cobalt::worker", "Clients::MatchAll()");
  DCHECK_EQ(base::MessageLoop::current(), settings_->context()->message_loop());
  // Algorithm for matchAll():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#clients-matchall
  // 1. Let promise be a new promise.
  auto promise = settings_->context()
                     ->global_environment()
                     ->script_value_factory()
                     ->CreateBasicPromise<script::SequenceWrappable>();
  std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
      promise_reference(new script::ValuePromiseSequenceWrappable::Reference(
          settings_->context()->GetWindowOrWorkerGlobalScope(), promise));
  // 2. Run the following steps in parallel:
  ServiceWorkerContext* context =
      settings_->context()->service_worker_context();
  DCHECK(context);
  context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContext::ClientsMatchAllSubSteps,
                     base::Unretained(context),
                     base::Unretained(settings_->context()),
                     base::Unretained(GetAssociatedServiceWorker(settings_)),
                     std::move(promise_reference),
                     options.include_uncontrolled(), options.type()));
  // 3. Return promise.
  return promise;
}

script::HandlePromiseVoid Clients::Claim() {
  TRACE_EVENT0("cobalt::worker", "Clients::Claim()");
  DCHECK_EQ(base::MessageLoop::current(), settings_->context()->message_loop());
  // Algorithm for claim():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#clients-claim
  // 2. Let promise be a new promise.
  // Note: Done first because it's needed for rejecting in step 1.
  auto promise = settings_->context()
                     ->global_environment()
                     ->script_value_factory()
                     ->CreateBasicPromise<void>();
  std::unique_ptr<script::ValuePromiseVoid::Reference> promise_reference(
      new script::ValuePromiseVoid::Reference(
          settings_->context()->GetWindowOrWorkerGlobalScope(), promise));

  // 1. If the service worker is not an active worker, return a promise rejected
  // with an "InvalidStateError" DOMException.
  ServiceWorkerObject* service_worker = GetAssociatedServiceWorker(settings_);
  DCHECK(service_worker->containing_service_worker_registration());
  if (service_worker != service_worker->containing_service_worker_registration()
                            ->active_worker()) {
    DCHECK(service_worker->state() != kServiceWorkerStateActivated &&
           service_worker->state() != kServiceWorkerStateActivating);
    // Perform the rest of the steps in a task, because the promise has to be
    // returned before we can safely reject it.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<script::ValuePromiseVoid::Reference>
                   promise_reference) {
              promise_reference->value().Reject(
                  new web::DOMException(web::DOMException::kInvalidStateErr));
            },
            std::move(promise_reference)));
    return promise;
  }

  DCHECK(service_worker->state() == kServiceWorkerStateActivated ||
         service_worker->state() == kServiceWorkerStateActivating);

  // 3. Run the following substeps in parallel:
  ServiceWorkerContext* context =
      settings_->context()->service_worker_context();
  DCHECK(context);
  context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContext::ClaimSubSteps,
                     base::Unretained(context),
                     base::Unretained(settings_->context()),
                     base::Unretained(GetAssociatedServiceWorker(settings_)),
                     std::move(promise_reference)));
  // 4. Return promise.
  return promise;
}

}  // namespace worker
}  // namespace cobalt
