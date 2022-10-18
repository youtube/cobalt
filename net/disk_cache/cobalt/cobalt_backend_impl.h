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

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_COBALT_COBALT_BACKEND_IMPL_H_
#define NET_DISK_CACHE_COBALT_COBALT_BACKEND_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/callback_helpers.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "net/base/completion_once_callback.h"
#include "net/disk_cache/cobalt/resource_type.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"

namespace disk_cache {

const char kCacheEnabledPersistentSettingsKey[] = "cacheEnabled";

// This class implements the Backend interface. An object of this class handles
// the operations of the cache without writing to disk.
class NET_EXPORT_PRIVATE CobaltBackendImpl final : public Backend {
 public:
  explicit CobaltBackendImpl(
      const base::FilePath& path,
      scoped_refptr<BackendCleanupTracker> cleanup_tracker,
      int64_t max_bytes,
      net::CacheType cache_type,
      net::NetLog* net_log);
  CobaltBackendImpl(const CobaltBackendImpl&) = delete;
  CobaltBackendImpl& operator=(const CobaltBackendImpl&) = delete;
  ~CobaltBackendImpl() override;

  net::Error Init(CompletionOnceCallback completion_callback);
  void UpdateSizes(ResourceType type, uint32_t bytes);
  uint32_t GetQuota(ResourceType type);
  void ValidatePersistentSettings();

  // Backend interface.
  net::CacheType GetCacheType() const override;
  int32_t GetEntryCount() const override;
  net::Error OpenEntry(const std::string& key,
                       net::RequestPriority request_priority,
                       Entry** entry,
                       CompletionOnceCallback callback) override;
  net::Error CreateEntry(const std::string& key,
                         net::RequestPriority request_priority,
                         Entry** entry,
                         CompletionOnceCallback callback) override;
  net::Error DoomEntry(const std::string& key,
                       net::RequestPriority priority,
                       CompletionOnceCallback callback) override;
  net::Error DoomAllEntries(CompletionOnceCallback callback) override;
  net::Error DoomEntriesBetween(base::Time initial_time,
                                base::Time end_time,
                                CompletionOnceCallback callback) override;
  net::Error DoomEntriesSince(base::Time initial_time,
                              CompletionOnceCallback callback) override;
  int64_t CalculateSizeOfAllEntries(
      Int64CompletionOnceCallback callback) override;
  int64_t CalculateSizeOfEntriesBetween(
      base::Time initial_time,
      base::Time end_time,
      Int64CompletionOnceCallback callback) override;
  std::unique_ptr<Iterator> CreateIterator() override;
  void GetStats(base::StringPairs* stats) override {}
  void OnExternalCacheHit(const std::string& key) override;
  size_t DumpMemoryStats(
      base::trace_event::ProcessMemoryDump* pmd,
      const std::string& parent_absolute_name) const override;
  net::Error DoomAllEntriesOfType(disk_cache::ResourceType type,
                          CompletionOnceCallback callback);

  // A refcounted class that runs a CompletionOnceCallback once it's destroyed.
  class RefCountedRunner : public base::RefCounted<RefCountedRunner> {
   public:
    explicit RefCountedRunner(CompletionOnceCallback completion_callback)
        : destruction_callback_(
              base::BindOnce(&RefCountedRunner::CompletionCallback,
                             base::Unretained(this),
                             std::move(completion_callback))) {}
    void set_callback_result(int result) { callback_result_ = result; }

   private:
    friend class base::RefCounted<RefCountedRunner>;
    ~RefCountedRunner() = default;

    void CompletionCallback(CompletionOnceCallback callback) {
      std::move(callback).Run(callback_result_);
    }

    base::ScopedClosureRunner destruction_callback_;
    int callback_result_ = net::OK;
  };

 private:
  class CobaltIterator;
  friend class CobaltIterator;

  base::WeakPtrFactory<CobaltBackendImpl> weak_factory_;

  std::map<ResourceType, SimpleBackendImpl*> simple_backend_map_;

  // Json PrefStore used for persistent settings.
  std::unique_ptr<cobalt::persistent_storage::PersistentSettings>
      persistent_settings_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_COBALT_COBALT_BACKEND_IMPL_H_
