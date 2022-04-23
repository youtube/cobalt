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

#include "cobalt/worker/service_worker_jobs.h"

#include <string>

#include "base/trace_event/trace_event.h"

namespace cobalt {
namespace worker {

ServiceWorkerJobs::ServiceWorkerJobs(network::NetworkModule* network_module,
                                     base::MessageLoop* message_loop) {}
ServiceWorkerJobs::~ServiceWorkerJobs() {}

void ServiceWorkerJobs::StartRegister(
    const base::Optional<GURL>& scope_url, const GURL& script_url,
    const script::Handle<script::Promise<void>>& promise,
    web::EnvironmentSettings* client, const WorkerType& type,
    const ServiceWorkerUpdateViaCache& update_via_cache) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::StartRegister()");
  NOTIMPLEMENTED();
}

void ServiceWorkerJobs::MatchServiceWorkerRegistration(const GURL& client_url) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerJobs::MatchServiceWorkerRegistration()");
  // TODO this need a callback to return the matching registration.
  NOTIMPLEMENTED();
}

}  // namespace worker
}  // namespace cobalt
