// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_CACHE_FETCHER_H_
#define COBALT_LOADER_CACHE_FETCHER_H_

#include <limits>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/loader/fetcher.h"

namespace cobalt {
namespace loader {

extern const char kCacheScheme[];

// CacheFetcher is for fetching data in the system cache,
// kSbSystemPathCacheDirectory. Cached splash screen HTML documents
// are stored there, for instance. Their subpaths are based on the
// initial URL they correspond to and must be fully specified in the
// URL passed into CacheFetcher. See SplashScreenCache for an example usage.
class CacheFetcher : public Fetcher {
 public:
  CacheFetcher(
      const GURL& url, const csp::SecurityCallback& security_callback,
      Handler* handler,
      const base::Callback<int(const std::string&, scoped_array<char>*)>&
          read_cache_callback =
              base::Callback<int(const std::string&, scoped_array<char>*)>());

  ~CacheFetcher() override;

 private:
  void Fetch();
  void GetCacheData(const std::string& key);
  bool IsAllowedByCsp();

  GURL url_;
  csp::SecurityCallback security_callback_;
  base::WeakPtrFactory<CacheFetcher> weak_ptr_factory_;
  base::Callback<int(const std::string&, scoped_array<char>*)>
      read_cache_callback_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_CACHE_FETCHER_H_
