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

#include <algorithm>
#include <string>

#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "cobalt/extension/javascript_cache.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/system.h"

namespace {

base::Optional<base::FilePath> CacheDirectory(
    disk_cache::ResourceType resource_type) {
  std::vector<char> path(kSbFileMaxPath, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       kSbFileMaxPath)) {
    return base::nullopt;
  }
  // TODO: make subdirectory determined by type of cache data.
  std::string cache_subdirectory;
  switch (resource_type) {
    case disk_cache::ResourceType::kCompiledScript:
      cache_subdirectory = "compiled_js";
      break;
    default:
      return base::nullopt;
  }
  auto cache_directory = base::FilePath(path.data()).Append(cache_subdirectory);
  SbDirectoryCreate(cache_directory.value().c_str());
  return cache_directory;
}

base::Optional<base::FilePath> FilePathFromKey(
    uint32_t key, disk_cache::ResourceType resource_type) {
  auto cache_directory = CacheDirectory(resource_type);
  if (!cache_directory) {
    return base::nullopt;
  }
  return cache_directory->Append(base::UintToString(key));
}

base::Optional<uint32_t> GetMaxStorageInBytes(
    disk_cache::ResourceType resource_type) {
  switch (resource_type) {
    case disk_cache::ResourceType::kCompiledScript:
      return 5 << 20;  // 5MiB
    default:
      return base::nullopt;
  }
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

namespace {

struct FileInfoComparer {
  bool operator()(const Cache::FileInfo& left, const Cache::FileInfo& right) {
    return left.GetLastModifiedTime() > right.GetLastModifiedTime();
  }
};

}  // namespace

Cache::FileInfo::FileInfo(base::FilePath root_path,
                          base::FileEnumerator::FileInfo file_info)
    : file_path_(root_path.Append(file_info.GetName())),
      last_modified_time_(file_info.GetLastModifiedTime()),
      size_(static_cast<uint32_t>(file_info.GetSize())) {}

Cache::FileInfo::FileInfo(base::FilePath file_path,
                          base::Time last_modified_time, uint32_t size)
    : file_path_(file_path),
      last_modified_time_(last_modified_time),
      size_(size) {}

base::Time Cache::FileInfo::GetLastModifiedTime() const {
  return last_modified_time_;
}

Cache* Cache::GetInstance() {
  return base::Singleton<Cache, base::LeakySingletonTraits<Cache>>::get();
}

base::Optional<std::vector<uint8_t>> Cache::Retrieve(
    uint32_t key, disk_cache::ResourceType resource_type) {
  const CobaltExtensionJavaScriptCacheApi* javascript_cache_extension =
      GetJavaScriptCacheExtension();
  if (javascript_cache_extension) {
    const uint8_t* cache_data_buf = nullptr;
    int cache_data_size = -1;
    if (javascript_cache_extension->GetCachedScript(key, 0, &cache_data_buf,
                                                    &cache_data_size)) {
      auto data = std::vector<uint8_t>(cache_data_buf,
                                       cache_data_buf + cache_data_size);
      javascript_cache_extension->ReleaseCachedScriptData(cache_data_buf);
      return data;
    }
  }
  // TODO: check if caching should be disabled.
  auto file_path = FilePathFromKey(key, resource_type);
  if (!file_path || !base::PathExists(file_path.value())) {
    return base::nullopt;
  }
  int64 size;
  base::GetFileSize(file_path.value(), &size);
  std::vector<uint8_t> data(static_cast<size_t>(size));
  int bytes_read =
      base::ReadFile(file_path.value(), reinterpret_cast<char*>(data.data()),
                     static_cast<int>(size));
  if (bytes_read != size) {
    return base::nullopt;
  }
  return data;
}

bool Cache::Store(uint32_t key, disk_cache::ResourceType resource_type,
                  const std::vector<uint8_t>& data) {
  const CobaltExtensionJavaScriptCacheApi* javascript_cache_extension =
      GetJavaScriptCacheExtension();
  if (javascript_cache_extension &&
      javascript_cache_extension->StoreCachedScript(key, 0, data.data(),
                                                    data.size())) {
    return true;
  }
  // TODO: check if caching should be disabled.
  base::AutoLock auto_lock(lock_);
  auto stored = GetStored(resource_type);
  if (!stored) {
    return false;
  }
  auto file_path = FilePathFromKey(key, resource_type);
  if (!file_path) {
    return false;
  }
  auto max_storage_in_bytes = GetMaxStorageInBytes(resource_type);
  if (!max_storage_in_bytes) {
    return false;
  }
  uint32_t new_entry_size = static_cast<uint32_t>(data.size());
  while (stored_size_by_resource_type_[resource_type] + new_entry_size >
         max_storage_in_bytes.value()) {
    std::pop_heap(stored->begin(), stored->end(), FileInfoComparer());
    auto removed = stored->back();
    stored_size_by_resource_type_[resource_type] -= removed.size_;
    base::DeleteFile(removed.file_path_, false);
    stored->pop_back();
  }
  int bytes_written =
      base::WriteFile(file_path.value(),
                      reinterpret_cast<const char*>(data.data()), data.size());
  if (bytes_written != data.size()) {
    base::DeleteFile(file_path.value(), false);
    return false;
  }
  stored_size_by_resource_type_[resource_type] += new_entry_size;
  stored->push_back(
      FileInfo(file_path.value(), base::Time::Now(), new_entry_size));
  stored_by_resource_type_[resource_type] = stored.value();
  return true;
}

base::Optional<std::vector<Cache::FileInfo>> Cache::GetStored(
    disk_cache::ResourceType resource_type) {
  auto it = stored_by_resource_type_.find(resource_type);
  if (it != stored_by_resource_type_.end()) {
    return it->second;
  }
  auto cache_directory = CacheDirectory(resource_type);
  if (!cache_directory) {
    return base::nullopt;
  }
  base::FileEnumerator file_enumerator(cache_directory.value(), false,
                                       base::FileEnumerator::FILES);
  std::vector<FileInfo> stored;
  while (!file_enumerator.Next().empty()) {
    auto file_info =
        Cache::FileInfo(cache_directory.value(), file_enumerator.GetInfo());
    stored.push_back(file_info);
    stored_size_by_resource_type_[resource_type] += file_info.size_;
  }
  std::make_heap(stored.begin(), stored.end(), FileInfoComparer());
  stored_by_resource_type_[resource_type] = stored;
  return stored;
}

}  // namespace cache
}  // namespace cobalt
