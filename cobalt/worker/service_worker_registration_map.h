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

#ifndef COBALT_WORKER_SERVICE_WORKER_REGISTRATION_MAP_H_
#define COBALT_WORKER_SERVICE_WORKER_REGISTRATION_MAP_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/service_worker_persistent_settings.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace worker {
class ServiceWorkerJobs;

// Algorithms for the service worker scope to registration map.
//   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-scope-to-registration-map
class ServiceWorkerRegistrationMap {
 public:
  explicit ServiceWorkerRegistrationMap(
      const ServiceWorkerPersistentSettings::Options& options);
  ~ServiceWorkerRegistrationMap() { AbortAllActive(); }

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#get-registration-algorithm
  scoped_refptr<ServiceWorkerRegistrationObject> GetRegistration(
      const url::Origin& storage_key, const GURL& scope);

  std::vector<scoped_refptr<ServiceWorkerRegistrationObject>> GetRegistrations(
      const url::Origin& storage_key);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#set-registration-algorithm
  scoped_refptr<ServiceWorkerRegistrationObject> SetRegistration(
      const url::Origin& storage_key, const GURL& scope,
      const ServiceWorkerUpdateViaCache& update_via_cache);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#scope-match-algorithm
  scoped_refptr<ServiceWorkerRegistrationObject> MatchServiceWorkerRegistration(
      const url::Origin& storage_key, const GURL& client_url);

  void RemoveRegistration(const url::Origin& storage_key, const GURL& scope);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-service-worker-registration-unregistered
  bool IsUnregistered(ServiceWorkerRegistrationObject* registration);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#on-user-agent-shutdown-algorithm
  void HandleUserAgentShutdown(ServiceWorkerJobs* jobs);

  void AbortAllActive();

  // Called from the end of ServiceWorkerJobs Install, Activate, and Clear
  // Registration since these are the cases in which a service worker
  // registration's active_worker or waiting_worker are updated.
  void PersistRegistration(const url::Origin& storage_key, const GURL& scope);

  void ReadPersistentSettings();

 private:
  // ThreadChecker for use by the methods operating on the registration map.
  THREAD_CHECKER(thread_checker_);

  // A registration map is an ordered map where the keys are (storage key,
  // serialized scope urls) and the values are service worker registrations.
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-scope-to-registration-map
  std::map<RegistrationMapKey, scoped_refptr<ServiceWorkerRegistrationObject>>
      registration_map_;

  // This lock is to allow atomic operations on the registration map.
  base::Lock mutex_;

  std::unique_ptr<ServiceWorkerPersistentSettings>
      service_worker_persistent_settings_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_REGISTRATION_MAP_H_
