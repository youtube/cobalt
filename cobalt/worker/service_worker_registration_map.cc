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

#include "cobalt/worker/service_worker_registration_map.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/worker/service_worker_context.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "url/gurl.h"
#include "url/origin.h"


namespace cobalt {
namespace worker {

namespace {

// Returns the serialized URL excluding the fragment.
std::string SerializeExcludingFragment(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  return std::string(
      base::TrimString(url.ReplaceComponents(replacements).spec(), "/",
                       base::TrimPositions::TRIM_TRAILING));
}

}  // namespace

ServiceWorkerRegistrationMap::ServiceWorkerRegistrationMap(
    const ServiceWorkerPersistentSettings::Options& options) {
  service_worker_persistent_settings_.reset(
      new ServiceWorkerPersistentSettings(options));
  DCHECK(service_worker_persistent_settings_);

  ReadPersistentSettings();
}

void ServiceWorkerRegistrationMap::ReadPersistentSettings() {
  service_worker_persistent_settings_->ReadServiceWorkerRegistrationMapSettings(
      registration_map_);
}

void ServiceWorkerRegistrationMap::DeletePersistentSettings() {
  base::OnceClosure closure = base::BindOnce(
      [](std::map<RegistrationMapKey,
                  scoped_refptr<ServiceWorkerRegistrationObject>>*
             registration_map) { (*registration_map).clear(); },
      &registration_map_);
  service_worker_persistent_settings_->DeleteAll(std::move(closure));
}

scoped_refptr<ServiceWorkerRegistrationObject>
ServiceWorkerRegistrationMap::MatchServiceWorkerRegistration(
    const url::Origin& storage_key, const GURL& client_url) {
  TRACE_EVENT0(
      "cobalt::worker",
      "ServiceWorkerRegistrationMap::MatchServiceWorkerRegistration()");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Algorithm for Match Service Worker Registration:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#scope-match-algorithm
  GURL matching_scope;

  // 1. Run the following steps atomically.
  {
    base::AutoLock lock(mutex_);

    // 2. Let clientURLString be serialized clientURL.
    std::string client_url_string(client_url.spec());

    // 3. Let matchingScopeString be the empty string.
    std::string matching_scope_string;

    // 4. Let scopeStringSet be an empty list.
    std::list<std::string> scope_string_set;

    // 5. For each (entry storage key, entry scope) → registration of
    // registration map:
    for (const auto& entry : registration_map_) {
      // 5.1. If storage key equals entry storage key, then append entry scope
      // to the end of scopeStringSet.
      if (entry.first.first == storage_key) {
        scope_string_set.push_back(entry.first.second);
      }
    }

    // 6. Set matchingScopeString to the longest value in scopeStringSet which
    // the value of clientURLString starts with, if it exists.
    for (const auto& scope_string : scope_string_set) {
      // TODO(b/234659851): Verify whether this is the expected behavior, where
      // a substring of the scope string is compared with the client url string.
      bool starts_with =
          client_url_string.substr(0, scope_string.length()) == scope_string;
      if (starts_with &&
          (scope_string.length() > matching_scope_string.length())) {
        matching_scope_string = scope_string;
      }
    }

    // 7. Let matchingScope be null.
    // 8. If matchingScopeString is not the empty string, then:
    if (!matching_scope_string.empty()) {
      // 8.1. Set matchingScope to the result of parsing matchingScopeString.
      matching_scope = GURL(matching_scope_string);

      // 8.2. Assert: matchingScope’s origin and clientURL’s origin are same
      // origin.
      DCHECK_EQ(url::Origin::Create(matching_scope),
                url::Origin::Create(client_url));
    }
  }
  // 9. Return the result of running Get Registration given storage key and
  // matchingScope.
  return GetRegistration(storage_key, matching_scope);
}

scoped_refptr<ServiceWorkerRegistrationObject>
ServiceWorkerRegistrationMap::GetRegistration(const url::Origin& storage_key,
                                              const GURL& scope) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerRegistrationMap::GetRegistration()");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Algorithm for Get Registration:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#get-registration-algorithm

  // 1. Run the following steps atomically.
  base::AutoLock lock(mutex_);

  // 2. Let scopeString be the empty string.
  std::string scope_string;

  // 3. If scope is not null, set scopeString to serialized scope with the
  // exclude fragment flag set.
  if (!scope.is_empty()) {
    scope_string = SerializeExcludingFragment(scope);
  }

  RegistrationMapKey registration_key(storage_key, scope_string);
  // 4. For each (entry storage key, entry scope) → registration of registration
  // map:
  for (const auto& entry : registration_map_) {
    // 4.1. If storage key equals entry storage key and scopeString matches
    // entry scope, then return registration.
    if (entry.first == registration_key) {
      return entry.second.get();
    }
  }

