// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SHELL_BROWSER_SPLASH_CACHE_READER_H_
#define COBALT_SHELL_BROWSER_SPLASH_CACHE_READER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace content {

class BlobReader;
class BrowserContext;

// An enum for UMA histogram to indicate the state of retrieving splash screen.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SplashScreenFetchedState {
  kOkBuiltIn = 0,
  kOkCache = 1,
  kErrorOnCacheEmptyContent = 2,
  kErrorOnCacheFileOversize = 3,
  kErrorOnReadCache = 4,
  kErrorOnResourceNotFound = 5,
  // The built-in resource, without reading the cache, as
  // kEnableCachedSplashFile is disabled. Unlike kOkBuiltIn, this isn't a
  // cache miss.
  kSkippedCachedAssets = 6,
  kMaxValue = kSkippedCachedAssets,
};

// Records `state` in Cobalt.SplashScreen.FetchedFromCache.
void RecordSplashScreenFetchedState(SplashScreenFetchedState state);

// Reads a splash screen resource, e.g., "splash.webm", that the WebApp cached
// with the Cache API of the splash domain: for splash.html, through
// H5vccSchemeURLLoaderFactory, and for the video played natively. Destroying it
// cancels the read.
class SplashCacheReader {
 public:
  // `content` is empty unless `state` is SplashScreenFetchedState::kOkCache,
  // e.g., if cached assets are disabled, or the resource isn't cached, can't be
  // read, or is larger than the limit, in which case the built-in resource is
  // the fallback.
  using Callback = base::OnceCallback<void(SplashScreenFetchedState state,
                                           std::string content)>;

  // Reads from the caches of `splash_domain`, e.g., "https://www.youtube.com/",
  // resources of at most `max_content_size` bytes.
  SplashCacheReader(BrowserContext* browser_context,
                    const std::string& splash_domain,
                    uint64_t max_content_size);

  SplashCacheReader(const SplashCacheReader&) = delete;
  SplashCacheReader& operator=(const SplashCacheReader&) = delete;

  ~SplashCacheReader();

  // Reads `name` from the cache `cache_name`, and runs `callback` once, from a
  // task of its own, unless `this` is destroyed first. Without
  // kEnableCachedSplashFile, runs `callback` with
  // SplashScreenFetchedState::kSkippedCachedAssets instead of reading
  // the cache.
  void Read(const std::string& name,
            const std::u16string& cache_name,
            Callback callback);

 private:
  void OnCacheMatched(const std::string& cache_name,
                      blink::mojom::MatchResultPtr result);
  void OnBlobRead(uint64_t expected_size, std::vector<uint8_t> content);
  void Fail(const std::string& message, SplashScreenFetchedState state);
  void Finish(SplashScreenFetchedState state, std::string content);
  void RunCallback(SplashScreenFetchedState state, std::string content);

  BrowserContext* const browser_context_;
  const std::string splash_domain_;
  const uint64_t max_content_size_;
  Callback callback_;
  mojo::Remote<blink::mojom::CacheStorage> cache_storage_remote_;
  std::unique_ptr<BlobReader> blob_reader_;
  base::WeakPtrFactory<SplashCacheReader> weak_factory_{this};
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_CACHE_READER_H_
