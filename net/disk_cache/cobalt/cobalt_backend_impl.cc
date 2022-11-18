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

#include "net/disk_cache/cobalt/cobalt_backend_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "net/disk_cache/backend_cleanup_tracker.h"

using base::Time;

namespace disk_cache {

namespace {

const char kPersistentSettingsJson[] = "cache_settings.json";

void CompletionOnceCallbackHandler(
    scoped_refptr<CobaltBackendImpl::RefCountedRunner> runner,
    int result) {
  // Save most recent non-success result.
  if (result != net::OK) {
    runner->set_callback_result(result);
  }
}

ResourceType GetType(const std::string& key) {
  // The type is stored at the front of the key, as an int that maps to the
  // ResourceType enum. This is done in HttpCache::GenerateCacheKey.
  DCHECK(!key.empty());
  char type_hint = key[0];
  if (isdigit(type_hint)) {
    // ASCII '0' = 48
    return (disk_cache::ResourceType)(static_cast<int>(type_hint) - 48);
  }
  return kOther;
}

void ReadDiskCacheSize(
    cobalt::persistent_storage::PersistentSettings* settings,
    int64_t max_bytes) {
  auto total_size = 0;
  disk_cache::ResourceTypeMetadata kTypeMetadataNew[disk_cache::kTypeCount];

  for (int i = 0; i < disk_cache::kTypeCount; i++) {
    auto metadata = disk_cache::kTypeMetadata[i];
    uint32_t bucket_size =
        static_cast<uint32_t>(settings->GetPersistentSettingAsDouble(
            metadata.directory, metadata.max_size_bytes));
    kTypeMetadataNew[i] = {metadata.directory, bucket_size};

    total_size += bucket_size;
  }

  // Check if PersistentSettings values are valid and can replace the disk_cache::kTypeMetadata.
  if (total_size <= max_bytes) {
    std::copy(std::begin(kTypeMetadataNew), std::end(kTypeMetadataNew), std::begin(disk_cache::kTypeMetadata));
    return;
  }

  // PersistentSettings values are invalid and will be replaced by the default values in
  // disk_cache::kTypeMetadata.
  for (int i = 0; i < disk_cache::kTypeCount; i++) {
    auto metadata = disk_cache::kTypeMetadata[i];
    settings->SetPersistentSetting(
            metadata.directory,
            std::make_unique<base::Value>(static_cast<double>(metadata.max_size_bytes)));
  }
}

}  // namespace

CobaltBackendImpl::CobaltBackendImpl(
    const base::FilePath& path,
    scoped_refptr<BackendCleanupTracker> cleanup_tracker,
    int64_t max_bytes,
    net::CacheType cache_type,
    net::NetLog* net_log)
    : weak_factory_(this) {
  persistent_settings_ =
      std::make_unique<cobalt::persistent_storage::PersistentSettings>(
          kPersistentSettingsJson);
  ReadDiskCacheSize(persistent_settings_.get(), max_bytes);

  // Initialize disk backend for each resource type.
  int64_t total_size = 0;
  for (int i = 0; i < kTypeCount; i++) {
    auto metadata = kTypeMetadata[i];
    base::FilePath dir = path.Append(FILE_PATH_LITERAL(metadata.directory));
    int64_t bucket_size = metadata.max_size_bytes;
    total_size += bucket_size;
    SimpleBackendImpl* simple_backend = new SimpleBackendImpl(
        dir, cleanup_tracker, /* file_tracker = */ nullptr, bucket_size,
        cache_type, net_log);
    simple_backend_map_[(ResourceType)i] = simple_backend;
  }

  // Must be at least enough space for each backend.
  DCHECK(total_size <= max_bytes);
}

CobaltBackendImpl::~CobaltBackendImpl() {
  for (int i = 0; i < kTypeCount; i++) {
    delete simple_backend_map_[(ResourceType)i];
  }
  simple_backend_map_.clear();
}

void CobaltBackendImpl::UpdateSizes(ResourceType type, uint32_t bytes) {
  if (bytes == disk_cache::kTypeMetadata[type].max_size_bytes)
    return;

  // Static cast value to double since base::Value cannot be a long.
  persistent_settings_->SetPersistentSetting(
      disk_cache::kTypeMetadata[type].directory,
      std::make_unique<base::Value>(static_cast<double>(bytes)));

  disk_cache::kTypeMetadata[type].max_size_bytes = bytes;
  SimpleBackendImpl* simple_backend = simple_backend_map_[type];
  simple_backend->SetMaxSize(bytes);
}

uint32_t CobaltBackendImpl::GetQuota(ResourceType type) {
  return disk_cache::kTypeMetadata[type].max_size_bytes;
}

void CobaltBackendImpl::ValidatePersistentSettings() {
  persistent_settings_->ValidatePersistentSettings();
}

net::Error CobaltBackendImpl::Init(CompletionOnceCallback completion_callback) {
  auto closure_runner =
      base::MakeRefCounted<RefCountedRunner>(std::move(completion_callback));
  for (auto const& backend : simple_backend_map_) {
    CompletionOnceCallback delegate =
        base::BindOnce(&CompletionOnceCallbackHandler, closure_runner);
    backend.second->Init(base::BindOnce(std::move(delegate)));
  }
  return net::ERR_IO_PENDING;
}

net::CacheType CobaltBackendImpl::GetCacheType() const {
  return net::DISK_CACHE;
}

int32_t CobaltBackendImpl::GetEntryCount() const {
  // Return total number of entries both on disk and in memory.
  int32_t count = 0;
  for (auto const& backend : simple_backend_map_) {
    count += backend.second->GetEntryCount();
  }
  return count;
}

net::Error CobaltBackendImpl::OpenEntry(const std::string& key,
                                        net::RequestPriority request_priority,
                                        Entry** entry,
                                        CompletionOnceCallback callback) {
  SimpleBackendImpl* simple_backend = simple_backend_map_[GetType(key)];
  return simple_backend->OpenEntry(key, request_priority, entry,
                                   std::move(callback));
}

net::Error CobaltBackendImpl::CreateEntry(const std::string& key,
                                          net::RequestPriority request_priority,
                                          Entry** entry,
                                          CompletionOnceCallback callback) {
  ResourceType type = GetType(key);
  auto quota = disk_cache::kTypeMetadata[type].max_size_bytes;
  if (quota == 0) {
    return net::Error::ERR_BLOCKED_BY_CLIENT;
  }
  SimpleBackendImpl* simple_backend = simple_backend_map_[GetType(key)];
  return simple_backend->CreateEntry(key, request_priority, entry,
                                     std::move(callback));
}

net::Error CobaltBackendImpl::DoomEntry(const std::string& key,
                                        net::RequestPriority priority,
                                        CompletionOnceCallback callback) {
  SimpleBackendImpl* simple_backend = simple_backend_map_[GetType(key)];
  return simple_backend->DoomEntry(key, priority, std::move(callback));
}

net::Error CobaltBackendImpl::DoomAllEntries(CompletionOnceCallback callback) {
  auto closure_runner =
      base::MakeRefCounted<RefCountedRunner>(std::move(callback));
  for (auto const& backend : simple_backend_map_) {
    CompletionOnceCallback delegate =
        base::BindOnce(&CompletionOnceCallbackHandler, closure_runner);
    backend.second->DoomAllEntries(std::move(delegate));
  }
  return net::ERR_IO_PENDING;
}

net::Error CobaltBackendImpl::DoomEntriesBetween(
    Time initial_time,
    Time end_time,
    CompletionOnceCallback callback) {
  auto closure_runner =
      base::MakeRefCounted<RefCountedRunner>(std::move(callback));
  for (auto const& backend : simple_backend_map_) {
    CompletionOnceCallback delegate =
        base::BindOnce(&CompletionOnceCallbackHandler, closure_runner);
    backend.second->DoomEntriesBetween(initial_time, end_time,
                                       std::move(delegate));
  }
  return net::ERR_IO_PENDING;
}

net::Error CobaltBackendImpl::DoomEntriesSince(
    Time initial_time,
    CompletionOnceCallback callback) {
  auto closure_runner =
      base::MakeRefCounted<RefCountedRunner>(std::move(callback));
  for (auto const& backend : simple_backend_map_) {
    CompletionOnceCallback delegate =
        base::BindOnce(&CompletionOnceCallbackHandler, closure_runner);
    backend.second->DoomEntriesSince(initial_time, std::move(delegate));
  }
  return net::ERR_IO_PENDING;
}

int64_t CobaltBackendImpl::CalculateSizeOfAllEntries(
    Int64CompletionOnceCallback callback) {
  // TODO: Implement
  return 0;
}

int64_t CobaltBackendImpl::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    Int64CompletionOnceCallback callback) {
  // TODO: Implement
  return 0;
}

