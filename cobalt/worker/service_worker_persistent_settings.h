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

#ifndef COBALT_WORKER_SERVICE_WORKER_PERSISTENT_SETTINGS_H_
#define COBALT_WORKER_SERVICE_WORKER_PERSISTENT_SETTINGS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cobalt/cache/cache.h"
#include "cobalt/network/network_module.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/web/web_settings.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace worker {

class ServiceWorkerPersistentSettings {
 public:
  struct Options {
    Options(web::WebSettings* web_settings,
            network::NetworkModule* network_module,
            web::UserAgentPlatformInfo* platform_info,
            ServiceWorkerContext* service_worker_context, const GURL& url)
        : web_settings(web_settings),
          network_module(network_module),
          platform_info(platform_info),
          service_worker_context(service_worker_context),
          url(url) {}
    web::WebSettings* web_settings;
    network::NetworkModule* network_module;
    web::UserAgentPlatformInfo* platform_info;
    ServiceWorkerContext* service_worker_context;
    const GURL& url;
  };

  explicit ServiceWorkerPersistentSettings(const Options& options);

  void ReadServiceWorkerRegistrationMapSettings(
      std::map<RegistrationMapKey,
               scoped_refptr<ServiceWorkerRegistrationObject>>&
          registration_map);

  bool ReadServiceWorkerObjectSettings(
      scoped_refptr<ServiceWorkerRegistrationObject> registration,
      std::string key_string, std::unique_ptr<base::Value> value_dict,
      std::string worker_type_string);

  void WriteServiceWorkerRegistrationObjectSettings(
      RegistrationMapKey key,
      scoped_refptr<ServiceWorkerRegistrationObject> registration);

  base::Value::Dict WriteServiceWorkerObjectSettings(
      std::string registration_key_string,
      const scoped_refptr<ServiceWorkerObject>& service_worker_object);

  void RemoveServiceWorkerRegistrationObjectSettings(RegistrationMapKey key);

  void RemoveServiceWorkerObjectSettings(std::string key_string);

  void RemoveAll();

  void DeleteAll(base::OnceClosure closure);

 private:
  Options options_;

  std::unique_ptr<cobalt::persistent_storage::PersistentSettings>
      persistent_settings_;

  std::set<std::string> key_set_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_PERSISTENT_SETTINGS_H_
