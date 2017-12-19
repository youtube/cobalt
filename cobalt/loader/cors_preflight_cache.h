/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_CORS_PREFLIGHT_CACHE_H_
#define COBALT_LOADER_CORS_PREFLIGHT_CACHE_H_

#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/hash_tables.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_fetcher.h"
#include "starboard/string.h"

namespace cobalt {
namespace loader {

// https://fetch.spec.whatwg.org/#concept-cache
class CORSPreflightCache : public base::RefCounted<CORSPreflightCache> {
 public:
  // An entry is appended at the end of every preflight to avoid same
  // preflight request in the near future.
  void AppendEntry(const std::string& url_str, const std::string& origin,
                   int max_age, bool has_credentials,
                   const std::vector<std::string>& methods_vec,
                   const std::vector<std::string>& headernames_vec);
  // Check if there is a preflight cache match. This method does not include
  // checking for CORS-safelisted method and request-header.
  bool HaveEntry(const std::string& url_str, const std::string& origin,
                 bool credentials_mode_is_include,
                 const net::URLFetcher::RequestType& new_request_method,
                 const std::vector<std::string>& unsafe_headernames);

 private:
  // Case-insensitive comparator.
  struct CaseInsensitiveCompare {
    bool operator()(const std::string& lhs, const std::string& rhs) const {
      return SbStringCompareNoCase(lhs.c_str(), rhs.c_str()) < 0;
    }
  };

  // The spec wants a cache entry for every method and for every header which is
  // a little unnesessarily expensive. We create an entry for each request and
  // If there is an old entry in the new entry's pleace we simply push the old
  // one out which potentially increases cache misses slightly but reduces
  // memory cost. Chromium also takes this approach.
  // The map's first key is entry's request url and second is entry's origin.
  struct CORSPreflightCacheEntry
      : public base::RefCounted<CORSPreflightCacheEntry> {
    bool credentials;
    // True if response has "Access-Control-Allow-Methods: *".
    bool allow_all_methods;
    // True if response has "Access-Control-Allow-Headers: *".
    // Non-wildcard request-header name is "Authentication".
    bool allow_all_headers_except_non_wildcard;
    base::Time expiration_time;
    std::set<net::URLFetcher::RequestType> methods;
    std::set<std::string, CaseInsensitiveCompare> headernames;

    CORSPreflightCacheEntry()
        : credentials(false),
          allow_all_methods(false),
          allow_all_headers_except_non_wildcard(false) {}
  };

  struct ExpirationHeapEntry {
    base::Time expiration_time;
    std::string url_str;
    std::string origin;
    bool operator>(const ExpirationHeapEntry& rhs) const {
      return expiration_time > rhs.expiration_time;
    }
    bool operator<(const ExpirationHeapEntry& rhs) const {
      return expiration_time < rhs.expiration_time;
    }
  };

  // This operator constructs a min-heap.
  class ExpirationMinHeapComparator {
   public:
    bool operator()(const ExpirationHeapEntry& lhs,
                    const ExpirationHeapEntry& rhs) {
      return lhs > rhs;
    }
  };

  void ClearObsoleteEntries();

  // TODO: Replace scoped_refptr with scoped_ptr when possible or replace the
  // map as a 'scoped_map'.
  base::hash_map<
      std::string,
      base::hash_map<std::string, scoped_refptr<CORSPreflightCacheEntry> > >
      content_;

  std::priority_queue<ExpirationHeapEntry, std::vector<ExpirationHeapEntry>,
                      ExpirationMinHeapComparator>
      expiration_time_heap_;
  base::ThreadChecker thread_checker_;
};

}  // namespace loader
}  // namespace cobalt
#endif  // COBALT_LOADER_CORS_PREFLIGHT_CACHE_H_
