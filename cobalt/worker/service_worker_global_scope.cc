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

#include "cobalt/worker/service_worker_global_scope.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/worker/service_worker_jobs.h"
#include "cobalt/worker/worker_settings.h"

namespace cobalt {
namespace worker {
ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(
    script::EnvironmentSettings* settings, ServiceWorkerObject* service_worker)
    : WorkerGlobalScope(settings), service_worker_(service_worker) {}

void ServiceWorkerGlobalScope::Initialize() {}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerGlobalScope::registration() const {
  // Algorithm for registration():
  //   https://w3c.github.io/ServiceWorker/#service-worker-global-scope-registration

  // The registration getter steps are to return the result of getting the
  // service worker registration object representing this's service worker's
  // containing service worker registration in this's relevant settings object.
  DCHECK(service_worker_);
  return environment_settings()->context()->GetServiceWorkerRegistration(
      service_worker_
          ? service_worker_->containing_service_worker_registration()
          : nullptr);
}

scoped_refptr<ServiceWorker> ServiceWorkerGlobalScope::service_worker() const {
  // Algorithm for service_worker():
  //   https://w3c.github.io/ServiceWorker/#service-worker-global-scope-serviceworker

  // The serviceWorker getter steps are to return the result of getting the
  // service worker object that represents this's service worker in this's
  // relevant settings object.
  DCHECK(service_worker_);
  return environment_settings()->context()->GetServiceWorker(service_worker_);
}


script::HandlePromiseVoid ServiceWorkerGlobalScope::SkipWaiting() {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerGlobalScope::SkipWaiting()");
  DCHECK_EQ(base::MessageLoop::current(),
            environment_settings()->context()->message_loop());
  // Algorithm for skipWaiting():
  //   https://w3c.github.io/ServiceWorker/#dom-serviceworkerglobalscope-skipwaiting
  // 1. Let promise be a new promise.
  auto promise = environment_settings()
                     ->context()
                     ->global_environment()
                     ->script_value_factory()
                     ->CreateBasicPromise<void>();
  std::unique_ptr<script::ValuePromiseVoid::Reference> promise_reference(
      new script::ValuePromiseVoid::Reference(this, promise));

  // 2. Run the following substeps in parallel:
  worker::ServiceWorkerJobs* jobs =
      environment_settings()->context()->service_worker_jobs();
  jobs->message_loop()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerJobs::SkipWaitingSubSteps,
                                base::Unretained(jobs),
                                base::Unretained(environment_settings()),
                                service_worker_, std::move(promise_reference)));
  // 3. Return promise.
  return promise;
}

}  // namespace worker
}  // namespace cobalt
