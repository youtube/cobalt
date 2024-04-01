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

#include "cobalt/network/disk_cache/cobalt_backend_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "net/disk_cache/backend_cleanup_tracker.h"

using base::Time;

namespace cobalt {
namespace network {
namespace disk_cache {

namespace {

void CompletionOnceCallbackHandler(
    scoped_refptr<CobaltBackendImpl::RefCountedRunner> runner, int result) {
  // Save most recent non-success result.
  if (result != net::OK) {
    runner->set_callback_result(result);
  }
}

bool NeedsBackend(ResourceType resource_type) {
  switch (resource_type) {
    case kHTML:
    case kCSS:
    case kImage:
    case kFont:
    case kUncompiledScript:
    case kOther:
    case kSplashScreen:
      return true;
    case kCompiledScript:
    case kCacheApi:
    case kServiceWorkerScript:
    default:
      return false;
  }
}

}  // namespace

CobaltBackendImpl::CobaltBackendImpl(
    const base::FilePath& path,
    scoped_refptr<::disk_cache::BackendCleanupTracker> cleanup_tracker,
    int64_t max_bytes, net::NetLog* net_log,
    cobalt::network::URLRequestContext* url_request_context)
    : weak_factory_(this),
      url_request_context_(url_request_context),
      Backend(net::CacheType::DISK_CACHE) {
  int64_t total_size = 0;
  for (int i = 0; i < kTypeCount; i++) {
    ResourceType resource_type = (ResourceType)i;
    if (!NeedsBackend(resource_type)) {
      continue;
    }
    std::string sub_directory = defaults::GetSubdirectory(resource_type);
    base::FilePath dir = path.Append(FILE_PATH_LITERAL(sub_directory));
    uint32_t bucket_size = disk_cache::settings::GetQuota(resource_type);
    total_size += bucket_size;
    ::disk_cache::SimpleBackendImpl* simple_backend =
        new ::disk_cache::SimpleBackendImpl(
            /*file_operations_factory=*/nullptr, dir, cleanup_tracker,
            /*file_tracker=*/nullptr, bucket_size,
            /*cache_type=*/net::DISK_CACHE, net_log);
    simple_backend_map_[resource_type] = simple_backend;
  }
  // Must be at least enough space for each backend.
  DCHECK(total_size <= max_bytes);
}

CobaltBackendImpl::~CobaltBackendImpl() {
  for (int i = 0; i < kTypeCount; i++) {
    ResourceType resource_type = (ResourceType)i;
    if (simple_backend_map_.count(resource_type) == 1) {
      delete simple_backend_map_[resource_type];
    }
  }
  simple_backend_map_.clear();
}

void CobaltBackendImpl::UpdateSizes(ResourceType type, uint32_t bytes) {
  if (simple_backend_map_.count(type) == 0) {
    return;
  }
  ::disk_cache::SimpleBackendImpl* simple_backend = simple_backend_map_[type];
  simple_backend->SetMaxSize(bytes);
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

int32_t CobaltBackendImpl::GetEntryCount() const {
  // Return total number of entries both on disk and in memory.
  int32_t count = 0;
  for (auto const& backend : simple_backend_map_) {
    count += backend.second->GetEntryCount();
  }
  return count;
}

CobaltBackendImpl::EntryResult CobaltBackendImpl::OpenOrCreateEntry(
    const std::string& key, net::RequestPriority request_priority,
    EntryResultCallback callback) {
  ResourceType type = GetType(key);
  auto quota = disk_cache::settings::GetQuota(type);
  if (quota == 0 || simple_backend_map_.count(type) == 0) {
    return EntryResult::MakeError(net::Error::ERR_BLOCKED_BY_CLIENT);
  }
  ::disk_cache::SimpleBackendImpl* simple_backend = simple_backend_map_[type];
  return simple_backend->OpenOrCreateEntry(key, request_priority,
                                           std::move(callback));
}

CobaltBackendImpl::EntryResult CobaltBackendImpl::OpenEntry(
    const std::string& key, net::RequestPriority request_priority,
    EntryResultCallback callback) {
  ResourceType type = GetType(key);
  auto quota = disk_cache::settings::GetQuota(type);
  if (quota == 0 || simple_backend_map_.count(type) == 0) {
    return EntryResult::MakeError(net::Error::ERR_BLOCKED_BY_CLIENT);
  }
  ::disk_cache::SimpleBackendImpl* simple_backend = simple_backend_map_[type];
  return simple_backend->OpenEntry(key, request_priority, std::move(callback));
}

CobaltBackendImpl::EntryResult CobaltBackendImpl::CreateEntry(
    const std::string& key, net::RequestPriority request_priority,
    EntryResultCallback callback) {
  ResourceType type = GetType(key);
  auto quota = disk_cache::settings::GetQuota(type);
  if (quota == 0 || simple_backend_map_.count(type) == 0) {
    return EntryResult::MakeError(net::Error::ERR_BLOCKED_BY_CLIENT);
  }
  ::disk_cache::SimpleBackendImpl* simple_backend = simple_backend_map_[type];
  return simple_backend->CreateEntry(key, request_priority,
                                     std::move(callback));
}

net::Error CobaltBackendImpl::DoomEntry(const std::string& key,
                                        net::RequestPriority priority,
                                        CompletionOnceCallback callback) {
  if (simple_backend_map_.count(GetType(key)) == 0) {
    return net::Error::ERR_BLOCKED_BY_CLIENT;
  }
  ::disk_cache::SimpleBackendImpl* simple_backend =
      simple_backend_map_[GetType(key)];
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
    Time initial_time, Time end_time, CompletionOnceCallback callback) {
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
    Time initial_time, CompletionOnceCallback callback) {
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
    base::Time initial_time, base::Time end_time,
    Int64CompletionOnceCallback callback) {
  // TODO: Implement
  return 0;
}

class CobaltBackendImpl::CobaltIterator final : public Backend::Iterator {
 public:
  explicit CobaltIterator(base::WeakPtr<CobaltBackendImpl> backend)
      : backend_(backend) {}

  EntryResult OpenNextEntry(EntryResultCallback callback) override {
    // TODO: Implement
    return EntryResult::MakeError(net::ERR_FAILED);
  }

 private:
  base::WeakPtr<CobaltBackendImpl> backend_ = nullptr;
};

std::unique_ptr<::disk_cache::Backend::Iterator>
CobaltBackendImpl::CreateIterator() {
  return std::unique_ptr<::disk_cache::Backend::Iterator>(
      new CobaltIterator(weak_factory_.GetWeakPtr()));
}

void CobaltBackendImpl::OnExternalCacheHit(const std::string& key) {
  if (simple_backend_map_.count(GetType(key)) == 0) {
    return;
  }
  ::disk_cache::SimpleBackendImpl* simple_backend =
      simple_backend_map_[GetType(key)];
  simple_backend->OnExternalCacheHit(key);
}

net::Error CobaltBackendImpl::DoomAllEntriesOfType(
    disk_cache::ResourceType type, CompletionOnceCallback callback) {
  if (simple_backend_map_.count(type) == 0) {
    std::move(callback).Run(net::OK);
    return net::OK;
  }
  ::disk_cache::SimpleBackendImpl* simple_backend = simple_backend_map_[type];
  return simple_backend->DoomAllEntries(std::move(callback));
}

ResourceType CobaltBackendImpl::GetType(const std::string& key) {
  return url_request_context_->GetType(key);
}

}  // namespace disk_cache
}  // namespace network
}  // namespace cobalt
