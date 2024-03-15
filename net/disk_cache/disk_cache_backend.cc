// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"

// Completing partial implementation of disk_cache.h, in place of
// disk_cache.cc.
namespace disk_cache {

disk_cache::BackendResult CreateCacheBackend(net::CacheType type,
                              net::BackendType backend_type,
                              scoped_refptr<BackendFileOperationsFactory> file_operations,
                              const base::FilePath& path,
                              int64_t max_bytes,
                              ResetHandling reset_handling,
                              net::NetLog* net_log,
                              BackendResultCallback callback) {
  return CreateCacheBackend(type, backend_type, std::move(file_operations), path, max_bytes, reset_handling, net_log, base::OnceClosure(), std::move(callback));
}

#if defined(OS_ANDROID)
NET_EXPORT net::Error CreateCacheBackend(
    net::CacheType type,
    net::BackendType backend_type,
    const base::FilePath& path,
    int64_t max_bytes,
    bool force,
    net::NetLog* net_log,
    std::unique_ptr<Backend>* backend,
    net::CompletionOnceCallback callback,
    base::android::ApplicationStatusListener* app_status_listener) {
  return net::ERR_FAILED;
}
#endif

net::Error CreateCacheBackend(net::CacheType type,
                              net::BackendType backend_type,
                              scoped_refptr<BackendFileOperationsFactory> file_operations,
                              const base::FilePath& path,
                              int64_t max_bytes,
                              ResetHandling reset_handling,
                              net::NetLog* net_log,
                              base::OnceClosure post_cleanup_callback,
                              net::CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());

  if (type == net::MEMORY_CACHE) {
    std::unique_ptr<MemBackendImpl> mem_backend_impl =
        disk_cache::MemBackendImpl::CreateBackend(max_bytes, net_log);
    if (mem_backend_impl) {
      mem_backend_impl->SetPostCleanupCallback(
          std::move(post_cleanup_callback));
      return net::OK;
    } else {
      if (!post_cleanup_callback.is_null())
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(post_cleanup_callback));
      return net::ERR_FAILED;
    }
  }
  return net::ERR_FAILED;
}

void FlushCacheThreadForTesting() {
  // For simple backend.
#ifndef USE_HACKY_COBALT_CHANGES
  SimpleBackendImpl::FlushWorkerPoolForTesting();
  base::ThreadPoolInstance::GetInstance()->FlushForTesting();
#endif

  // Block backend.
  // BackendImpl::FlushForTesting();
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
