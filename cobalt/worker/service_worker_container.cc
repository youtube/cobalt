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
#include "base/threading/thread_task_runner_handle.h"
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
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#navigator-service-worker-controller
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
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#navigator-service-worker-ready
  // 1. If this's ready promise is null, then set this's ready promise to a new
  //    promise.
  if (!promise_reference_) {
    ready_promise_ = environment_settings()
                         ->context()
                         ->global_environment()
                         ->script_value_factory()
                         ->CreateInterfacePromise<
                             scoped_refptr<ServiceWorkerRegistration>>();
    promise_reference_.reset(new script::ValuePromiseWrappable::Reference(
        environment_settings()->context()->GetWindowOrWorkerGlobalScope(),
        ready_promise_));
  }
  // 2. Let readyPromise be this's ready promise.
  script::HandlePromiseWrappable ready_promise(ready_promise_);
  // 3. If readyPromise is pending, run the following substeps in parallel:
  if (ready_promise->State() == script::PromiseState::kPending) {
    //    3.1. Let client by this's service worker client.
    web::Context* client = environment_settings()->context();
    ServiceWorkerContext* worker_context = client->service_worker_context();
    worker_context->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerContext::MaybeResolveReadyPromiseSubSteps,
                       base::Unretained(worker_context), client));
  }
  // 4. Return readyPromise.
  return ready_promise;
}

void ServiceWorkerContainer::MaybeResolveReadyPromise(
    scoped_refptr<ServiceWorkerRegistrationObject> registration) {
  // This implements resolving of the ready promise for the Activate algorithm
  // (steps 7.1-7.3) as well as for the ready attribute (step 3.3).
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#activation-algorithm
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#navigator-service-worker-ready
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
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#navigator-service-worker-register
  // 1. Let p be a promise.
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

  // 2. Let client be this's service worker client.
  web::Context* client = environment_settings()->context();
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContext::StartRegister,
                     base::Unretained(client->service_worker_context()),
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
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#navigator-service-worker-getRegistration
  // Let promise be a new promise.
  // Perform the rest of the steps in a task, because the promise has to be
  // returned before we can safely reject or resolve it.
  auto promise =
      environment_settings()
          ->context()
          ->global_environment()
          ->script_value_factory()
          ->CreateInterfacePromise<scoped_refptr<ServiceWorkerRegistration>>();
  std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference(
      new script::ValuePromiseWrappable::Reference(
          environment_settings()->context()->GetWindowOrWorkerGlobalScope(),
          promise));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#navigator-service-worker-getRegistration
  // 1. Let client be this's service worker client.
  web::Context* client = environment_settings()->context();

  // 2. Let storage key be the result of running obtain a storage key given
  //    client.
  url::Origin storage_key = client->environment_settings()->ObtainStorageKey();

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
  GURL::Replacements replacements;
  replacements.ClearRef();
  client_url = client_url.ReplaceComponents(replacements);
  DCHECK(!client_url.has_ref() || client_url.ref().empty());

  // 6. If the origin of clientURL is not client’s origin, return a promise
  //    rejected with a "SecurityError" web::DOMException.
  if (client_url.DeprecatedGetOriginAsURL() !=
      base_url.DeprecatedGetOriginAsURL()) {
    promise_reference->value().Reject(
        new web::DOMException(web::DOMException::kSecurityErr));
    return;
  }

  // 7. Let promise be a new promise.
  // 8. Run the following substeps in parallel:
  ServiceWorkerContext* worker_context = client->service_worker_context();
  DCHECK(worker_context);
  worker_context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContext::GetRegistrationSubSteps,
                     base::Unretained(worker_context), storage_key, client_url,
                     client, std::move(promise_reference)));
}

script::HandlePromiseSequenceWrappable
ServiceWorkerContainer::GetRegistrations() {
  auto promise = environment_settings()
                     ->context()
                     ->global_environment()
                     ->script_value_factory()
                     ->CreateBasicPromise<script::SequenceWrappable>();
  std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
      promise_reference(new script::ValuePromiseSequenceWrappable::Reference(
          environment_settings()->context()->GetWindowOrWorkerGlobalScope(),
          promise));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerContainer::GetRegistrationsTask,
                     base::Unretained(this), std::move(promise_reference)));
  return promise;
}

void ServiceWorkerContainer::GetRegistrationsTask(
    std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
        promise_reference) {
  auto* client = environment_settings()->context();
  // https://w3c.github.io/ServiceWorker/#navigator-service-worker-getRegistrations
  ServiceWorkerContext* worker_context =
      environment_settings()->context()->service_worker_context();
  url::Origin storage_key = environment_settings()->ObtainStorageKey();
  worker_context->message_loop()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerContext::GetRegistrationsSubSteps,
                                base::Unretained(worker_context), storage_key,
                                client, std::move(promise_reference)));
}

void ServiceWorkerContainer::StartMessages() {}

}  // namespace worker
}  // namespace cobalt
