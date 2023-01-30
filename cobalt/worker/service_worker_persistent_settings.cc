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
#include "cobalt/persistent_storage/persistent_settings.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/web/cache_utils.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/service_worker_jobs.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "cobalt/worker/worker_global_scope.h"
#include "net/base/completion_once_callback.h"
#include "net/disk_cache/cobalt/resource_type.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace worker {

namespace {
// ServiceWorkerRegistrationMap persistent settings keys.
const char kSettingsJson[] = "service_worker_settings.json";
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
  persistent_settings_.reset(
      new cobalt::persistent_storage::PersistentSettings(kSettingsJson));
  persistent_settings_->ValidatePersistentSettings();
  DCHECK(persistent_settings_);

  cache_.reset(cobalt::cache::Cache::GetInstance());
  DCHECK(cache_);
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
                 << kSettingsJson;
      continue;
    }
    if (!CheckPersistentValue(key_string, kSettingsStorageKeyKey, dict,
                              base::Value::Type::STRING))
      continue;
    url::Origin storage_key =
        url::Origin::Create(GURL(dict[kSettingsStorageKeyKey]->GetString()));

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
                              base::Value::Type::DICTIONARY)) {
      worker_key = kSettingsActiveWorkerKey;
      if (!CheckPersistentValue(key_string, worker_key, dict,
                                base::Value::Type::DICTIONARY))
        continue;
    }
    if (!ReadServiceWorkerObjectSettings(
            registration, key_string, std::move(dict[worker_key]), worker_key))
      continue;

    if (CheckPersistentValue(key_string, kSettingsLastUpdateCheckTimeKey, dict,
                             base::Value::Type::DOUBLE)) {
      double last_update_check_time =
          dict[kSettingsLastUpdateCheckTimeKey]->GetDouble();
      registration->set_last_update_check_time(
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromMicroseconds(
                  (int64_t)dict[kSettingsLastUpdateCheckTimeKey]
                      ->GetDouble())));
    }

    // Persisted registration and worker are valid, add the registration
    // to the registration_map and key_set_.
    key_set_.insert(key_string);
    registration_map.insert(std::make_pair(key, registration));

    // TODO(b/228904017)
    // Not in spec. Run SoftUpdate on the new registration.
    options_.service_worker_jobs->SoftUpdate(registration);
  }
}

bool ServiceWorkerPersistentSettings::ReadServiceWorkerObjectSettings(
    scoped_refptr<ServiceWorkerRegistrationObject> registration,
    std::string key_string, std::unique_ptr<base::Value> value_dict,
    std::string worker_key_string) {
  base::Value* options_name_value = value_dict->FindKeyOfType(
      kSettingsOptionsNameKey, base::Value::Type::STRING);
  if (options_name_value == nullptr) return false;
  ServiceWorkerObject::Options options(options_name_value->GetString(),
                                       options_.web_settings,
                                       options_.network_module, registration);
  options.web_options.platform_info = options_.platform_info;
  options.web_options.service_worker_jobs = options_.service_worker_jobs;
  scoped_refptr<ServiceWorkerObject> worker(new ServiceWorkerObject(options));

  base::Value* script_url_value = value_dict->FindKeyOfType(
      kSettingsScriptUrlKey, base::Value::Type::STRING);
  if (script_url_value == nullptr) return false;
  worker->set_script_url(GURL(script_url_value->GetString()));

  base::Value* skip_waiting_value = value_dict->FindKeyOfType(
      kSettingsSkipWaitingKey, base::Value::Type::BOOLEAN);
  if (skip_waiting_value == nullptr) return false;
  if (skip_waiting_value->GetBool()) worker->set_skip_waiting();

  base::Value* classic_scripts_imported_value = value_dict->FindKeyOfType(
      kSettingsClassicScriptsImportedKey, base::Value::Type::BOOLEAN);
  if (classic_scripts_imported_value == nullptr) return false;
  if (classic_scripts_imported_value->GetBool())
    worker->set_classic_scripts_imported();

  worker->set_start_status(nullptr);

  base::Value* used_scripts_value = value_dict->FindKeyOfType(
      kSettingsSetOfUsedScriptsKey, base::Value::Type::LIST);
  if (used_scripts_value == nullptr) return false;
  std::vector<base::Value> used_scripts_list = used_scripts_value->TakeList();
  for (int i = 0; i < used_scripts_list.size(); i++) {
    auto script_value = std::move(used_scripts_list[i]);
    if (script_value.is_string()) {
      worker->AppendToSetOfUsedScripts(GURL(script_value.GetString()));
    }
  }
  base::Value* script_urls_value = value_dict->FindKeyOfType(
      kSettingsScriptResourceMapScriptUrlsKey, base::Value::Type::LIST);
  if (script_urls_value == nullptr) return false;
  std::vector<base::Value> script_urls_list = script_urls_value->TakeList();
  ScriptResourceMap script_resource_map;
  for (int i = 0; i < script_urls_list.size(); i++) {
    auto script_url_value = std::move(script_urls_list[i]);
    if (script_url_value.is_string()) {
      auto script_url_string = script_url_value.GetString();
      auto script_url = GURL(script_url_string);
      std::unique_ptr<std::vector<uint8_t>> data = cache_->Retrieve(
          disk_cache::ResourceType::kServiceWorkerScript,
          web::cache_utils::GetKey(key_string + script_url_string));
      if (data == nullptr) {
        return false;
      }
      std::string script_string(data->begin(), data->end());
      auto result = script_resource_map.insert(std::make_pair(
          script_url,
          ScriptResource(std::make_unique<std::string>(script_string))));
      DCHECK(result.second);
    }
  }
  if (script_resource_map.size() == 0) {
    return false;
  }
  worker->set_script_resource_map(std::move(script_resource_map));

  options_.service_worker_jobs->UpdateWorkerState(worker,
                                                  kServiceWorkerStateActivated);
  registration->set_active_worker(worker);

  return true;
}

