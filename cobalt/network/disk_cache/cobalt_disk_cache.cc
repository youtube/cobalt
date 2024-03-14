// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/network/disk_cache/cobalt_disk_cache.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "cobalt/network/disk_cache/cobalt_backend_impl.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/memory/mem_backend_impl.h"

namespace {

// Builds an instance of the backend depending on platform, type, experiments
// etc. Takes care of the retry state. This object will self-destroy when
// finished.
class CacheCreator {
 public:
  CacheCreator(const base::FilePath& path, int64_t max_bytes,
               net::NetLog* net_log,
               std::unique_ptr<::disk_cache::Backend>* backend,
               net::CompletionOnceCallback callback,
               cobalt::network::URLRequestContext* url_request_context);

  net::Error TryCreateCleanupTrackerAndRun();

  // Creates the backend, the cleanup context for it having been already
  // established... or purposefully left as null.
  net::Error Run();

 private:
  ~CacheCreator();

  void DoCallback(int result);

  void OnIOComplete(int result);

  const base::FilePath path_;
  bool retry_;
  int64_t max_bytes_;
  std::unique_ptr<::disk_cache::Backend>* backend_;
  net::CompletionOnceCallback callback_;
  std::unique_ptr<::disk_cache::Backend> created_cache_;
  net::NetLog* net_log_;
  scoped_refptr<::disk_cache::BackendCleanupTracker> cleanup_tracker_;
  cobalt::network::URLRequestContext* url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(CacheCreator);
};

CacheCreator::CacheCreator(
    const base::FilePath& path, int64_t max_bytes, net::NetLog* net_log,
    std::unique_ptr<::disk_cache::Backend>* backend,
    net::CompletionOnceCallback callback,
    cobalt::network::URLRequestContext* url_request_context)
    : path_(path),
      max_bytes_(max_bytes),
      backend_(backend),
      callback_(std::move(callback)),
      net_log_(net_log),
      url_request_context_(url_request_context) {}

CacheCreator::~CacheCreator() = default;

net::Error CacheCreator::Run() {
  auto* cobalt_cache = new cobalt::network::disk_cache::CobaltBackendImpl(
      path_, cleanup_tracker_.get(), max_bytes_, net_log_,
      url_request_context_);
  created_cache_.reset(cobalt_cache);
  return cobalt_cache->Init(
      base::Bind(&CacheCreator::OnIOComplete, base::Unretained(this)));
}

void CacheCreator::DoCallback(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result == net::OK) {
    *backend_ = std::move(created_cache_);
  } else {
    LOG(ERROR) << "Unable to create cache";
    created_cache_.reset();
  }
  std::move(callback_).Run(result);
  delete this;
}

void CacheCreator::OnIOComplete(int result) {
  if (result == net::OK || retry_) return DoCallback(result);

  // This is a failure and we are supposed to try again, so delete the object,
  // delete all the files, and try again.
  retry_ = true;
  created_cache_.reset();
  return DoCallback(result);
}

}  // namespace

namespace cobalt {
namespace network {
namespace disk_cache {

net::Error CreateCobaltCacheBackend(
    const base::FilePath& path, int64_t max_bytes, net::NetLog* net_log,
    std::unique_ptr<::disk_cache::Backend>* backend,
    net::CompletionOnceCallback callback,
    cobalt::network::URLRequestContext* url_request_context) {
  DCHECK(!callback.is_null());
  CacheCreator* creator =
      new CacheCreator(path, max_bytes, net_log, backend, std::move(callback),
                       url_request_context);
  return creator->Run();
}

}  // namespace disk_cache
}  // namespace network
}  // namespace cobalt
