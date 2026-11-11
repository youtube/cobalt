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

#ifndef COBALT_SHELL_BROWSER_SPLASH_SPLASH_CACHE_READER_H_
#define COBALT_SHELL_BROWSER_SPLASH_SPLASH_CACHE_READER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "url/gurl.h"

namespace mojo {
class DataPipeDrainer;
}

namespace content {

class BrowserContext;

// Reads entries that the web app put into the splash Cache Storage, e.g. with
// `caches.open('default').then(c => c.put('splash.webm', response))`, from
// the browser process, without a renderer.
//
// This is the same cache that H5vccSchemeURLLoaderFactory serves splash
// videos from. Lives on the UI thread. One read at a time.
class SplashCacheReader {
 public:
  enum class Error {
    kNotFound,
    kEmpty,
    kTooLarge,
    kReadFailed,
  };
  using ReadCallback =
      base::OnceCallback<void(base::expected<std::vector<uint8_t>, Error>)>;

  // |splash_origin| is the origin of the web app that owns the cache.
  SplashCacheReader(BrowserContext* browser_context,
                    const GURL& splash_origin,
                    std::u16string cache_name);
  SplashCacheReader(const SplashCacheReader&) = delete;
  SplashCacheReader& operator=(const SplashCacheReader&) = delete;
  ~SplashCacheReader();

  // Reads the entry for |key|, resolved against the splash origin, if it is
  // at most |max_size| bytes. Runs |callback| asynchronously, unless this is
  // destroyed first.
  void Read(const std::string& key, uint64_t max_size, ReadCallback callback);

  static const char* ErrorToString(Error error);

 private:
  class Drainer;

  void OnMatched(uint64_t max_size, blink::mojom::MatchResultPtr result);
  void OnDrained(uint64_t expected_size, std::vector<uint8_t> content);
  void Finish(base::expected<std::vector<uint8_t>, Error> result);

  const raw_ptr<BrowserContext> browser_context_;
  const GURL splash_origin_;
  const std::u16string cache_name_;
  mojo::Remote<blink::mojom::CacheStorage> cache_storage_;
  mojo::Remote<blink::mojom::Blob> blob_;
  std::unique_ptr<Drainer> drainer_;
  ReadCallback callback_;
  base::WeakPtrFactory<SplashCacheReader> weak_factory_{this};
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_SPLASH_CACHE_READER_H_
