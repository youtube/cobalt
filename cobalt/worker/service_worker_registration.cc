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

#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"

namespace cobalt {
namespace worker {

ServiceWorkerRegistration::ServiceWorkerRegistration(
    script::EnvironmentSettings* settings, const Options& options)
    : dom::EventTarget(settings),
      registration_id_(options.registration_id),
      scope_(std::move(options.scope)) {}

script::Handle<script::Promise<void>> ServiceWorkerRegistration::Update() {
  // Todo: Add logic for update()
  script::Handle<script::Promise<void>> promise =
      base::polymorphic_downcast<web::EnvironmentSettings*>(
          environment_settings())
          ->context()
          ->global_environment()
          ->script_value_factory()
          ->CreateBasicPromise<void>();
  return promise;
}

script::Handle<script::Promise<void>> ServiceWorkerRegistration::Unregister() {
  // Todo: Add logic for unregister
  script::Handle<script::Promise<void>> promise =
      base::polymorphic_downcast<web::EnvironmentSettings*>(
          environment_settings())
          ->context()
          ->global_environment()
          ->script_value_factory()
          ->CreateBasicPromise<void>();
  return promise;
}

std::string ServiceWorkerRegistration::scope() const {
  return scope_.GetContent();
}

void ServiceWorkerRegistration::EnableNavigationPreload(bool enable) {
  // Todo: Implement the logic for set header
}

void ServiceWorkerRegistration::SetNavigationPreloadHeader() {
  // Todo: Implement the logic for set header
}

}  // namespace worker
}  // namespace cobalt
