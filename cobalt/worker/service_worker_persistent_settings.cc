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

bool CheckPersistentValue(
    std::string key_string, std::string settings_key,
    base::flat_map<std::string, std::unique_ptr<base::Value>>& dict,
    base::Value::Type type) {
  if (!dict.contains(settings_key)) {
    DLOG(INFO) << "Key: " << key_string << " does not contain " << settings_key;
    return false;
  } else if (!(dict[settings_key]->type() == type)) {
    DLOG(INFO) << "Key: " << key_string << " " << settings_key
               << " is of type: " << dict[settings_key]->type()
               << ", but expected type is: " << type;
    return false;
  }
  return true;
}
}  // namespace

ServiceWorkerPersistentSettings::ServiceWorkerPersistentSettings(
    const Options& options)
    : options_(options) {
  persistent_settings_.reset(new cobalt::persistent_storage::PersistentSettings(
      WorkerConsts::kSettingsJson));
  persistent_settings_->ValidatePersistentSettings();
  DCHECK(persistent_settings_);
}

void ServiceWorkerPersistentSettings::ReadServiceWorkerRegistrationMapSettings(
    std::map<RegistrationMapKey,
             scoped_refptr<ServiceWorkerRegistrationObject>>&
        registration_map) {
  std::vector<base::Value> key_list =
      persistent_settings_->GetPersistentSettingAsList(kSettingsKeyList);
  std::set<std::string> unverified_key_set;
  for (auto& key : key_list) {
    if (key.is_string()) {
      unverified_key_set.insert(key.GetString());
    }
  }
  for (auto& key_string : unverified_key_set) {
    auto dict =
        persistent_settings_->GetPersistentSettingAsDictionary(key_string);
    if (dict.empty()) {
      DLOG(INFO) << "Key: " << key_string << " does not exist in "
                 << WorkerConsts::kSettingsJson;
      continue;
    }
    if (!CheckPersistentValue(key_string, kSettingsStorageKeyKey, dict,
                              base::Value::Type::STRING))
      continue;
    url::Origin storage_key =
        url::Origin::Create(GURL(dict[kSettingsStorageKeyKey]->GetString()));

    // Only add persisted workers to the registration_map
    // if their storage_key matches the origin of the initial_url.
    if (!storage_key.IsSameOriginWith(url::Origin::Create(options_.url))) {
      continue;
    }

    if (!CheckPersistentValue(key_string, kSettingsScopeUrlKey, dict,
                              base::Value::Type::STRING))
      continue;
    GURL scope(dict[kSettingsScopeUrlKey]->GetString());

    if (!CheckPersistentValue(key_string, kSettingsUpdateViaCacheModeKey, dict,
                              base::Value::Type::INTEGER))
      continue;
    ServiceWorkerUpdateViaCache update_via_cache =
        static_cast<ServiceWorkerUpdateViaCache>(
            dict[kSettingsUpdateViaCacheModeKey]->GetInt());

    if (!CheckPersistentValue(key_string, kSettingsScopeStringKey, dict,
                              base::Value::Type::STRING))
      continue;
    std::string scope_string(dict[kSettingsScopeStringKey]->GetString());

    RegistrationMapKey key(storage_key, scope_string);
    scoped_refptr<ServiceWorkerRegistrationObject> registration(
        new ServiceWorkerRegistrationObject(storage_key, scope,
                                            update_via_cache));

    auto worker_key = kSettingsWaitingWorkerKey;
    if (!CheckPersistentValue(key_string, worker_key, dict,
                              base::Value::Type::DICT)) {
      worker_key = kSettingsActiveWorkerKey;
      if (!CheckPersistentValue(key_string, worker_key, dict,
                                base::Value::Type::DICT))
        continue;
    }
    if (!ReadServiceWorkerObjectSettings(
            registration, key_string, std::move(dict[worker_key]), worker_key))
      continue;

    if (CheckPersistentValue(key_string, kSettingsLastUpdateCheckTimeKey, dict,
                             base::Value::Type::STRING)) {
      int64_t last_update_check_time =
          std::atol(dict[kSettingsLastUpdateCheckTimeKey]->GetString().c_str());
      registration->set_last_update_check_time(
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromMicroseconds(last_update_check_time)));
    }

    key_set_.insert(key_string);
    registration_map.insert(std::make_pair(key, registration));
    registration->set_is_persisted(true);

    options_.service_worker_context->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerContext::Activate,
                       base::Unretained(options_.service_worker_context),
                       registration));

    auto job = options_.service_worker_context->jobs()->CreateJobWithoutPromise(
        ServiceWorkerJobs::JobType::kUpdate, storage_key, scope,
        registration->waiting_worker()->script_url());
    options_.service_worker_context->message_loop()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ServiceWorkerJobs::ScheduleJob,
                                  base::Unretained(
                                      options_.service_worker_context->jobs()),
                                  std::move(job)));
  }
}

