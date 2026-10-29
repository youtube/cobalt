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

#ifndef COBALT_SHELL_BROWSER_SPLASH_ASSET_LIST_H_
#define COBALT_SHELL_BROWSER_SPLASH_ASSET_LIST_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;

// Lists the splash screen assets that splash.html's WebAssembly policy chooses
// from, with cobalt::features::kSplashWasmPolicy: the built-in images and
// videos, and those that the WebApp cached where splash.html reads them, i.e.,
// splash.webm and splash.png in each cache of the splash domain's Cache API.
// The cache is only listed with kEnableCachedSplashFile, as splash.html only
// reads it then. The list is JSON, an array with one object per asset:
//   {"source": "builtin" or "cache", "cache": <the cache's name, or "">,
//    "name": <e.g., "splash.webm">, "kind": "video" or "image",
//    "topic": <"default", "music", or the cache's name>,
//    "width": <in px, 0 if unknown>, "height": <in px, 0 if unknown>,
//    "bytes": <the size>}
//
// Shell starts listing when the splash screen starts, so that the list is
// usually ready when splash.html asks for it, through
// h5vcc-embedded://splash_assets.json. Lives on the UI thread, as user data of
// its BrowserContext.
class SplashAssetList : public base::SupportsUserData::Data {
 public:
  // Runs with the list, or with std::nullopt if the cached assets couldn't
  // all be listed, e.g., within kTimeout. splash.html then reads the cache
  // itself, rather than miss an asset that the WebApp cached.
  using Callback = base::OnceCallback<void(std::optional<std::string> json)>;
  // Binds `receiver` to the splash domain's Cache API.
  using CacheStorageBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver)>;

  // How long listing may take before the list is unavailable.
  static constexpr base::TimeDelta kTimeout = base::Seconds(2);

  // Returns the list of `browser_context`, which must not be null, creating
  // it if needed.
  static SplashAssetList* GetOrCreateForBrowserContext(
      BrowserContext* browser_context);

  // Lists the cached assets of `splash_domain`, e.g.,
  // "https://www.youtube.com/", through `bind_cache_storage`. Public for
  // testing: see GetOrCreateForBrowserContext().
  SplashAssetList(const std::string& splash_domain,
                  CacheStorageBinder bind_cache_storage);

  SplashAssetList(const SplashAssetList&) = delete;
  SplashAssetList& operator=(const SplashAssetList&) = delete;

  ~SplashAssetList() override;

  // Starts listing, unless it already is. A list that's ready is listed
  // again, so that each splash screen gets the assets of its launch.
  void Start();

  // Runs `callback` with the list once it's ready, right away if it is.
  // Starts listing if it hasn't started.
  void Get(Callback callback);

 private:
  enum class State { kIdle, kListing, kReady };

  void OnCacheNames(const std::vector<std::u16string>& cache_names);
  void OnCacheMatched(size_t lookup, blink::mojom::MatchResultPtr result);
  void Finish(bool complete);
  std::string ToJson() const;

  SEQUENCE_CHECKER(sequence_checker_);

  const GURL splash_domain_;
  const CacheStorageBinder bind_cache_storage_;

  State state_ = State::kIdle;
  base::TimeTicks start_time_;
  mojo::Remote<blink::mojom::CacheStorage> cache_storage_;
  base::OneShotTimer timeout_;
  std::vector<std::u16string> cache_names_;
  // The size of each asset that splash.html reads from each cache, or
  // std::nullopt if it isn't cached: one lookup per cache and asset.
  std::vector<std::optional<uint64_t>> cached_sizes_;
  size_t pending_lookups_ = 0;
  // Once the state is kReady.
  std::optional<std::string> json_;
  std::vector<Callback> callbacks_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_ASSET_LIST_H_
