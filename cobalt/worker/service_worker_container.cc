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

#include "cobalt/worker/service_worker_container.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/registration_options.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "cobalt/worker/worker_type.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

ServiceWorkerContainer::ServiceWorkerContainer(
    script::EnvironmentSettings* settings)
    : web::EventTarget(settings) {}

scoped_refptr<ServiceWorker> ServiceWorkerContainer::controller() {
  // Algorithm for controller:
  //   https://w3c.github.io/ServiceWorker/#navigator-service-worker-controller
  // 1. Let client be this's service worker client.
  web::EnvironmentSettings* client = environment_settings();

  // 2. If client’s active service worker is null, then return null.
  if (!client->context()->active_service_worker()) {
    return scoped_refptr<ServiceWorker>();
  }

  // 3. Return the result of getting the service worker object that represents
  //    client’s active service worker in this's relevant settings object.
  return client->context()->GetServiceWorker(
      client->context()->active_service_worker());
}

script::HandlePromiseWrappable ServiceWorkerContainer::ready() {
  // Algorithm for ready attribute:
  //   https://w3c.github.io/ServiceWorker/#navigator-service-worker-ready
  // 1. If this's ready promise is null, then set this's ready promise to a new
  //    promise.
  if (!promise_reference_) {
    ready_promise_ = environment_settings()
                         ->context()
                         ->global_environment()
                         ->script_value_factory()
                         ->CreateInterfacePromise<
                             scoped_refptr<ServiceWorkerRegistration>>();
    promise_reference_.reset(
        new script::ValuePromiseWrappable::Reference(this, ready_promise_));
  }
  // 2. Let readyPromise be this's ready promise.
  script::HandlePromiseWrappable ready_promise(ready_promise_);
  // 3. If readyPromise is pending, run the following substeps in parallel:
  if (ready_promise->State() == script::PromiseState::kPending) {
    //    3.1. Let client by this's service worker client.
    web::EnvironmentSettings* client = environment_settings();
    worker::ServiceWorkerJobs* jobs = client->context()->service_worker_jobs();
    jobs->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerJobs::MaybeResolveReadyPromiseSubSteps,
                       base::Unretained(jobs), client));
  }
  // 4. Return readyPromise.
  return ready_promise;
}

void ServiceWorkerContainer::MaybeResolveReadyPromise(
    ServiceWorkerRegistrationObject* registration) {
  // This implements resolving of the ready promise for the Activate algorithm
  // (steps 7.1-7.3) as well as for the ready attribute (step 3.3).
  //   https://w3c.github.io/ServiceWorker/#activation-algorithm
  //   https://w3c.github.io/ServiceWorker/#navigator-service-worker-ready
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerContainer::MaybeResolveReadyPromise()");
  DCHECK_EQ(base::MessageLoop::current(),
            environment_settings()->context()->message_loop());
  if (!registration || !registration->active_worker()) return;
  if (!promise_reference_) return;
  if (ready_promise_->State() != script::PromiseState::kPending) return;

  auto registration_object =
      environment_settings()->context()->LookupServiceWorkerRegistration(
          registration);
  if (registration_object) {
    DCHECK(registration_object->active());
    ready_promise_->Resolve(registration_object);
    promise_reference_.reset();
  }
}

script::HandlePromiseWrappable ServiceWorkerContainer::Register(
    const std::string& url) {
  RegistrationOptions options;
  return ServiceWorkerContainer::Register(url, options);
}

script::HandlePromiseWrappable ServiceWorkerContainer::Register(
    const std::string& url, const RegistrationOptions& options) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContainer::Register()");
  DCHECK_EQ(base::MessageLoop::current(),
            environment_settings()->context()->message_loop());
  // https://w3c.github.io/ServiceWorker/#navigator-service-worker-register
  // 1. Let p be a promise.
  script::HandlePromiseWrappable promise =
      environment_settings()
          ->context()
          ->global_environment()
          ->script_value_factory()
          ->CreateInterfacePromise<scoped_refptr<ServiceWorkerRegistration>>();
  std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference(
      new script::ValuePromiseWrappable::Reference(this, promise));

  // 2. Let client be this's service worker client.
  web::EnvironmentSettings* client = environment_settings();
  // 3. Let scriptURL be the result of parsing scriptURL with this's
  // relevant settings object’s API base URL.
  const GURL& base_url = environment_settings()->base_url();
  GURL script_url = base_url.Resolve(url);
  // 4. Let scopeURL be null.
  base::Optional<GURL> scope_url;
  // 5. If options["scope"] exists, set scopeURL to the result of parsing
  //    options["scope"] with this's relevant settings object’s API base URL.
  if (options.has_scope()) {
    scope_url = base_url.Resolve(options.scope());
  }
  // 6. Invoke Start Register with scopeURL, scriptURL, p, client, client’s
  //    creation URL, options["type"], and options["updateViaCache"].
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerJobs::StartRegister,
                     base::Unretained(client->context()->service_worker_jobs()),
                     scope_url, script_url, std::move(promise_reference),
                     client, options.type(), options.update_via_cache()));
  // 7. Return p.
  return promise;
}

