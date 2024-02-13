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

#include "cobalt/worker/service_worker_registration.h"

#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "cobalt/worker/service_worker.h"
#include "cobalt/worker/service_worker_context.h"
#include "cobalt/worker/service_worker_global_scope.h"
#include "cobalt/worker/service_worker_jobs.h"
#include "cobalt/worker/service_worker_registration_object.h"

namespace cobalt {
namespace worker {

ServiceWorkerRegistration::ServiceWorkerRegistration(
    script::EnvironmentSettings* settings,
    worker::ServiceWorkerRegistrationObject* registration)
    : web::EventTarget(settings), registration_(registration) {}

script::HandlePromiseWrappable ServiceWorkerRegistration::Update() {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerRegistration::Update()");
  // Algorithm for update():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-registration-update

  script::HandlePromiseWrappable promise =
      environment_settings()
          ->context()
          ->global_environment()
          ->script_value_factory()
          ->CreateInterfacePromise<scoped_refptr<ServiceWorkerRegistration>>();
  std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference(
      new script::ValuePromiseWrappable::Reference(
          environment_settings()->context()->GetWindowOrWorkerGlobalScope(),
          promise));
  // Perform the rest of the steps in a task, because the promise has to be
  // returned before we can safely reject or resolve it.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerRegistration::UpdateTask,
                     base::Unretained(this), std::move(promise_reference)));

  // 9. Return promise.
  return promise;
}

void ServiceWorkerRegistration::UpdateTask(
    std::unique_ptr<script::ValuePromiseWrappable::Reference>
        promise_reference) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerRegistration::UpdateTask()");
  DCHECK_EQ(base::MessageLoop::current(),
            environment_settings()->context()->message_loop());
  // Algorithm for update():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-registration-update

  // 1. Let registration be the service worker registration.
  // 2. Let newestWorker be the result of running Get Newest Worker algorithm
  //    passing registration as its argument.
  ServiceWorkerObject* newest_worker = registration_->GetNewestWorker();

  // 3. If newestWorker is null, return a promise rejected with an
  //    "InvalidStateError" DOMException and abort these steps.
  if (!newest_worker) {
    promise_reference->value().Reject(
        new web::DOMException(web::DOMException::kInvalidStateErr));
    return;
  }

  // 4. If this's relevant global object globalObject is a
  //    ServiceWorkerGlobalScope object, and globalObject’s associated service
  //    worker's state is "installing", return a promise rejected with an
  //    "InvalidStateError" DOMException and abort these steps.
  web::WindowOrWorkerGlobalScope* window_or_worker_global_scope =
      environment_settings()->context()->GetWindowOrWorkerGlobalScope();
  if (window_or_worker_global_scope->IsServiceWorker()) {
    ServiceWorkerGlobalScope* global_object =
        base::polymorphic_downcast<ServiceWorkerGlobalScope*>(
            window_or_worker_global_scope);
    ServiceWorker* service_worker = global_object->service_worker();
    if (service_worker &&
        service_worker->state() == kServiceWorkerStateInstalling) {
      promise_reference->value().Reject(
          new web::DOMException(web::DOMException::kInvalidStateErr));
      return;
    }
  }

  // 5. Let promise be a promise.
  // 6. Let job be the result of running Create Job with update, registration’s
  //    storage key, registration’s scope url, newestWorker’s script url,
  //    promise, and this's relevant settings object.
  ServiceWorkerJobs* jobs =
      environment_settings()->context()->service_worker_context()->jobs();
  std::unique_ptr<ServiceWorkerJobs::Job> job = jobs->CreateJob(
      ServiceWorkerJobs::JobType::kUpdate, registration_->storage_key(),
      registration_->scope_url(), newest_worker->script_url(),
      std::move(promise_reference), environment_settings()->context());

  // 7. Set job’s worker type to newestWorker’s type.
  // Cobalt only supports 'classic' worker type.

  // 8. Invoke Schedule Job with job.
  jobs->ScheduleJob(std::move(job));
}

script::HandlePromiseBool ServiceWorkerRegistration::Unregister() {
  // Algorithm for unregister():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#navigator-service-worker-unregister
  // 1. Let registration be the service worker registration.
  // 2. Let promise be a new promise.
  script::HandlePromiseBool promise = environment_settings()
                                          ->context()
                                          ->global_environment()
                                          ->script_value_factory()
                                          ->CreateBasicPromise<bool>();
  std::unique_ptr<script::ValuePromiseBool::Reference> promise_reference(
      new script::ValuePromiseBool::Reference(
          environment_settings()->context()->GetWindowOrWorkerGlobalScope(),
          promise));

  // Perform the rest of the steps in a task, so that unregister doesn't race
  // past any previously submitted update requests.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ServiceWorkerJobs* jobs, const url::Origin& storage_key,
             const GURL& scope_url,
             std::unique_ptr<script::ValuePromiseBool::Reference>
                 promise_reference,
             web::Context* client) {
            // 3. Let job be the result of running Create Job with unregister,
            //    registration’s storage key, registration’s scope url, null,
            //    promise, and this's relevant settings object.
            std::unique_ptr<ServiceWorkerJobs::Job> job = jobs->CreateJob(
                ServiceWorkerJobs::JobType::kUnregister, storage_key, scope_url,
                GURL(), std::move(promise_reference), client);
            DCHECK(!promise_reference);

            // 4. Invoke Schedule Job with job.
            jobs->ScheduleJob(std::move(job));
            DCHECK(!job.get());
          },
          environment_settings()->context()->service_worker_context()->jobs(),
          registration_->storage_key(), registration_->scope_url(),
          std::move(promise_reference), environment_settings()->context()));
  // 5. Return promise.
  return promise;
}

std::string ServiceWorkerRegistration::scope() const {
  return registration_->scope_url().spec();
}

ServiceWorkerUpdateViaCache ServiceWorkerRegistration::update_via_cache()
    const {
  return registration_->update_via_cache_mode();
}

void ServiceWorkerRegistration::EnableNavigationPreload(bool enable) {
  // Todo: Implement the logic for set header
}

void ServiceWorkerRegistration::SetNavigationPreloadHeader() {
  // Todo: Implement the logic for set header
}

}  // namespace worker
}  // namespace cobalt
