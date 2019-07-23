// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_SPLASH_SCREEN_CACHE_H_
#define COBALT_BROWSER_SPLASH_SCREEN_CACHE_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "url/gurl.h"

namespace cobalt {
namespace browser {

// SplashScreenCache saves a splash screen document loaded by the
// HTMLLinkElement into the system cache, if any, so that it's
// available to BrowserModule to show on subsequent runs before
// loading the initial URL. The key for the cached document is based
// on the passed in initial URL. See cobalt/doc/splash_screen.md for
// more information.
class SplashScreenCache {
 public:
  SplashScreenCache();

  // Cache the splash screen.
  bool CacheSplashScreen(const std::string& key,
                         const std::string& content) const;

  // Read the cached the splash screen.
  int ReadCachedSplashScreen(const std::string& key,
                             std::unique_ptr<char[]>* result) const;

  // Determine if a splash screen is cached corresponding to the key.
  bool IsSplashScreenCached(const std::string& key) const;

  // Get the key that corresponds to a starting URL. Optionally create
  // subdirectories along the path.
  static base::Optional<std::string> GetKeyForStartUrl(const GURL& url);

 private:
  // Lock to protect access to the cache file.
  mutable base::Lock lock_;
  // Hash of the last read page contents.
  mutable uint32_t last_page_hash_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_SPLASH_SCREEN_CACHE_H_
