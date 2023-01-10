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

#ifndef COBALT_CACHE_MEMORY_CAPPED_DIRECTORY_H_
#define COBALT_CACHE_MEMORY_CAPPED_DIRECTORY_H_

#include <map>
#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/values.h"

namespace cobalt {
namespace cache {

class MemoryCappedDirectory {
  class FileInfo {
   private:
    friend class MemoryCappedDirectory;
    FileInfo(base::FilePath root_path,
             base::FileEnumerator::FileInfo file_info);
    FileInfo(base::FilePath file_path, base::Time last_modified_time,
             uint32_t size);

    struct OldestFirst {
      bool operator()(const FileInfo& left, const FileInfo& right) const;
    };

    base::FilePath file_path_;
    base::Time last_modified_time_;
    uint32_t size_;
  };  // class FileInfo

 public:
  static std::unique_ptr<MemoryCappedDirectory> Create(
      const base::FilePath& directory_path, uint32_t max_size);
  bool Delete(uint32_t key);
  void DeleteAll();
  std::vector<uint32_t> KeysWithMetadata();
  base::Optional<base::Value> Metadata(uint32_t key);
  std::unique_ptr<std::vector<uint8_t>> Retrieve(uint32_t key);
  void Store(uint32_t key, const std::vector<uint8_t>& data,
             const base::Optional<base::Value>& metadata);
  void Resize(uint32_t size);

 private:
  MemoryCappedDirectory(const base::FilePath& directory_path,
                        uint32_t max_size);

  base::FilePath GetFilePath(uint32_t key) const;
  bool EnsureEnoughSpace(uint32_t additional_size_required);

  mutable base::Lock lock_;
  base::FilePath directory_path_;
  // Min-heap determined by last modified time.
  std::vector<FileInfo> file_info_heap_;
  std::map<base::FilePath, uint32_t> file_sizes_;
  std::map<base::FilePath, uint32_t> file_keys_with_metadata_;
  uint32_t size_;
  uint32_t max_size_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCappedDirectory);
};  // MemoryCappedDirectory

}  // namespace cache
}  // namespace cobalt

#endif  // COBALT_CACHE_MEMORY_CAPPED_DIRECTORY_H_