bool ServiceWorkerPersistentSettings::ReadServiceWorkerObjectSettings(
    scoped_refptr<ServiceWorkerRegistrationObject> registration,
    std::string key_string, std::unique_ptr<base::Value> value_dict,
    std::string worker_key_string) {
  std::string* options_name_value =
      value_dict->FindStringKey(kSettingsOptionsNameKey);
  if (options_name_value == nullptr) return false;
  ServiceWorkerObject::Options options(*options_name_value,
                                       options_.web_settings,
                                       options_.network_module, registration);
  options.web_options.platform_info = options_.platform_info;
  options.web_options.service_worker_context = options_.service_worker_context;
  scoped_refptr<ServiceWorkerObject> worker(new ServiceWorkerObject(options));

  std::string* script_url_value =
      value_dict->FindStringKey(kSettingsScriptUrlKey);
  if (script_url_value == nullptr) return false;
  worker->set_script_url(GURL(*script_url_value));

  absl::optional<bool> skip_waiting_value =
      value_dict->FindBoolKey(kSettingsSkipWaitingKey);
  if (!skip_waiting_value.has_value()) return false;
  if (skip_waiting_value.value()) worker->set_skip_waiting();

  absl::optional<bool> classic_scripts_imported_value =
      value_dict->FindBoolKey(kSettingsClassicScriptsImportedKey);
  if (!classic_scripts_imported_value.has_value()) return false;
  if (classic_scripts_imported_value.value())
    worker->set_classic_scripts_imported();

  worker->set_start_status(nullptr);

  base::Value* used_scripts_value =
      value_dict->FindListKey(kSettingsSetOfUsedScriptsKey);
  if (used_scripts_value == nullptr) return false;
  base::Value::List used_scripts_list =
      std::move(*used_scripts_value).TakeList();
  for (int i = 0; i < used_scripts_list.size(); i++) {
    auto script_value = std::move(used_scripts_list[i]);
    if (script_value.is_string()) {
      worker->AppendToSetOfUsedScripts(GURL(script_value.GetString()));
    }
  }
  base::Value* script_urls_value =
      value_dict->FindListKey(kSettingsScriptResourceMapScriptUrlsKey);
  if (script_urls_value == nullptr) return false;
  base::Value::List script_urls_list = std::move(*script_urls_value).TakeList();
  ScriptResourceMap script_resource_map;
  for (int i = 0; i < script_urls_list.size(); i++) {
    auto script_url_value = std::move(script_urls_list[i]);
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
        std::string* raw_header_value =
            value_dict->FindStringKey(kSettingsRawHeadersKey);
        if (raw_header_value == nullptr) return false;
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
  std::vector<base::Value> key_list;
  for (auto& key : key_set_) {
    key_list.emplace_back(key);
  }
  persistent_settings_->SetPersistentSetting(kSettingsKeyList, nullptr);

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

  persistent_settings_->SetPersistentSetting(key_string, nullptr);
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
  std::vector<base::Value> key_list;
  for (auto& key : key_set_) {
    key_list.emplace_back(key);
  }
  persistent_settings_->SetPersistentSetting(kSettingsKeyList, nullptr);

  // Remove the registration dictionary.
  persistent_settings_->RemovePersistentSetting(key_string);
}

void ServiceWorkerPersistentSettings::RemoveServiceWorkerObjectSettings(
    std::string key_string) {
  auto dict =
      persistent_settings_->GetPersistentSettingAsDictionary(key_string);
  if (dict.empty()) return;
  std::vector<std::string> worker_keys{kSettingsWaitingWorkerKey,
                                       kSettingsActiveWorkerKey};
  for (std::string worker_key : worker_keys) {
    if (!CheckPersistentValue(key_string, worker_key, dict,
                              base::Value::Type::DICT))
      continue;
    auto worker_dict = std::move(dict[worker_key]);
    base::Value* script_urls_value =
        worker_dict->FindListKey(kSettingsScriptResourceMapScriptUrlsKey);
    if (script_urls_value == nullptr) return;
    base::Value::List script_urls_list =
        std::move(*script_urls_value).TakeList();

    for (int i = 0; i < script_urls_list.size(); i++) {
      auto script_url_value = std::move(script_urls_list[i]);
      if (script_url_value.is_string()) {
        auto script_url_string = script_url_value.GetString();
        cobalt::cache::Cache::GetInstance()->Delete(
            network::disk_cache::ResourceType::kServiceWorkerScript,
            web::cache_utils::GetKey(key_string + script_url_string));
      }
    }
  }
}

void ServiceWorkerPersistentSettings::RemoveAll() {
  for (auto& key : key_set_) {
    persistent_settings_->RemovePersistentSetting(key);
  }
}

void ServiceWorkerPersistentSettings::DeleteAll(base::OnceClosure closure) {
  persistent_settings_->DeletePersistentSettings(std::move(closure));
}

}  // namespace worker
}  // namespace cobalt
