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
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "cobalt/cache/memory_capped_directory.h"
#include "net/disk_cache/cobalt/resource_type.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace cobalt {
namespace cache {

class Cache {
 public:
  static Cache* GetInstance();

  bool Delete(disk_cache::ResourceType resource_type, uint32_t key);
  void Delete(disk_cache::ResourceType resource_type);
  void DeleteAll();
  std::vector<uint32_t> KeysWithMetadata(
      disk_cache::ResourceType resource_type);
  base::Optional<base::Value> Metadata(disk_cache::ResourceType resource_type,
                                       uint32_t key);
  std::unique_ptr<std::vector<uint8_t>> Retrieve(
      disk_cache::ResourceType resource_type, uint32_t key,
      std::function<std::pair<std::unique_ptr<std::vector<uint8_t>>,
                              base::Optional<base::Value>>()>
          generate);
  std::unique_ptr<std::vector<uint8_t>> Retrieve(
      disk_cache::ResourceType resource_type, uint32_t key);
  void Resize(disk_cache::ResourceType resource_type, uint32_t bytes);
  void Store(disk_cache::ResourceType resource_type, uint32_t key,
             const std::vector<uint8_t>& data,
             const base::Optional<base::Value>& metadata);
  base::Optional<uint32_t> GetMaxCacheStorageInBytes(
      disk_cache::ResourceType resource_type);

 private:
  friend struct base::DefaultSingletonTraits<Cache>;
  Cache() = default;

  MemoryCappedDirectory* GetMemoryCappedDirectory(
      disk_cache::ResourceType resource_type);
  base::WaitableEvent* GetWaitableEvent(disk_cache::ResourceType resource_type,
                                        uint32_t key);
  void Notify(disk_cache::ResourceType resource_type, uint32_t key);
  bool CanCache(disk_cache::ResourceType resource_type, uint32_t data_size);

  mutable base::Lock lock_;
  // The following map is only used when the JavaScript cache extension is
  // neither present nor valid.
  std::map<disk_cache::ResourceType, std::unique_ptr<MemoryCappedDirectory>>
      memory_capped_directories_;
  std::map<disk_cache::ResourceType,
           std::map<uint32_t, std::vector<base::WaitableEvent*>>>
      pending_;

  DISALLOW_COPY_AND_ASSIGN(Cache);
};  // class Cache

}  // namespace cache
}  // namespace cobalt

#endif  // COBALT_CACHE_CACHE_H_
