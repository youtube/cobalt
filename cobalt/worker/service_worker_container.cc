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

#include "base/optional.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/script/promise.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/registration_options.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "cobalt/worker/worker_type.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

ServiceWorkerContainer::ServiceWorkerContainer(
    script::EnvironmentSettings* settings,
    worker::ServiceWorkerJobs* service_worker_jobs)
    : dom::EventTarget(settings) {}

// TODO: Implement the service worker ready algorithm. b/219972966
script::Handle<script::PromiseWrappable> ServiceWorkerContainer::ready() {
  // https://w3c.github.io/ServiceWorker/#navigator-service-worker-ready
  // 1. If this's ready promise is null, then set this's ready promise to a new
  // promise.
  // 2. Let readyPromise be this's ready promise.
  // 3. If readyPromise is pending, run the following substeps in parallel:
  //    1. Let registration be the result of running Match Service Worker
  //    Registration with this's service worker client's creation URL.
  //    2. If registration is not null, and registration’s active worker is not
  //    null, queue a task on readyPromise’s relevant settings object's
  //    responsible event loop, using the DOM manipulation task source, to
  //    resolve readyPromise with the result of getting the service worker
  //    registration object that represents registration in readyPromise’s
  //    relevant settings object.
  // 4. Return readyPromise.
  auto promise =
      base::polymorphic_downcast<web::EnvironmentSettings*>(
          environment_settings())
          ->context()
          ->global_environment()
          ->script_value_factory()
          ->CreateInterfacePromise<scoped_refptr<ServiceWorkerRegistration>>();
  return promise;
}

script::Handle<script::PromiseWrappable> ServiceWorkerContainer::Register(
    const std::string& url) {
  RegistrationOptions options;
  return ServiceWorkerContainer::Register(url, options);
}

script::Handle<script::PromiseWrappable> ServiceWorkerContainer::Register(
    const std::string& url, const RegistrationOptions& options) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContainer::Register()");
  // https://w3c.github.io/ServiceWorker/#navigator-service-worker-register
  // 1. Let p be a promise.
  script::HandlePromiseWrappable promise =
      base::polymorphic_downcast<web::EnvironmentSettings*>(
          environment_settings())
          ->context()
          ->global_environment()
          ->script_value_factory()
          ->CreateInterfacePromise<scoped_refptr<ServiceWorkerRegistration>>();
  std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference(
      new script::ValuePromiseWrappable::Reference(this, promise));

  // 2. Let client be this's service worker client.
  web::EnvironmentSettings* client =
      base::polymorphic_downcast<web::EnvironmentSettings*>(
          environment_settings());
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
  base::polymorphic_downcast<web::EnvironmentSettings*>(environment_settings())
      ->context()
      ->message_loop()
      ->task_runner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ServiceWorkerJobs::StartRegister,
              base::Unretained(base::polymorphic_downcast<dom::DOMSettings*>(
                                   environment_settings())
                                   ->service_worker_jobs()),
              scope_url, script_url, std::move(promise_reference), client,
              options.type(), options.update_via_cache()));
  // 7. Return p.
  return promise;
}

script::Handle<script::PromiseWrappable>
ServiceWorkerContainer::GetRegistration(const std::string& url) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContainer::GetRegistration()");
  // https://w3c.github.io/ServiceWorker/#navigator-service-worker-getRegistration
  // 1. Let client be this's service worker client.
  web::EnvironmentSettings* client =
      base::polymorphic_downcast<web::EnvironmentSettings*>(
          environment_settings());
  // 2. Let clientURL be the result of parsing clientURL with this's relevant
  //    settings object’s API base URL.
  const GURL& base_url = environment_settings()->base_url();
  GURL client_url = base_url.Resolve(url);

  // 3. If clientURL is failure, return a promise rejected with a TypeError.
  auto promise =
      base::polymorphic_downcast<web::EnvironmentSettings*>(
          environment_settings())
          ->context()
          ->global_environment()
          ->script_value_factory()
          ->CreateInterfacePromise<scoped_refptr<ServiceWorkerRegistration>>();
  if (client_url.is_empty()) {
    promise->Reject(script::kTypeError);
    return promise;
  }

  // 4. Set clientURL’s fragment to null.
  url::Replacements<char> replacements;
  replacements.ClearRef();
  client_url = client_url.ReplaceComponents(replacements);
  DCHECK(!client_url.has_ref() || client_url.ref().empty());

  // 5. If the origin of clientURL is not client’s origin, return a promise
  //    rejected with a "SecurityError" DOMException.
  if (client_url.GetOrigin() != base_url.GetOrigin()) {
    promise->Reject(new dom::DOMException(dom::DOMException::kSecurityErr));
    return promise;
  }

  // 6. Let promise be a new promise.
  // 7. Run the following substeps in parallel:
  //    1. Let registration be the result of running Match Service Worker
  //       Registration algorithm with clientURL as its argument.
  // TODO handle parallelism, and add callback to resolve the promise.
  base::polymorphic_downcast<dom::DOMSettings*>(environment_settings())
      ->service_worker_jobs()
      ->MatchServiceWorkerRegistration(client_url);
  //    2. If registration is null, resolve promise with undefined and abort
  //       these steps.
  //    3. Resolve promise with the result of getting the service worker
  //       registration object that represents registration in promise’s
  //       relevant settings object.
  // 8. Return promise.
  return promise;
}

script::Handle<script::PromiseSequenceWrappable>
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
  auto promise = base::polymorphic_downcast<web::EnvironmentSettings*>(
                     environment_settings())
                     ->context()
                     ->global_environment()
                     ->script_value_factory()
                     ->CreateBasicPromise<script::SequenceWrappable>();

  return promise;
}

void ServiceWorkerContainer::StartMessages() {}

}  // namespace worker
}  // namespace cobalt