script::HandlePromiseWrappable ServiceWorkerContainer::GetRegistration(
    const std::string& url) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContainer::GetRegistration()");
  DCHECK_EQ(base::MessageLoop::current(),
            environment_settings()->context()->message_loop());
  // Algorithm for 'ServiceWorkerContainer.getRegistration()':
  //   https://w3c.github.io/ServiceWorker/#navigator-service-worker-getRegistration
  // Let promise be a new promise.
  // Perform the rest of the steps in a task, because the promise has to be
  // returned before we can safely reject or resolve it.
  auto promise =
      base::polymorphic_downcast<web::EnvironmentSettings*>(
          environment_settings())
          ->context()
          ->global_environment()
          ->script_value_factory()
          ->CreateInterfacePromise<scoped_refptr<ServiceWorkerRegistration>>();
  std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference(
      new script::ValuePromiseWrappable::Reference(this, promise));
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerContainer::GetRegistrationTask,
                                base::Unretained(this), url,
                                std::move(promise_reference)));
  // 9. Return promise.
  return promise;
}

void ServiceWorkerContainer::GetRegistrationTask(
    const std::string& url,
    std::unique_ptr<script::ValuePromiseWrappable::Reference>
        promise_reference) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerContainer::GetRegistrationTask()");
  DCHECK_EQ(base::MessageLoop::current(),
            environment_settings()->context()->message_loop());
  // Algorithm for 'ServiceWorkerContainer.getRegistration()':
  //   https://w3c.github.io/ServiceWorker/#navigator-service-worker-getRegistration
  // 1. Let client be this's service worker client.
  web::EnvironmentSettings* client = environment_settings();

  // 2. Let storage key be the result of running obtain a storage key given
  //    client.
  url::Origin storage_key = client->ObtainStorageKey();

  // 3. Let clientURL be the result of parsing clientURL with this's relevant
  //    settings object’s API base URL.
  // TODO(b/234659851): Investigate whether this behaves as expected for empty
  // url values.
  const GURL& base_url = environment_settings()->base_url();
  GURL client_url = base_url.Resolve(url);

  // 4. If clientURL is failure, return a promise rejected with a TypeError.
  if (client_url.is_empty()) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 5. Set clientURL’s fragment to null.
  url::Replacements<char> replacements;
  replacements.ClearRef();
  client_url = client_url.ReplaceComponents(replacements);
  DCHECK(!client_url.has_ref() || client_url.ref().empty());

  // 6. If the origin of clientURL is not client’s origin, return a promise
  //    rejected with a "SecurityError" web::DOMException.
  if (client_url.GetOrigin() != base_url.GetOrigin()) {
    promise_reference->value().Reject(
        new web::DOMException(web::DOMException::kSecurityErr));
    return;
  }

  // 7. Let promise be a new promise.
  // 8. Run the following substeps in parallel:
  worker::ServiceWorkerJobs* jobs = client->context()->service_worker_jobs();
  DCHECK(jobs);
  jobs->message_loop()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerJobs::GetRegistrationSubSteps,
                                base::Unretained(jobs), storage_key, client_url,
                                client, std::move(promise_reference)));
}

script::HandlePromiseSequenceWrappable
ServiceWorkerContainer::GetRegistrations() {
  // https://w3c.github.io/ServiceWorker/#navigator-service-worker-getRegistrations
  // 1. Let client be this's service worker client.
  // 2. Let promise be a new promise.
  // 3. Run the following steps in parallel:
  //    1. Let registrations be a new list.
  //    2. For each scope → registration of scope to registration map:
  //       1. If the origin of the result of parsing scope is the same as
  //          client’s origin, then append registration to registrations.
  //    3. Queue a task on promise’s relevant settings object's responsible
  //       event loop, using the DOM manipulation task source, to run the
  //       following steps:
  //       1. Let registrationObjects be a new list.
  //       2. For each registration of registrations:
  //          1. Let registrationObj be the result of getting the service worker
  //             registration object that represents registration in promise’s
  //             relevant settings object.
  //          2. Append registrationObj to registrationObjects.
  //       3. Resolve promise with a new frozen array of registrationObjects in
  //          promise’s relevant Realm.
  // 4. Return promise.
  // TODO(b/235531652): Implement getRegistrations().
  auto promise = environment_settings()
                     ->context()
                     ->global_environment()
                     ->script_value_factory()
                     ->CreateBasicPromise<script::SequenceWrappable>();

  return promise;
}

void ServiceWorkerContainer::StartMessages() {}

}  // namespace worker
}  // namespace cobalt
