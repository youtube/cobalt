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

#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

// This class implements the Backend interface. An object of this class handles
// the operations of the cache without writing to disk.
class NET_EXPORT_PRIVATE CobaltBackendImpl final : public Backend {
 public:
  explicit CobaltBackendImpl(net::NetLog* net_log);
  ~CobaltBackendImpl() override;

  static std::unique_ptr<CobaltBackendImpl> CreateBackend(int64_t max_bytes,
                                                          net::NetLog* net_log);

  // Sets a callback to be posted after we are destroyed. Should be called at
  // most once.
  void SetPostCleanupCallback(base::OnceClosure cb);

  // Backend interface.
  net::CacheType GetCacheType() const override;
  int32_t GetEntryCount() const override;
  net::Error OpenEntry(const std::string& key,
                       net::RequestPriority request_priority,
                       Entry** entry,
                       std::string type,
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

 private:
  class CobaltIterator;
  friend class CobaltIterator;

  base::OnceClosure post_cleanup_callback_;

  base::WeakPtrFactory<CobaltBackendImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CobaltBackendImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_COBALT_COBALT_BACKEND_IMPL_H_
