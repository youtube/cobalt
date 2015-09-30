/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LOADER_FETCHER_FACTORY_H_
#define LOADER_FETCHER_FACTORY_H_

#include "base/threading/thread.h"
#include "cobalt/loader/fetcher.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace network {
class NetworkModule;
}

namespace loader {

class FetcherFactory {
 public:
  explicit FetcherFactory(network::NetworkModule* network_module);
  scoped_ptr<Fetcher> CreateFetcher(
      const GURL& url, Fetcher::Handler* handler);
  scoped_ptr<Fetcher> CreateFetcherWithRange(const GURL& url,
                                             int64 start_offset,
                                             int64 bytes_to_read,
                                             Fetcher::Handler* handler);

  network::NetworkModule* network_module() const { return network_module_; }

 private:
  base::Thread file_thread_;
  network::NetworkModule* network_module_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // LOADER_FETCHER_FACTORY_H_
