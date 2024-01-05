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

#ifndef COBALT_NETWORK_DISK_CACHE_COBALT_BACKEND_FACTORY_H_
#define COBALT_NETWORK_DISK_CACHE_COBALT_BACKEND_FACTORY_H_

#include <memory>

#include "base/files/file_path.h"
#include "cobalt/network/url_request_context.h"
#include "net/base/cache_type.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"

namespace cobalt {
namespace network {
namespace disk_cache {

class CobaltBackendFactory : public net::HttpCache::BackendFactory {
 public:
  // |path| is the destination for any files used by the backend. If
  // |max_bytes| is  zero, a default value will be calculated automatically.
  CobaltBackendFactory(const base::FilePath& path, int max_bytes,
                       cobalt::network::URLRequestContext* url_request_context);
  ~CobaltBackendFactory() override;

  // BackendFactory implementation.
  int CreateBackend(net::NetLog* net_log,
                    std::unique_ptr<::disk_cache::Backend>* backend,
                    net::CompletionOnceCallback callback) override;

 private:
  const base::FilePath path_;
  int max_bytes_;
  cobalt::network::URLRequestContext* url_request_context_;
};

}  // namespace disk_cache
}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_DISK_CACHE_COBALT_BACKEND_FACTORY_H_
