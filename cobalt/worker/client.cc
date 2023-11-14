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

#include "cobalt/worker/client.h"

#include "cobalt/web/environment_settings.h"
#include "cobalt/web/navigator_base.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "cobalt/worker/client_type.h"
#include "cobalt/worker/service_worker_container.h"

namespace cobalt {
namespace worker {
Client::Client(web::EnvironmentSettings* client) {
  DCHECK(client);
  EntangleWithEventTarget(client->context()
                              ->GetWindowOrWorkerGlobalScope()
                              ->navigator_base()
                              ->service_worker());
  // Algorithm for Create Client:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#create-client
  // 1. Let clientObject be a new Client object.
  // 2. Set clientObject’s service worker client to client.
  service_worker_client_ = client;

  // 3. Return clientObject.
}

ClientType Client::type() {
  // Algorithm for the type getter:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#client-type

  // 1. Let client be this's service worker client.
  web::WindowOrWorkerGlobalScope* client_global_scope =
      service_worker_client_->context()->GetWindowOrWorkerGlobalScope();

  // 2. If client is an environment settings object, then:
  // In Cobalt, this includes all clients.

  // 2.1. If client is a window client, return "window".
  if (client_global_scope->IsWindow()) return kClientTypeWindow;

  // 2.2. Else if client is a dedicated worker client, return "worker".
  if (client_global_scope->IsDedicatedWorker()) return kClientTypeWorker;

  // 2.3. Else if client is a shared worker client, return "sharedworker".
  // Shared workers are not supported in Cobalt.

  // 3. Else:
  // 3.1. Return "window".
  return kClientTypeWindow;
}

}  // namespace worker
}  // namespace cobalt
