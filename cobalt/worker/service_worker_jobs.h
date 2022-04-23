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

#ifndef COBALT_WORKER_SERVICE_WORKER_JOBS_H_
#define COBALT_WORKER_SERVICE_WORKER_JOBS_H_

#include "cobalt/network/network_module.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "cobalt/worker/worker_type.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

// Algorithms for Service Worker Jobs.
class ServiceWorkerJobs {
 public:
  ServiceWorkerJobs(network::NetworkModule* network_module,
                    base::MessageLoop* message_loop);
  ~ServiceWorkerJobs();

  // https://w3c.github.io/ServiceWorker/#start-register-algorithm
  void StartRegister(const base::Optional<GURL>& scope_url,
                     const GURL& script_url,
                     const script::Handle<script::Promise<void>>& promise,
                     web::EnvironmentSettings* client, const WorkerType& type,
                     const ServiceWorkerUpdateViaCache& update_via_cache);

  // https://w3c.github.io/ServiceWorker/#scope-match-algorithm
  void MatchServiceWorkerRegistration(const GURL& client_url);
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_JOBS_H_