class CobaltBackendImpl::CobaltIterator final : public Backend::Iterator {
 public:
  explicit CobaltIterator(base::WeakPtr<CobaltBackendImpl> backend)
      : backend_(backend) {}

  net::Error OpenNextEntry(Entry** next_entry,
                           CompletionOnceCallback callback) override {
    // TODO: Implement
    return net::ERR_FAILED;
  }

 private:
  base::WeakPtr<CobaltBackendImpl> backend_ = nullptr;
};

std::unique_ptr<Backend::Iterator> CobaltBackendImpl::CreateIterator() {
  return std::unique_ptr<Backend::Iterator>(
      new CobaltIterator(weak_factory_.GetWeakPtr()));
}

void CobaltBackendImpl::OnExternalCacheHit(const std::string& key) {
  SimpleBackendImpl* simple_backend = simple_backend_map_[GetType(key)];
  simple_backend->OnExternalCacheHit(key);
}

size_t CobaltBackendImpl::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_absolute_name) const {
  // Dump memory stats for all backends.
  std::string name = parent_absolute_name + "/cobalt_backend";
  size_t size = 0;
  for (auto const& backend : simple_backend_map_) {
    size += backend.second->DumpMemoryStats(pmd, name);
  }
  return size;
}

net::Error CobaltBackendImpl::DoomAllEntriesOfType(disk_cache::ResourceType type,
                        CompletionOnceCallback callback) {
  SimpleBackendImpl* simple_backend = simple_backend_map_[type];
  return simple_backend->DoomAllEntries(std::move(callback));
}

}  // namespace disk_cache
