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

#ifndef COBALT_NETWORK_DISK_CACHE_COBALT_DISK_CACHE_H_
#define COBALT_NETWORK_DISK_CACHE_COBALT_DISK_CACHE_H_

#include <memory>

#include "cobalt/network/url_request_context.h"
#include "net/base/cache_type.h"
#include "net/disk_cache/disk_cache.h"

namespace cobalt {
namespace network {
namespace disk_cache {

net::Error CreateCobaltCacheBackend(
    const base::FilePath& path, int64_t max_bytes, net::NetLog* net_log,
    std::unique_ptr<::disk_cache::Backend>* backend,
    net::CompletionOnceCallback callback,
    cobalt::network::URLRequestContext* url_request_context);

}  // namespace disk_cache
}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_DISK_CACHE_COBALT_DISK_CACHE_H_
