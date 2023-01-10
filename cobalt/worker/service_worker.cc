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

#include "base/bind.h"
#include "cobalt/base/tokens.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/message_port.h"
#include "cobalt/worker/service_worker_global_scope.h"
#include "cobalt/worker/service_worker_jobs.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/service_worker_state.h"

namespace cobalt {
namespace worker {

ServiceWorker::ServiceWorker(script::EnvironmentSettings* settings,
                             worker::ServiceWorkerObject* worker)
    : web::EventTarget(settings), worker_(worker) {}

void ServiceWorker::PostMessage(const script::ValueHandleHolder& message) {
  // Algorithm for ServiceWorker.postMessage():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-postmessage

  // 1. Let serviceWorker be the service worker represented by this.
  ServiceWorkerObject* service_worker = service_worker_object();
  // 2. Let incumbentSettings be the incumbent settings object.
  web::EnvironmentSettings* incumbent_settings = environment_settings();
  // 3. Let incumbentGlobal be incumbentSettings’s global object.
  // 4. Let serializeWithTransferResult be
  //    StructuredSerializeWithTransfer(message, options["transfer"]).
  //    Rethrow any exceptions.
  std::unique_ptr<script::DataBuffer> serialize_result(
      script::SerializeScriptValue(message));
  if (!serialize_result) {
    return;
  }
  // 5. If the result of running the Should Skip Event algorithm with
  // "message" and serviceWorker is true, then return.
  if (service_worker->ShouldSkipEvent(base::Tokens::message())) return;
  // 6. Run these substeps in parallel:
  ServiceWorkerJobs* jobs =
      incumbent_settings->context()->service_worker_jobs();
  DCHECK(jobs);
  jobs->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerJobs::ServiceWorkerPostMessageSubSteps,
                     base::Unretained(jobs), base::Unretained(service_worker),
                     base::Unretained(incumbent_settings->context()),
                     std::move(serialize_result)));
}

}  // namespace worker
}  // namespace cobalt
