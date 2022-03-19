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

#include "base/threading/sequenced_task_runner_handle.h"

using base::Time;

namespace disk_cache {

CobaltBackendImpl::CobaltBackendImpl(net::NetLog* net_log)
    : weak_factory_(this) {}

CobaltBackendImpl::~CobaltBackendImpl() {
  if (!post_cleanup_callback_.is_null())
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(post_cleanup_callback_));
}

// static
std::unique_ptr<CobaltBackendImpl> CobaltBackendImpl::CreateBackend(
    int64_t max_bytes,
    net::NetLog* net_log) {
  std::unique_ptr<CobaltBackendImpl> cache(
      std::make_unique<CobaltBackendImpl>(net_log));
  return cache;
}

void CobaltBackendImpl::SetPostCleanupCallback(base::OnceClosure cb) {
  DCHECK(post_cleanup_callback_.is_null());
  post_cleanup_callback_ = std::move(cb);
}

net::CacheType CobaltBackendImpl::GetCacheType() const {
  return net::DISK_CACHE;
}

int32_t CobaltBackendImpl::GetEntryCount() const {
  // TODO: Implement
  return 0;
}

net::Error CobaltBackendImpl::OpenEntry(const std::string& key,
                                        net::RequestPriority request_priority,
                                        Entry** entry,
                                        std::string type,
                                        CompletionOnceCallback callback) {
  // TODO: Implement
  return net::ERR_FAILED;
}

net::Error CobaltBackendImpl::CreateEntry(const std::string& key,
                                          net::RequestPriority request_priority,
                                          Entry** entry,
                                          CompletionOnceCallback callback) {
  // TODO: Implement
  return net::ERR_FAILED;
}

net::Error CobaltBackendImpl::DoomEntry(const std::string& key,
                                        net::RequestPriority priority,
                                        CompletionOnceCallback callback) {
  // TODO: Implement
  return net::OK;
}

net::Error CobaltBackendImpl::DoomAllEntries(CompletionOnceCallback callback) {
  // TODO: Implement
  return net::OK;
}

net::Error CobaltBackendImpl::DoomEntriesBetween(
    Time initial_time,
    Time end_time,
    CompletionOnceCallback callback) {
  // TODO: Implement
  return net::OK;
}

net::Error CobaltBackendImpl::DoomEntriesSince(
    Time initial_time,
    CompletionOnceCallback callback) {
  // TODO: Implement
  return net::OK;
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
  // TODO: Implement
}

size_t CobaltBackendImpl::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_absolute_name) const {
  // TODO: Implement
  return 0;
}

}  // namespace disk_cache
