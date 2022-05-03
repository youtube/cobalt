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
      update_via_cache_mode_(update_via_cache_mode) {}

}  // namespace worker
}  // namespace cobalt
