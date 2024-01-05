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

#include "cobalt/network/disk_cache/cobalt_backend_factory.h"

#include <utility>

#include "cobalt/network/disk_cache/cobalt_disk_cache.h"

namespace cobalt {
namespace network {
namespace disk_cache {

CobaltBackendFactory::CobaltBackendFactory(
    const base::FilePath& path, int max_bytes,
    cobalt::network::URLRequestContext* url_request_context)
    : path_(path),
      max_bytes_(max_bytes),
      url_request_context_(url_request_context) {}

CobaltBackendFactory::~CobaltBackendFactory() = default;

int CobaltBackendFactory::CreateBackend(
    net::NetLog* net_log, std::unique_ptr<::disk_cache::Backend>* backend,
    net::CompletionOnceCallback callback) {
  DCHECK_GE(max_bytes_, 0);
  return CreateCobaltCacheBackend(path_, max_bytes_, net_log, backend,
                                  std::move(callback), url_request_context_);
}

}  // namespace disk_cache
}  // namespace network
}  // namespace cobalt
