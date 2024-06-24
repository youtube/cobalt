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

#include "cobalt/worker/service_worker_persistent_settings.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/network/disk_cache/resource_type.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/web/cache_utils.h"
#include "cobalt/worker/service_worker_context.h"
#include "cobalt/worker/service_worker_jobs.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "cobalt/worker/worker_consts.h"
#include "cobalt/worker/worker_global_scope.h"
#include "net/base/completion_once_callback.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace worker {

namespace {
// ServiceWorkerRegistrationMap persistent settings keys.
const char kSettingsKeyList[] = "key_list";

// ServiceWorkerRegistrationObject persistent settings keys.
const char kSettingsStorageKeyKey[] = "storage_key";
const char kSettingsScopeStringKey[] = "scope_string";
const char kSettingsScopeUrlKey[] = "scope_url";
const char kSettingsUpdateViaCacheModeKey[] = "update_via_cache_mode";
const char kSettingsWaitingWorkerKey[] = "waiting_worker";
const char kSettingsActiveWorkerKey[] = "active_worker";
const char kSettingsLastUpdateCheckTimeKey[] = "last_update_check_time";

// ServicerWorkerObject persistent settings keys.
const char kSettingsOptionsNameKey[] = "options_name";
const char kSettingsScriptUrlKey[] = "script_url";
const char kSettingsScriptResourceMapScriptUrlsKey[] =
    "script_resource_map_script_urls";
const char kSettingsSetOfUsedScriptsKey[] = "set_of_used_scripts";
const char kSettingsSkipWaitingKey[] = "skip_waiting";
const char kSettingsClassicScriptsImportedKey[] = "classic_scripts_imported";
const char kSettingsRawHeadersKey[] = "raw_headers";
}  // namespace

ServiceWorkerPersistentSettings::ServiceWorkerPersistentSettings(
    const Options& options)
    : options_(options) {
  persistent_settings_.reset(new cobalt::persistent_storage::PersistentSettings(
      WorkerConsts::kSettingsJson));
  persistent_settings_->Validate();
  DCHECK(persistent_settings_);
}

void ServiceWorkerPersistentSettings::ReadServiceWorkerRegistrationMapSettings(
    std::map<RegistrationMapKey,
             scoped_refptr<ServiceWorkerRegistrationObject>>&
        registration_map) {
  base::Value local_key_list_value;
  persistent_settings_->Get(kSettingsKeyList, &local_key_list_value);
  const base::Value::List* key_list = local_key_list_value.GetIfList();
  if (!key_list || key_list->empty()) return;
  std::set<std::string> unverified_key_set;
  for (auto& key : *key_list) {
    if (key.is_string()) {
      unverified_key_set.insert(key.GetString());
    }
  }
  for (auto& key_string : unverified_key_set) {
    base::Value local_dict_value;
    persistent_settings_->Get(key_string, &local_dict_value);
    const base::Value::Dict* dict = local_dict_value.GetIfDict();
    if (!dict || dict->empty()) {
      DLOG(INFO) << "Key: " << key_string << " does not exist in "
                 << WorkerConsts::kSettingsJson;
      continue;
    }
    auto storage_key = dict->FindString(kSettingsStorageKeyKey);
    if (!storage_key) continue;
    url::Origin storage_key_origin = url::Origin::Create(GURL(*storage_key));

    auto scope_url = dict->FindString(kSettingsScopeUrlKey);
    if (!scope_url) continue;
    GURL scope(*scope_url);

    auto update_via_cache_mode = dict->FindInt(kSettingsUpdateViaCacheModeKey);
    if (!update_via_cache_mode) continue;
    ServiceWorkerUpdateViaCache update_via_cache =
        static_cast<ServiceWorkerUpdateViaCache>(*update_via_cache_mode);

    auto scope_string = dict->FindString(kSettingsScopeStringKey);
    if (!scope_string) continue;

    RegistrationMapKey key(storage_key_origin, *scope_string);
    scoped_refptr<ServiceWorkerRegistrationObject> registration(
        new ServiceWorkerRegistrationObject(storage_key_origin, scope,
                                            update_via_cache));

    const base::Value::Dict* worker = dict->FindDict(kSettingsWaitingWorkerKey);
    if (!worker) {
      worker = dict->FindDict(kSettingsActiveWorkerKey);
      if (!worker) continue;
    }
    if (!ReadServiceWorkerObjectSettings(registration, key_string, *worker))
      continue;

    auto last_update_check_time =
        dict->FindString(kSettingsLastUpdateCheckTimeKey);
    if (last_update_check_time) {
      registration->set_last_update_check_time(
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromMicroseconds(
                  std::atol(last_update_check_time->c_str()))));
    }

    key_set_.insert(key_string);
    registration_map.insert(std::make_pair(key, registration));
    registration->set_is_persisted(true);

    options_.service_worker_context->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerContext::Activate,
                       base::Unretained(options_.service_worker_context),
                       registration));

    auto job = options_.service_worker_context->jobs()->CreateJobWithoutPromise(
        ServiceWorkerJobs::JobType::kUpdate, storage_key_origin, scope,
        registration->waiting_worker()->script_url());
    options_.service_worker_context->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ServiceWorkerJobs::ScheduleJob,
                                  base::Unretained(
                                      options_.service_worker_context->jobs()),
                                  std::move(job)));
  }
}

