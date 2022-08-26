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

#include "cobalt/worker/dedicated_worker_global_scope.h"

#include <string>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/worker/worker_global_scope.h"
#include "cobalt/worker/worker_settings.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {
DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(
    script::EnvironmentSettings* settings,
    bool parent_cross_origin_isolated_capability)
    : WorkerGlobalScope(settings), cross_origin_isolated_capability_(false) {
  // Algorithm for 'run a worker'
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#run-a-worker
  //     14.9. If is shared is false and owner's cross-origin isolated
  //     capability is false, then set worker global scope's cross-origin
  //     isolated capability to false.
  if (!parent_cross_origin_isolated_capability) {
    cross_origin_isolated_capability_ = false;
  }
}

void DedicatedWorkerGlobalScope::Initialize() {
  // Algorithm for 'run a worker'
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#run-a-worker
  //     14.4. Initialize worker global scope's policy container given worker
  //        global scope, response, and inside settings.
  //     14.5. If the Run CSP initialization for a global object algorithm
  //     returns
  //        "Blocked" when executed upon worker global scope, set response to a
  //        network error. [CSP]

  //     14.6. If worker global scope's embedder policy's value is compatible
  //     with
  //        cross-origin isolation and is shared is true, then set agent's agent
  //        cluster's cross-origin isolation mode to "logical" or "concrete".
  //        The one chosen is implementation-defined.
  //     No need for dedicated worker.

  //     14.7. If the result of checking a global object's embedder policy with
  //        worker global scope, outside settings, and response is false, then
  //        set response to a network error.
  //     14.8. Set worker global scope's cross-origin isolated capability to
  //     true
  //        if agent's agent cluster's cross-origin isolation mode is
  //        "concrete".

  //     14.10. If is shared is false and response's url's scheme is "data",
  //     then
  //         set worker global scope's cross-origin isolated capability to
  //         false.
  if (!Url().SchemeIs("data")) {
    cross_origin_isolated_capability_ = false;
  }
}

void DedicatedWorkerGlobalScope::PostMessage(
    const script::ValueHandleHolder& message) {
  base::polymorphic_downcast<worker::WorkerSettings*>(environment_settings())
      ->message_port()
      ->PostMessage(message);
}


}  // namespace worker
}  // namespace cobalt
