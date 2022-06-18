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

#include "cobalt/worker/service_worker.h"

#include <memory>
#include <utility>

#include "cobalt/script/environment_settings.h"
#include "cobalt/web/message_port.h"
#include "cobalt/worker/service_worker_global_scope.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/service_worker_state.h"

namespace cobalt {
namespace worker {

ServiceWorker::ServiceWorker(script::EnvironmentSettings* settings,
                             worker::ServiceWorkerObject* worker)
    : web::EventTarget(settings),
      worker_(worker),
      message_port_(new web::MessagePort(worker->worker_global_scope())),
      state_(kServiceWorkerStateParsed) {}

}  // namespace worker
}  // namespace cobalt