bool ServiceWorkerPersistentSettings::ReadServiceWorkerObjectSettings(
    scoped_refptr<ServiceWorkerRegistrationObject> registration,
    std::string key_string, const base::Value::Dict& value_dict) {
  const std::string* options_name_value =
      value_dict.FindString(kSettingsOptionsNameKey);
  if (!options_name_value) return false;
  ServiceWorkerObject::Options options(*options_name_value,
                                       options_.web_settings,
                                       options_.network_module, registration);
  options.web_options.platform_info = options_.platform_info;
  options.web_options.service_worker_context = options_.service_worker_context;
  scoped_refptr<ServiceWorkerObject> worker(new ServiceWorkerObject(options));

  const std::string* script_url_value =
      value_dict.FindString(kSettingsScriptUrlKey);
  if (!script_url_value) return false;
  worker->set_script_url(GURL(*script_url_value));

  absl::optional<bool> skip_waiting_value =
      value_dict.FindBool(kSettingsSkipWaitingKey);
  if (!skip_waiting_value) return false;
  if (skip_waiting_value.value()) worker->set_skip_waiting();

  absl::optional<bool> classic_scripts_imported_value =
      value_dict.FindBool(kSettingsClassicScriptsImportedKey);
  if (!classic_scripts_imported_value) return false;
  if (classic_scripts_imported_value.value())
    worker->set_classic_scripts_imported();

  worker->set_start_status(nullptr);

  const base::Value::List* used_scripts_list =
      value_dict.FindList(kSettingsSetOfUsedScriptsKey);
  if (!used_scripts_list) return false;
  for (const auto& script_value : *used_scripts_list) {
    if (script_value.is_string()) {
      worker->AppendToSetOfUsedScripts(GURL(script_value.GetString()));
    }
  }
  const base::Value::List* script_urls_list =
      value_dict.FindList(kSettingsScriptResourceMapScriptUrlsKey);
  if (!script_urls_list) return false;
  ScriptResourceMap script_resource_map;
  for (const auto& script_url_value : *script_urls_list) {
    if (script_url_value.is_string()) {
      auto script_url_string = script_url_value.GetString();
      auto script_url = GURL(script_url_string);
      std::unique_ptr<std::vector<uint8_t>> data =
          cobalt::cache::Cache::GetInstance()->Retrieve(
              network::disk_cache::ResourceType::kServiceWorkerScript,
              web::cache_utils::GetKey(key_string + script_url_string));
      if (data == nullptr) {
        return false;
      }
      auto script_resource = ScriptResource(std::make_unique<std::string>(
          std::string(data->begin(), data->end())));
      if (script_url == worker->script_url()) {
        // Get the persistent headers for the ServiceWorkerObject script_url_.
        // This is used in ServiceWorkerObject::Initialize().
        const std::string* raw_header_value =
            value_dict.FindString(kSettingsRawHeadersKey);
        if (!raw_header_value) return false;
        const scoped_refptr<net::HttpResponseHeaders> headers =
            scoped_refptr<net::HttpResponseHeaders>(
                new net::HttpResponseHeaders(*raw_header_value));
        script_resource.headers = headers;
      }
      auto result = script_resource_map.insert(
          std::make_pair(script_url, std::move(script_resource)));
      DCHECK(result.second);
    }
  }
  if (script_resource_map.size() == 0) {
    return false;
  }
  worker->set_script_resource_map(std::move(script_resource_map));

  registration->set_waiting_worker(worker);

  return true;
}

void ServiceWorkerPersistentSettings::
    WriteServiceWorkerRegistrationObjectSettings(
        RegistrationMapKey key,
        scoped_refptr<ServiceWorkerRegistrationObject> registration) {
  auto key_string = key.first.GetURL().spec() + key.second;
  base::Value::Dict dict;

  // https://w3c.github.io/ServiceWorker/#user-agent-shutdown
  // An installing worker does not persist, but is discarded.
  auto waiting_worker = registration->waiting_worker();
  auto active_worker = registration->active_worker();

  if (waiting_worker == nullptr && active_worker == nullptr) {
    // If the installing worker was the only service worker for the service
    // worker registration, the service worker registration is discarded.
    RemoveServiceWorkerRegistrationObjectSettings(key);
    return;
  }

  if (waiting_worker) {
    // A waiting worker promotes to an active worker. This will be handled
    // upon restart.
    dict.Set(kSettingsWaitingWorkerKey,
             WriteServiceWorkerObjectSettings(key_string, waiting_worker));
  } else {
    dict.Set(kSettingsActiveWorkerKey,
             WriteServiceWorkerObjectSettings(key_string, active_worker));
  }

  // Add key_string to the registered keys and write to persistent settings.
  key_set_.insert(key_string);
  base::Value::List key_list;
  for (auto& key : key_set_) {
    key_list.Append(key);
  }
  persistent_settings_->Set(kSettingsKeyList, base::Value(std::move(key_list)));

  // Persist ServiceWorkerRegistrationObject's fields.
  dict.Set(kSettingsStorageKeyKey, registration->storage_key().GetURL().spec());

  dict.Set(kSettingsScopeStringKey, key.second);

  dict.Set(kSettingsScopeUrlKey, registration->scope_url().spec());

  dict.Set(kSettingsUpdateViaCacheModeKey,
           registration->update_via_cache_mode());

  dict.Set(kSettingsLastUpdateCheckTimeKey,
           std::to_string(registration->last_update_check_time()
                              .ToDeltaSinceWindowsEpoch()
                              .InMicroseconds()));

  persistent_settings_->Set(key_string, base::Value(std::move(dict)));
}

