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

namespace cobalt {
namespace worker {

ServiceWorkerContainer::ServiceWorkerContainer(
    script::ScriptValueFactory* script_value_factory)
    : script_value_factory_(script_value_factory) {}

script::Handle<script::Promise<void>> ServiceWorkerContainer::Register() {
  LOG(INFO) << "The service worker is registered";
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  return promise;
}

}  // namespace worker
}  // namespace cobalt
