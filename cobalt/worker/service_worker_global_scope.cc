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

#include "base/logging.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/worker_settings.h"

namespace cobalt {
namespace worker {
ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(
    web::EnvironmentSettings* settings)
    : WorkerGlobalScope(settings) {}

void ServiceWorkerGlobalScope::Initialize() {}

script::Handle<script::Promise<void>> ServiceWorkerGlobalScope::SkipWaiting() {
  auto promise = base::polymorphic_downcast<web::EnvironmentSettings*>(
                     environment_settings())
                     ->context()
                     ->global_environment()
                     ->script_value_factory()
                     ->CreateBasicPromise<void>();
  NOTIMPLEMENTED();
  return promise;
}

}  // namespace worker
}  // namespace cobalt