base::Value::Dict
ServiceWorkerPersistentSettings::WriteServiceWorkerObjectSettings(
    std::string registration_key_string,
    const scoped_refptr<ServiceWorkerObject>& service_worker_object) {
  base::Value::Dict dict;
  DCHECK(service_worker_object);
  dict.Set(kSettingsOptionsNameKey, service_worker_object->options_name());

  dict.Set(kSettingsScriptUrlKey, service_worker_object->script_url().spec());

  dict.Set(kSettingsSkipWaitingKey, service_worker_object->skip_waiting());

  dict.Set(kSettingsClassicScriptsImportedKey,
           service_worker_object->classic_scripts_imported());

  // Persist set_of_used_scripts as a List.
  base::Value set_of_used_scripts_value(base::Value::Type::LIST);
  for (auto script_url : service_worker_object->set_of_used_scripts()) {
    set_of_used_scripts_value.GetList().Append(base::Value(script_url.spec()));
  }
  dict.Set(kSettingsSetOfUsedScriptsKey, std::move(set_of_used_scripts_value));

  // Persist the script_resource_map script urls as a List.
  base::Value script_urls_value(base::Value::Type::LIST);
  for (auto const& script_resource :
       service_worker_object->script_resource_map()) {
    std::string script_url_string = script_resource.first.spec();
    script_urls_value.GetList().Append(base::Value(script_url_string));
    // Use Cache::Store to persist the script resource.
    std::string resource = *(script_resource.second.content.get());
    std::vector<uint8_t> data(resource.begin(), resource.end());
    cobalt::cache::Cache::GetInstance()->Store(
        network::disk_cache::ResourceType::kServiceWorkerScript,
        web::cache_utils::GetKey(registration_key_string + script_url_string),
        data,
        /* metadata */ base::nullopt);

    if (script_url_string == service_worker_object->script_url().spec()) {
      // Persist the raw headers from the ServiceWorkerObject script_url_
      // ScriptResource headers.
      dict.Set(kSettingsRawHeadersKey,
               script_resource.second.headers->raw_headers());
    }
  }
  dict.Set(kSettingsScriptResourceMapScriptUrlsKey,
           std::move(script_urls_value));

  return std::move(dict);
}

void ServiceWorkerPersistentSettings::
    RemoveServiceWorkerRegistrationObjectSettings(RegistrationMapKey key) {
  auto key_string = key.first.GetURL().spec() + key.second;

  if (key_set_.find(key_string) == key_set_.end()) {
    // The key does not exist in PersistentSettings.
    return;
  }

  // Remove the worker script_resource_map from the Cache.
  RemoveServiceWorkerObjectSettings(key_string);

  // Remove registration key string.
  key_set_.erase(key_string);
  base::Value::List key_list;
  for (auto& key : key_set_) {
    key_list.Append(key);
  }
  persistent_settings_->Set(kSettingsKeyList, base::Value(std::move(key_list)));

  // Remove the registration dictionary.
  persistent_settings_->Remove(key_string);
}

void ServiceWorkerPersistentSettings::RemoveServiceWorkerObjectSettings(
    std::string key_string) {
  base::Value local_dict_value;
  persistent_settings_->Get(key_string, &local_dict_value);
  const base::Value::Dict* dict = local_dict_value.GetIfDict();
  if (!dict || dict->empty()) return;
  std::vector<std::string> worker_keys{kSettingsWaitingWorkerKey,
                                       kSettingsActiveWorkerKey};
  for (std::string worker_key : worker_keys) {
    const base::Value::Dict* worker = dict->FindDict(worker_key);
    if (!worker) continue;
    const base::Value::List* script_urls =
        worker->FindList(kSettingsScriptResourceMapScriptUrlsKey);
    if (!script_urls) return;

    for (const auto& script_url : *script_urls) {
      auto script_url_string = script_url.GetIfString();
      if (script_url_string) {
        cobalt::cache::Cache::GetInstance()->Delete(
            network::disk_cache::ResourceType::kServiceWorkerScript,
            web::cache_utils::GetKey(key_string + *script_url_string));
      }
    }
  }
}

void ServiceWorkerPersistentSettings::RemoveAll() {
  for (auto& key : key_set_) {
    persistent_settings_->Remove(key);
  }
}

void ServiceWorkerPersistentSettings::DeleteAll() {
  persistent_settings_->RemoveAll();
}

}  // namespace worker
}  // namespace cobalt