void ServiceWorkerPersistentSettings::
    WriteServiceWorkerRegistrationObjectSettings(
        RegistrationMapKey key,
        scoped_refptr<ServiceWorkerRegistrationObject> registration) {
  auto key_string = key.first.GetURL().spec() + key.second;
  base::flat_map<std::string, std::unique_ptr<base::Value>> dict;

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
    dict.try_emplace(
        kSettingsWaitingWorkerKey,
        WriteServiceWorkerObjectSettings(key_string, waiting_worker));
  } else {
    dict.try_emplace(kSettingsActiveWorkerKey, WriteServiceWorkerObjectSettings(
                                                   key_string, active_worker));
  }

  // Add key_string to the registered keys and write to persistent settings.
  key_set_.insert(key_string);
  std::vector<base::Value> key_list;
  for (auto& key : key_set_) {
    key_list.emplace_back(key);
  }
  persistent_settings_->SetPersistentSetting(
      kSettingsKeyList, std::make_unique<base::Value>(std::move(key_list)));

  // Persist ServiceWorkerRegistrationObject's fields.
  dict.try_emplace(kSettingsStorageKeyKey,
                   std::make_unique<base::Value>(
                       registration->storage_key().GetURL().spec()));

  dict.try_emplace(kSettingsScopeStringKey,
                   std::make_unique<base::Value>(key.second));

  dict.try_emplace(kSettingsScopeUrlKey, std::make_unique<base::Value>(
                                             registration->scope_url().spec()));

  dict.try_emplace(
      kSettingsUpdateViaCacheModeKey,
      std::make_unique<base::Value>(registration->update_via_cache_mode()));

  dict.try_emplace(kSettingsLastUpdateCheckTimeKey,
                   std::make_unique<base::Value>(static_cast<double>(
                       registration->last_update_check_time()
                           .ToDeltaSinceWindowsEpoch()
                           .InMicroseconds())));

  persistent_settings_->SetPersistentSetting(
      key_string, std::make_unique<base::Value>(dict));
}

std::unique_ptr<base::Value>
ServiceWorkerPersistentSettings::WriteServiceWorkerObjectSettings(
    std::string registration_key_string,
    const scoped_refptr<ServiceWorkerObject>& service_worker_object) {
  base::flat_map<std::string, std::unique_ptr<base::Value>> dict;
  DCHECK(service_worker_object);
  dict.try_emplace(
      kSettingsOptionsNameKey,
      std::make_unique<base::Value>(service_worker_object->options_name()));

  dict.try_emplace(kSettingsScriptUrlKey,
                   std::make_unique<base::Value>(
                       service_worker_object->script_url().spec()));

  dict.try_emplace(
      kSettingsSkipWaitingKey,
      std::make_unique<base::Value>(service_worker_object->skip_waiting()));

  dict.try_emplace(kSettingsClassicScriptsImportedKey,
                   std::make_unique<base::Value>(
                       service_worker_object->classic_scripts_imported()));

  // Persist set_of_used_scripts as a List.
  base::Value set_of_used_scripts_value(base::Value::Type::LIST);
  for (auto script_url : service_worker_object->set_of_used_scripts()) {
    set_of_used_scripts_value.GetList().push_back(
        base::Value(script_url.spec()));
  }
  dict.try_emplace(
      kSettingsSetOfUsedScriptsKey,
      std::make_unique<base::Value>(std::move(set_of_used_scripts_value)));

  // Persist the script_resource_map script urls as a List.
  base::Value script_urls_value(base::Value::Type::LIST);
  for (auto const& script_resource :
       service_worker_object->script_resource_map()) {
    std::string script_url_string = script_resource.first.spec();
    script_urls_value.GetList().push_back(base::Value(script_url_string));
    // Use Cache::Store to persist the script resource.
    std::string resource = *(script_resource.second.content.get());
    std::vector<uint8_t> data(resource.begin(), resource.end());
    cache_->Store(
        disk_cache::ResourceType::kServiceWorkerScript,
        web::cache_utils::GetKey(registration_key_string + script_url_string),
        data,
        /* metadata */ base::nullopt);
  }
  dict.try_emplace(kSettingsScriptResourceMapScriptUrlsKey,
                   std::make_unique<base::Value>(std::move(script_urls_value)));

  return std::move(std::make_unique<base::Value>(dict));
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
  persistent_settings_->SetPersistentSetting(
      kSettingsKeyList, std::make_unique<base::Value>(std::move(key_list)));

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
                              base::Value::Type::DICTIONARY))
      continue;
    auto worker_dict = std::move(dict[worker_key]);
    base::Value* script_urls_value = worker_dict->FindKeyOfType(
        kSettingsScriptResourceMapScriptUrlsKey, base::Value::Type::LIST);
    if (script_urls_value == nullptr) return;
    std::vector<base::Value> script_urls_list = script_urls_value->TakeList();

    for (int i = 0; i < script_urls_list.size(); i++) {
      auto script_url_value = std::move(script_urls_list[i]);
      if (script_url_value.is_string()) {
        auto script_url_string = script_url_value.GetString();
        cache_->Delete(
            disk_cache::ResourceType::kServiceWorkerScript,
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

}  // namespace worker
}  // namespace cobalt
