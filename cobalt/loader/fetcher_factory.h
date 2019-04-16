// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LOADER_FETCHER_FACTORY_H_
#define COBALT_LOADER_FETCHER_FACTORY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/threading/thread.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/loader/blob_fetcher.h"
#include "cobalt/loader/fetcher.h"
#include "url/gurl.h"

namespace cobalt {
namespace network {
class NetworkModule;
}

namespace loader {

class FetcherFactory {
 public:
  explicit FetcherFactory(network::NetworkModule* network_module);
  FetcherFactory(network::NetworkModule* network_module,
                 const base::FilePath& extra_search_dir);
  FetcherFactory(
      network::NetworkModule* network_module,
      const base::FilePath& extra_search_dir,
      const BlobFetcher::ResolverCallback& blob_resolver,
      const base::Callback<int(const std::string&,
                               std::unique_ptr<char[]>*)>& read_cache_callback =
          base::Callback<int(const std::string&, std::unique_ptr<char[]>*)>());

  // Creates a fetcher. Returns NULL if the creation fails.
  std::unique_ptr<Fetcher> CreateFetcher(const GURL& url,
                                         Fetcher::Handler* handler);

  std::unique_ptr<Fetcher> CreateSecureFetcher(
      const GURL& url, const csp::SecurityCallback& url_security_callback,
      RequestMode request_mode, const Origin& origin,
      Fetcher::Handler* handler);

  network::NetworkModule* network_module() const { return network_module_; }

 private:
  base::Thread file_thread_;
  network::NetworkModule* network_module_;
  base::FilePath extra_search_dir_;
  BlobFetcher::ResolverCallback blob_resolver_;
  base::Callback<int(const std::string&, std::unique_ptr<char[]>*)>
      read_cache_callback_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_FETCHER_FACTORY_H_
