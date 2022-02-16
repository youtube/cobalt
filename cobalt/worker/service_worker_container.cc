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

namespace cobalt {
namespace worker {

ServiceWorkerContainer::ServiceWorkerContainer(
    script::EnvironmentSettings* settings,
    script::ScriptValueFactory* script_value_factory)
    : dom::EventTarget(settings), script_value_factory_(script_value_factory) {}

// TODO: Implement the service worker registration algorithm. b/219972966
script::Handle<script::Promise<void>> ServiceWorkerContainer::ready() {
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
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  return promise;
}

script::Handle<script::Promise<void>> ServiceWorkerContainer::Register(
    const std::string& url) {
  RegistrationOptions* options = new RegistrationOptions();
  return ServiceWorkerContainer::Register(url, *options);
}

script::Handle<script::Promise<void>> ServiceWorkerContainer::Register(
    const std::string& url, const RegistrationOptions& options) {
  // https://w3c.github.io/ServiceWorker/#navigator-service-worker-registers
  // 1. Let p be a promise.
  // 2. Let client be this's service worker client.
  // 3. Let scriptURL be the result of parsing scriptURL with this's relevant
  //    settings object’s API base URL.
  // 4. Let scopeURL be null.
  // 5. If options["scope"] exists, set scopeURL to the result of parsing
  //    options["scope"] with this's relevant settings object’s API base URL.
  // 6. Invoke Start Register with scopeURL, scriptURL, p, client, client’s
  //    creation URL, options["type"], and options["updateViaCache"].
  // 7. Return p.
  LOG(INFO) << "The service worker is registered";
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  return promise;
}

script::Handle<script::Promise<void>> ServiceWorkerContainer::GetRegistration(
    const std::string& url) {
  // https://w3c.github.io/ServiceWorker/#navigator-service-worker-getRegistration
  // 1. Let client be this's service worker client.
  // 2. Let clientURL be the result of parsing clientURL with this's relevant
  //    settings object’s API base URL.
  // 3. If clientURL is failure, return a promise rejected with a TypeError.
  // 4. Set clientURL’s fragment to null.
  // 5. If the origin of clientURL is not client’s origin, return a promise
  //    rejected with a "SecurityError" DOMException.
  // 6. Let promise be a new promise.
  // 7. Run the following substeps in parallel:
  //    1. Let registration be the result of running Match Service Worker
  //       Registration algorithm with clientURL as its argument.
  //    2. If registration is null, resolve promise with undefined and abort
  //       these steps.
  //    3. Resolve promise with the result of getting the service worker
  //       registration object that represents registration in promise’s
  //       relevant settings object.
  // 8. Return promise.
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  return promise;
}

script::Handle<script::Promise<void>>
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
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  return promise;
}

void ServiceWorkerContainer::StartMessages() {}

}  // namespace worker
}  // namespace cobalt