  // 5. Return null.
  return nullptr;
}

std::vector<scoped_refptr<ServiceWorkerRegistrationObject>>
ServiceWorkerRegistrationMap::GetRegistrations(const url::Origin& storage_key) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerRegistrationMap::GetRegistrations()");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock lock(mutex_);
  std::vector<scoped_refptr<ServiceWorkerRegistrationObject>> result;
  for (const auto& entry : registration_map_) {
    if (entry.first.first == storage_key) {
      result.push_back(std::move(entry.second));
    }
  }
  return result;
}

scoped_refptr<ServiceWorkerRegistrationObject>
ServiceWorkerRegistrationMap::SetRegistration(
    const url::Origin& storage_key, const GURL& scope,
    const ServiceWorkerUpdateViaCache& update_via_cache) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerRegistrationMap::SetRegistration()");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Algorithm for Set Registration:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#set-registration-algorithm

  // 1. Run the following steps atomically.
  base::AutoLock lock(mutex_);

  // 2. Let scopeString be serialized scope with the exclude fragment flag set.
  std::string scope_string = SerializeExcludingFragment(scope);

  // 3. Let registration be a new service worker registration whose storage key
  // is set to storage key, scope url is set to scope, and update via cache mode
  // is set to updateViaCache.
  scoped_refptr<ServiceWorkerRegistrationObject> registration(
      new ServiceWorkerRegistrationObject(storage_key, scope,
                                          update_via_cache));

  // 4. Set registration map[(storage key, scopeString)] to registration.
  RegistrationMapKey registration_key(storage_key, scope_string);
  registration_map_.insert(std::make_pair(
      registration_key,
      scoped_refptr<ServiceWorkerRegistrationObject>(registration)));

  // 5. Return registration.
  return registration;
}

void ServiceWorkerRegistrationMap::RemoveRegistration(
    const url::Origin& storage_key, const GURL& scope) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::string scope_string = SerializeExcludingFragment(scope);
  RegistrationMapKey registration_key(storage_key, scope_string);
  auto entry = registration_map_.find(registration_key);
  DCHECK(entry != registration_map_.end());
  if (entry != registration_map_.end()) {
    registration_map_.erase(entry);
    service_worker_persistent_settings_
        ->RemoveServiceWorkerRegistrationObjectSettings(registration_key);
  }
}

bool ServiceWorkerRegistrationMap::IsUnregistered(
    ServiceWorkerRegistrationObject* registration) {
  // A service worker registration is said to be unregistered if registration
  // map[this service worker registration's (storage key, serialized scope url)]
  // is not this service worker registration.
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-service-worker-registration-unregistered
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!registration) return true;
  std::string scope_string =
      SerializeExcludingFragment(registration->scope_url());
  RegistrationMapKey registration_key(registration->storage_key(),
                                      scope_string);
  auto entry = registration_map_.find(registration_key);
  if (entry == registration_map_.end()) return true;

  return entry->second.get() != registration;
}

void ServiceWorkerRegistrationMap::HandleUserAgentShutdown(
    ServiceWorkerContext* context) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerRegistrationMap::HandleUserAgentShutdown()");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Algorithm for Handle User Agent Shutdown:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#on-user-agent-shutdown-algorithm

  // 1. For each (storage key, scope) -> registration of registration map:
  for (auto& entry : registration_map_) {
    const scoped_refptr<ServiceWorkerRegistrationObject>& registration =
        entry.second;
    // 1.1. If registration’s installing worker installingWorker is not null,
    // then:
    if (registration->installing_worker()) {
      // 1.1.1. If registration’s waiting worker is null and registration’s
      // active worker is null, invoke Clear Registration with registration and
      // continue to the next iteration of the loop.
      if (!registration->waiting_worker() && !registration->active_worker()) {
        context->ClearRegistration(registration);
        continue;
      } else {
        // 1.1.2. Else, set installingWorker to null.
        registration->set_installing_worker(nullptr);
      }
    }

    if (registration->waiting_worker()) {
      // 1.2. If registration’s waiting worker is not null, run the following
      // substep in parallel:

      // 1.2.1. Invoke Activate with registration.
      context->Activate(registration);
    }
  }
}

void ServiceWorkerRegistrationMap::AbortAllActive() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto entry : registration_map_) {
    entry.second->AbortAll();
  }
}

void ServiceWorkerRegistrationMap::PersistRegistration(
    const url::Origin& storage_key, const GURL& scope) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::string scope_string = SerializeExcludingFragment(scope);
  RegistrationMapKey registration_key(storage_key, scope_string);
  auto entry = registration_map_.find(registration_key);
  if (entry != registration_map_.end()) {
    service_worker_persistent_settings_
        ->WriteServiceWorkerRegistrationObjectSettings(registration_key,
                                                       entry->second);
  } else {
    service_worker_persistent_settings_
        ->RemoveServiceWorkerRegistrationObjectSettings(registration_key);
  }
}

}  // namespace worker
}  // namespace cobalt
