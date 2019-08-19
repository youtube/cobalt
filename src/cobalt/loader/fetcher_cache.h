// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LOADER_FETCHER_CACHE_H_
#define COBALT_LOADER_FETCHER_CACHE_H_

#include <memory>
#include <string>

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/c_val.h"
#include "cobalt/loader/loader.h"
#include "net/base/linked_hash_map.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace loader {

// Manages a cache for data fetched by Fetchers.
class FetcherCache {
 public:
  FetcherCache(const char* name, size_t capacity);
  ~FetcherCache();

  Loader::FetcherCreator GetFetcherCreator(
      const GURL& url, const Loader::FetcherCreator& real_fetcher_creator);
  void NotifyResourceRequested(const std::string& url);

 private:
  class CacheEntry;

  std::unique_ptr<Fetcher> CreateCachedFetcher(
      const GURL& url, const Loader::FetcherCreator& real_fetcher_creator,
      Fetcher::Handler* handler);
  void OnFetchSuccess(const std::string& url,
                      const scoped_refptr<net::HttpResponseHeaders>& headers,
                      const Origin& last_url_origin,
                      bool did_fail_from_transient_error, std::string* data);

  THREAD_CHECKER(thread_checker_);

  const size_t capacity_;
  size_t total_size_ = 0;

  net::linked_hash_map<std::string, CacheEntry*> cache_entries_;

  base::CVal<base::cval::SizeInBytes, base::CValPublic> memory_size_in_bytes_;
  base::CVal<int, base::CValPublic> count_resources_cached_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_FETCHER_CACHE_H_
