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

#include "cobalt/worker/navigation_preload_manager.h"

#include <vector>

#include "cobalt/web/dom_exception.h"
#include "cobalt/worker/service_worker_registration.h"

namespace cobalt {
namespace worker {

NavigationPreloadManager::NavigationPreloadManager(
    ServiceWorkerRegistration* registration,
    script::ScriptValueFactory* script_value_factory)
    : script_value_factory_(script_value_factory),
      registration_(registration) {}

script::Handle<script::Promise<void>> NavigationPreloadManager::Enable() {
  return SetEnabled(true);
}

script::Handle<script::Promise<void>> NavigationPreloadManager::Disable() {
  return SetEnabled(false);
}

script::Handle<script::Promise<void>> NavigationPreloadManager::SetHeaderValue(
    std::vector<uint8_t> value) {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();

  if (registration_->active() == nullptr) {
    promise->Reject(new web::DOMException(web::DOMException::kInvalidStateErr));
    return promise;
  }

  registration_->SetNavigationPreloadHeader();
  promise->Resolve();
  return promise;
}

script::Handle<script::Promise<void>> NavigationPreloadManager::SetEnabled(
    bool enable) {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  if (registration_->active() == nullptr) {
    promise->Reject(new web::DOMException(web::DOMException::kInvalidStateErr));
    return promise;
  }
  registration_->EnableNavigationPreload(enable);
  promise->Resolve();
  return promise;
}

script::Handle<NavigationPreloadManager::NavigationPreloadStatePromise>
NavigationPreloadManager::GetState() {
  // Todo: Implement getState logic
  script::Handle<NavigationPreloadStatePromise> promise =
      script_value_factory_->CreateInterfacePromise<NavigationPreloadState>();
  return promise;
}

}  // namespace worker
}  // namespace cobalt
