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

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "url/gurl.h"
#include "url/origin.h"


namespace cobalt {
namespace worker {

namespace {
// Returns the serialized URL excluding the fragment.
std::string SerializeExcludingFragment(const GURL& url) {
  url::Replacements<char> replacements;
  replacements.ClearRef();
  GURL no_fragment_url = url.ReplaceComponents(replacements);
  DCHECK(!no_fragment_url.has_ref() || no_fragment_url.ref().empty());
  DCHECK(!no_fragment_url.is_empty());
  return no_fragment_url.spec();
}
}  // namespace

worker::ServiceWorkerRegistrationObject*
ServiceWorkerRegistrationMap::MatchServiceWorkerRegistration(
    const url::Origin& storage_key, const GURL& client_url) {
  TRACE_EVENT0(
      "cobalt::worker",
      "ServiceWorkerRegistrationMap::MatchServiceWorkerRegistration()");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Algorithm for Match Service Worker Registration:
  //   https://w3c.github.io/ServiceWorker/#scope-match-algorithm
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

ServiceWorkerRegistrationObject* ServiceWorkerRegistrationMap::GetRegistration(
    const url::Origin& storage_key, const GURL& scope) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerRegistrationMap::GetRegistration()");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Algorithm for Get Registration:
  //   https://w3c.github.io/ServiceWorker/#get-registration-algorithm

  // 1. Run the following steps atomically.
  base::AutoLock lock(mutex_);

  // 2. Let scopeString be the empty string.
  std::string scope_string;

  // 3. If scope is not null, set scopeString to serialized scope with the
  // exclude fragment flag set.
  if (!scope.is_empty()) {
    scope_string = SerializeExcludingFragment(scope);
  }

  Key registration_key(storage_key, scope_string);
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

ServiceWorkerRegistrationObject* ServiceWorkerRegistrationMap::SetRegistration(
    const url::Origin& storage_key, const GURL& scope,
    const ServiceWorkerUpdateViaCache& update_via_cache) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerRegistrationMap::SetRegistration()");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Algorithm for Set Registration:
  //   https://w3c.github.io/ServiceWorker/#set-registration-algorithm

  // 1. Run the following steps atomically.
  base::AutoLock lock(mutex_);

  // 2. Let scopeString be serialized scope with the exclude fragment flag set.
  std::string scope_string = SerializeExcludingFragment(scope);

  // 3. Let registration be a new service worker registration whose storage key
  // is set to storage key, scope url is set to scope, and update via cache mode
  // is set to updateViaCache.
  ServiceWorkerRegistrationObject* registration(
      new ServiceWorkerRegistrationObject(storage_key, scope,
                                          update_via_cache));

  // 4. Set registration map[(storage key, scopeString)] to registration.
  Key registration_key(storage_key, scope_string);
  registration_map_.insert(std::make_pair(
      registration_key,
      std::unique_ptr<ServiceWorkerRegistrationObject>(registration)));

  // 5. Return registration.
  return registration;
}

void ServiceWorkerRegistrationMap::RemoveRegistration(
    const url::Origin& storage_key, const GURL& scope) {
  std::string scope_string = SerializeExcludingFragment(scope);
  Key registration_key(storage_key, scope_string);
  auto entry = registration_map_.find(registration_key);
  DCHECK(entry != registration_map_.end());
  if (entry != registration_map_.end()) {
    registration_map_.erase(entry);
  }
}

}  // namespace worker
}  // namespace cobalt
