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

#ifndef COBALT_NETWORK_DISK_CACHE_COBALT_BACKEND_IMPL_H_
#define COBALT_NETWORK_DISK_CACHE_COBALT_BACKEND_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "cobalt/network/disk_cache/resource_type.h"
#include "cobalt/network/url_request_context.h"
#include "net/base/completion_once_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_backend_impl.h"

namespace cobalt {
namespace network {
namespace disk_cache {

const char kCacheEnabledPersistentSettingsKey[] = "cacheEnabled";

// This class implements the Backend interface. An object of this class handles
// the operations of the cache without writing to disk.
class NET_EXPORT_PRIVATE CobaltBackendImpl final
    : public ::disk_cache::Backend {
 public:
  explicit CobaltBackendImpl(
      const base::FilePath& path,
      scoped_refptr<::disk_cache::BackendCleanupTracker> cleanup_tracker,
      int64_t max_bytes, net::NetLog* net_log,
      cobalt::network::URLRequestContext* url_request_context);
  CobaltBackendImpl(const CobaltBackendImpl&) = delete;
  CobaltBackendImpl& operator=(const CobaltBackendImpl&) = delete;
  ~CobaltBackendImpl() override;

  net::Error Init(CompletionOnceCallback completion_callback) {
    return net::OK;
  }
  void UpdateSizes(ResourceType type, uint32_t bytes) {}

  // Backend interface.
  net::CacheType GetCacheType() const;
  int32_t GetEntryCount() const override;
  EntryResult OpenEntry(const std::string& key,
                        net::RequestPriority request_priority,
                        EntryResultCallback callback) override;
  EntryResult CreateEntry(const std::string& key,
                          net::RequestPriority request_priority,
                          EntryResultCallback callback) override;
  net::Error DoomEntry(const std::string& key, net::RequestPriority priority,
                       CompletionOnceCallback callback) override {
    return net::OK;
  }
  net::Error DoomAllEntries(CompletionOnceCallback callback) override {
    return net::OK;
  }
  net::Error DoomEntriesBetween(base::Time initial_time, base::Time end_time,
                                CompletionOnceCallback callback) override {
    return net::OK;
  }
  net::Error DoomEntriesSince(base::Time initial_time,
                              CompletionOnceCallback callback) override {
    return net::OK;
  }
  int64_t CalculateSizeOfAllEntries(
      Int64CompletionOnceCallback callback) override {
    return 0;
  }
  int64_t CalculateSizeOfEntriesBetween(
      base::Time initial_time, base::Time end_time,
      Int64CompletionOnceCallback callback) override {
    return 0;
  }
  std::unique_ptr<Iterator> CreateIterator() override;
  void GetStats(base::StringPairs* stats) override {}
  void OnExternalCacheHit(const std::string& key) override {}
  //   size_t DumpMemoryStats(
  //       base::trace_event::ProcessMemoryDump* pmd,
  //       const std::string& parent_absolute_name) const override;
  net::Error DoomAllEntriesOfType(disk_cache::ResourceType type,
                                  CompletionOnceCallback callback) {
    return net::OK;
  }
  uint8_t GetEntryInMemoryData(const std::string& key) override { return 0; }
  void SetEntryInMemoryData(const std::string& key, uint8_t data) override {}

  EntryResult OpenOrCreateEntry(const std::string& key,
                                net::RequestPriority priority,
                                EntryResultCallback callback) override;
  int64_t MaxFileSize() const override;

  // A refcounted class that runs a CompletionOnceCallback once it's destroyed.
  class RefCountedRunner : public base::RefCounted<RefCountedRunner> {
   public:
    explicit RefCountedRunner(CompletionOnceCallback completion_callback)
        : destruction_callback_(base::BindOnce(
              &RefCountedRunner::CompletionCallback, base::Unretained(this),
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

  void AssociateKeyWithResourceType(const std::string& key,
                                    ResourceType resource_type);

 private:
  class CobaltIterator;
  friend class CobaltIterator;

  ResourceType GetType(const std::string& key);

  base::WeakPtrFactory<CobaltBackendImpl> weak_factory_;

  std::map<ResourceType, ::disk_cache::SimpleBackendImpl*> simple_backend_map_;
  cobalt::network::URLRequestContext* url_request_context_;
};

}  // namespace disk_cache
}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_DISK_CACHE_COBALT_BACKEND_IMPL_H_
