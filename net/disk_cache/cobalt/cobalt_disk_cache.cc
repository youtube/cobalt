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

#include "base/metrics/field_trial.h"
#include "net/disk_cache/cobalt/cobalt_backend_impl.h"

namespace disk_cache {

net::Error CreateCacheBackendImpl(net::CacheType type,
                                  net::BackendType backend_type,
                                  const base::FilePath& path,
                                  int64_t max_bytes,
                                  bool force,
                                  net::NetLog* net_log,
                                  std::unique_ptr<Backend>* backend,
                                  base::OnceClosure post_cleanup_callback,
                                  net::CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());

  std::unique_ptr<CobaltBackendImpl> cobalt_backend_impl =
      disk_cache::CobaltBackendImpl::CreateBackend(max_bytes, net_log);
  if (cobalt_backend_impl) {
    cobalt_backend_impl->SetPostCleanupCallback(
        std::move(post_cleanup_callback));
    *backend = std::move(cobalt_backend_impl);
    return net::OK;
  }

  if (!post_cleanup_callback.is_null())
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(post_cleanup_callback));
  return net::ERR_FAILED;
}

net::Error CreateCacheBackend(net::CacheType type,
                              net::BackendType backend_type,
                              const base::FilePath& path,
                              int64_t max_bytes,
                              bool force,
                              net::NetLog* net_log,
                              std::unique_ptr<Backend>* backend,
                              net::CompletionOnceCallback callback) {
  return CreateCacheBackendImpl(type, backend_type, path, max_bytes, force,
                                net_log, backend, base::OnceClosure(),
                                std::move(callback));
}

net::Error CreateCacheBackend(net::CacheType type,
                              net::BackendType backend_type,
                              const base::FilePath& path,
                              int64_t max_bytes,
                              bool force,
                              net::NetLog* net_log,
                              std::unique_ptr<Backend>* backend,
                              base::OnceClosure post_cleanup_callback,
                              net::CompletionOnceCallback callback) {
  return CreateCacheBackendImpl(
      type, backend_type, path, max_bytes, force, net_log, backend,
      std::move(post_cleanup_callback), std::move(callback));
}

int64_t Backend::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    Int64CompletionOnceCallback callback) {
  return net::ERR_NOT_IMPLEMENTED;
}

uint8_t Backend::GetEntryInMemoryData(const std::string& key) {
  return 0;
}

void Backend::SetEntryInMemoryData(const std::string& key, uint8_t data) {}

}  // namespace disk_cache