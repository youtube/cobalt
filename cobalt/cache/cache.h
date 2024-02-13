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

#ifndef COBALT_CACHE_CACHE_H_
#define COBALT_CACHE_CACHE_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "cobalt/cache/memory_capped_directory.h"
#include "cobalt/network/disk_cache/resource_type.h"
#include "cobalt/persistent_storage/persistent_settings.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace cobalt {
namespace cache {

class Cache {
 public:
  static Cache* GetInstance();

  bool Delete(network::disk_cache::ResourceType resource_type, uint32_t key);
  void Delete(network::disk_cache::ResourceType resource_type);
  void DeleteAll();
  std::vector<uint32_t> KeysWithMetadata(
      network::disk_cache::ResourceType resource_type);
  base::Optional<base::Value> Metadata(
      network::disk_cache::ResourceType resource_type, uint32_t key);
  std::unique_ptr<std::vector<uint8_t>> Retrieve(
      network::disk_cache::ResourceType resource_type, uint32_t key,
      std::function<std::pair<std::unique_ptr<std::vector<uint8_t>>,
                              base::Optional<base::Value>>()>
          generate);
  std::unique_ptr<std::vector<uint8_t>> Retrieve(
      network::disk_cache::ResourceType resource_type, uint32_t key);
  void Resize(network::disk_cache::ResourceType resource_type, uint32_t bytes);
  void Store(network::disk_cache::ResourceType resource_type, uint32_t key,
             const std::vector<uint8_t>& data,
             const base::Optional<base::Value>& metadata);
  base::Optional<uint32_t> GetMaxCacheStorageInBytes(
      network::disk_cache::ResourceType resource_type);

  void set_enabled(bool enabled);

  void set_persistent_settings(
      persistent_storage::PersistentSettings* persistent_settings);

 private:
  friend struct base::DefaultSingletonTraits<Cache>;
  Cache() {}

  MemoryCappedDirectory* GetMemoryCappedDirectory(
      network::disk_cache::ResourceType resource_type);
  base::WaitableEvent* GetWaitableEvent(
      network::disk_cache::ResourceType resource_type, uint32_t key);
  void Notify(network::disk_cache::ResourceType resource_type, uint32_t key);
  bool CanCache(network::disk_cache::ResourceType resource_type,
                uint32_t data_size);

  mutable base::Lock lock_;
  // The following map is only used when the JavaScript cache extension is
  // neither present nor valid.
  std::map<network::disk_cache::ResourceType,
           std::unique_ptr<MemoryCappedDirectory>>
      memory_capped_directories_;
  std::map<network::disk_cache::ResourceType,
           std::map<uint32_t, std::vector<base::WaitableEvent*>>>
      pending_;
  bool enabled_ = true;

  persistent_storage::PersistentSettings* persistent_settings_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Cache);
};  // class Cache

}  // namespace cache
}  // namespace cobalt

#endif  // COBALT_CACHE_CACHE_H_
