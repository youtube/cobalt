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

#include "cobalt/worker/service_worker_registration_object.h"

#include <string>

#include "cobalt/worker/service_worker_update_via_cache.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace worker {

ServiceWorkerRegistrationObject::ServiceWorkerRegistrationObject(
    const url::Origin& storage_key, const GURL& scope_url,
    const ServiceWorkerUpdateViaCache& update_via_cache_mode)
    : storage_key_(storage_key),
      scope_url_(scope_url),
      update_via_cache_mode_(update_via_cache_mode),
      last_update_check_time_(base::Time()) {}

ServiceWorkerRegistrationObject::~ServiceWorkerRegistrationObject() {
  AbortAll();
}

void ServiceWorkerRegistrationObject::AbortAll() {
  if (installing_worker()) {
    installing_worker()->Abort();
  }
  if (waiting_worker()) {
    waiting_worker()->Abort();
  }
  if (active_worker()) {
    active_worker()->Abort();
  }
}

scoped_refptr<ServiceWorkerObject>
ServiceWorkerRegistrationObject::GetNewestWorker() {
  // Algorithm for Get Newest Worker:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#get-newest-worker
  // 1. Run the following steps atomically.
  base::AutoLock lock(mutex_);
  if (installing_worker_) {
    // 3. If registration’s installing worker is not null, set newestWorker to
    //    registration’s installing worker.
    // 6. Return newestWorker.
    return installing_worker_;
  } else if (waiting_worker_) {
    // 4. Else if registration’s waiting worker is not null, set newestWorker to
    //    registration’s waiting worker.
    // 6. Return newestWorker.
    return waiting_worker_;
  } else if (active_worker_) {
    // 5. Else if registration’s active worker is not null, set newestWorker to
    //    registration’s active worker.
    // 6. Return newestWorker.
    return active_worker_;
  }

  // 2. Let newestWorker be null.
  // 6. Return newestWorker.
  return nullptr;
}

}  // namespace worker
}  // namespace cobalt
