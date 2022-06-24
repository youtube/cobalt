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

#include "cobalt/cache/cache.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/extension/javascript_cache.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "net/disk_cache/cobalt/cobalt_backend_impl.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"

namespace {

base::Optional<uint32_t> GetMaxCacheStorageInBytes(
    disk_cache::ResourceType resource_type) {
  switch (resource_type) {
    case disk_cache::ResourceType::kCompiledScript:
      return 5u << 20;  // 5MiB
    default:
      return base::nullopt;
  }
}

base::Optional<uint32_t> GetMinSizeToCacheInBytes(
    disk_cache::ResourceType resource_type) {
  switch (resource_type) {
    case disk_cache::ResourceType::kCompiledScript:
      return 4096u;
    default:
      return base::nullopt;
  }
}

base::Optional<std::string> GetSubdirectory(
    disk_cache::ResourceType resource_type) {
  switch (resource_type) {
    case disk_cache::ResourceType::kCompiledScript:
      return "compiled_js";
    default:
      return base::nullopt;
  }
}

base::Optional<base::FilePath> GetCacheDirectory(
    disk_cache::ResourceType resource_type) {
  auto subdirectory = GetSubdirectory(resource_type);
  if (!subdirectory) {
    return base::nullopt;
  }
  std::vector<char> path(kSbFileMaxPath, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       kSbFileMaxPath)) {
    return base::nullopt;
  }
  return base::FilePath(path.data()).Append(subdirectory.value());
}

const CobaltExtensionJavaScriptCacheApi* GetJavaScriptCacheExtension() {
  const CobaltExtensionJavaScriptCacheApi* javascript_cache_extension =
      static_cast<const CobaltExtensionJavaScriptCacheApi*>(
          SbSystemGetExtension(kCobaltExtensionJavaScriptCacheName));
  if (javascript_cache_extension &&
      strcmp(javascript_cache_extension->name,
             kCobaltExtensionJavaScriptCacheName) == 0 &&
      javascript_cache_extension->version >= 1) {
    return javascript_cache_extension;
  }
  return nullptr;
}

}  // namespace

namespace cobalt {
namespace cache {

// static
Cache* Cache::GetInstance() {
  return base::Singleton<Cache, base::LeakySingletonTraits<Cache>>::get();
}

void Cache::Delete(disk_cache::ResourceType resource_type, uint32_t key) {
  auto* memory_capped_directory = GetMemoryCappedDirectory(resource_type);
  if (memory_capped_directory) {
    memory_capped_directory->Delete(key);
  }
}

std::unique_ptr<std::vector<uint8_t>> Cache::Retrieve(
    disk_cache::ResourceType resource_type, uint32_t key,
    std::function<std::unique_ptr<std::vector<uint8_t>>()> generate) {
  base::ScopedClosureRunner notifier(base::BindOnce(
      &Cache::Notify, base::Unretained(this), resource_type, key));
  auto* e = GetWaitableEvent(resource_type, key);
  if (e) {
    e->Wait();
    delete e;
  }

  if (resource_type == disk_cache::ResourceType::kCompiledScript) {
    const CobaltExtensionJavaScriptCacheApi* javascript_cache_extension =
        GetJavaScriptCacheExtension();
    if (javascript_cache_extension) {
      const uint8_t* cache_data_buf = nullptr;
      int cache_data_size = -1;
      if (javascript_cache_extension->GetCachedScript(key, 0, &cache_data_buf,
                                                      &cache_data_size)) {
        auto data = std::make_unique<std::vector<uint8_t>>(
            cache_data_buf, cache_data_buf + cache_data_size);
        javascript_cache_extension->ReleaseCachedScriptData(cache_data_buf);
        return data;
      }
      auto data = generate();
      if (data) {
        javascript_cache_extension->StoreCachedScript(
            key, data->size(), data->data(), data->size());
      }
      return data;
    }
  }

  auto* memory_capped_directory = GetMemoryCappedDirectory(resource_type);
  if (memory_capped_directory) {
    auto data = memory_capped_directory->Retrieve(key);
    if (data) {
      return data;
    }
  }
  auto data = generate();
  if (data) {
    TryStore(resource_type, key, *data);
  }
  return data;
}

void Cache::set_enabled(bool enabled) { enabled_ = enabled; }

void Cache::set_persistent_settings(
    persistent_storage::PersistentSettings* persistent_settings) {
  persistent_settings_ = persistent_settings;

  // Guaranteed to be called before any calls to Retrieve()
  // since set_persistent_settings() is called from the Application()
  // constructor before the NetworkModule is initialized.
  set_enabled(persistent_settings_->GetPersistentSettingAsBool(
      disk_cache::kCacheEnabledPersistentSettingsKey, true));
}

MemoryCappedDirectory* Cache::GetMemoryCappedDirectory(
    disk_cache::ResourceType resource_type) {
  base::AutoLock auto_lock(lock_);
  auto it = memory_capped_directories_.find(resource_type);
  if (it != memory_capped_directories_.end()) {
    return it->second.get();
  }

  auto cache_directory = GetCacheDirectory(resource_type);
  auto max_size = GetMaxCacheStorageInBytes(resource_type);
  if (!cache_directory || !max_size) {
    return nullptr;
  }

  auto memory_capped_directory =
      MemoryCappedDirectory::Create(cache_directory.value(), max_size.value());
  memory_capped_directories_[resource_type] =
      std::move(memory_capped_directory);
  return memory_capped_directories_[resource_type].get();
}

base::WaitableEvent* Cache::GetWaitableEvent(
    disk_cache::ResourceType resource_type, uint32_t key) {
  base::AutoLock auto_lock(lock_);
  if (pending_.find(resource_type) == pending_.end()) {
    pending_[resource_type] =
        std::map<uint32_t, std::vector<base::WaitableEvent*>>();
  }
  if (pending_[resource_type].find(key) == pending_[resource_type].end()) {
    pending_[resource_type][key] = std::vector<base::WaitableEvent*>();
    return nullptr;
  }
  auto* e = new base::WaitableEvent;
  pending_[resource_type][key].push_back(e);
  return e;
}

void Cache::Notify(disk_cache::ResourceType resource_type, uint32_t key) {
  base::AutoLock auto_lock(lock_);
  if (pending_.find(resource_type) == pending_.end()) {
    return;
  }
  if (pending_[resource_type].find(key) == pending_[resource_type].end()) {
    return;
  }
  for (auto* waitable_event : pending_[resource_type][key]) {
    waitable_event->Signal();
  }
  pending_[resource_type][key].clear();
  pending_[resource_type].erase(key);
}

void Cache::TryStore(disk_cache::ResourceType resource_type, uint32_t key,
                     const std::vector<uint8_t>& data) {
  if (!CanCache(resource_type, data.size())) {
    return;
  }
  auto* memory_capped_directory = GetMemoryCappedDirectory(resource_type);
  if (memory_capped_directory) {
    memory_capped_directory->Store(key, data);
  }
}

bool Cache::CanCache(disk_cache::ResourceType resource_type,
                     uint32_t data_size) {
  return enabled_ &&
         cobalt::configuration::Configuration::GetInstance()
             ->CobaltCanStoreCompiledJavascript() &&
         data_size > 0u &&
         data_size >= GetMinSizeToCacheInBytes(resource_type) &&
         data_size <= GetMaxCacheStorageInBytes(resource_type);
}

}  // namespace cache
}  // namespace cobalt
