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

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "net/disk_cache/cobalt/resource_type.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace cobalt {
namespace cache {

class Cache {
 public:
  class FileInfo {
   public:
    FileInfo(base::FilePath root_path,
             base::FileEnumerator::FileInfo file_info);
    FileInfo(base::FilePath file_path, base::Time last_modified_time,
             uint32_t size);

    base::Time GetLastModifiedTime() const;

   private:
    friend class Cache;

    base::FilePath file_path_;
    base::Time last_modified_time_;
    uint32_t size_;
  };  // class FileInfo

  static Cache* GetInstance();
  base::Optional<std::vector<uint8_t>> Retrieve(
      uint32_t key, disk_cache::ResourceType resource_type);
  bool Store(uint32_t key, disk_cache::ResourceType resource_type,
             const std::vector<uint8_t>& data);

 private:
  friend struct base::DefaultSingletonTraits<Cache>;
  Cache() = default;
  base::Optional<std::vector<FileInfo>> GetStored(
      disk_cache::ResourceType resource_type);

  mutable base::Lock lock_;
  // The following maps are only used when the JavaScript cache extension is
  // neither present nor valid.
  std::map<disk_cache::ResourceType, uint32_t> stored_size_by_resource_type_;
  std::map<disk_cache::ResourceType, std::vector<FileInfo>>
      stored_by_resource_type_;

  DISALLOW_COPY_AND_ASSIGN(Cache);
};  // class Cache

}  // namespace cache
}  // namespace cobalt

#endif  // COBALT_CACHE_CACHE_H_
