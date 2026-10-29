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

namespace content {

class BrowserContext;

// Lists the splash screen assets that splash.html's WebAssembly policy chooses
// from, with cobalt::features::kSplashWasmPolicy: the built-in images and
// videos, and those that the WebApp cached where splash.html reads them, i.e.,
// splash.webm and splash.png in the cache of the splash screen's topic, in the
// splash domain's Cache API. The cache is only read with
// kEnableCachedSplashFile, as splash.html only reads it then, and a cached
// asset is only listed if the loader would serve it, i.e., if it's neither
// empty nor larger than the loader's limit. The list is JSON, an array with
// one object per asset:
//   {"source": "builtin" or "cache", "cache": <the cache's name, or "">,
//    "name": <e.g., "splash.webm">, "kind": "video" or "image",
//    "topic": <"default", "music", or the cache's name>,
//    "width": <in px, 0 if unknown>, "height": <in px, 0 if unknown>,
//    "bytes": <the size>}
//
// Shell starts listing for its topic when the splash screen starts, so that the
// list is usually ready when splash.html asks for it, through
// h5vcc-embedded://splash_assets.json?cache=<topic>. The list is a snapshot
// from then: an asset that the WebApp caches later isn't shown, as the policy's
// choice of a built-in asset skips the cache (source=builtin). Lives on the UI
// thread, as user data of its BrowserContext.
class SplashAssetList : public base::SupportsUserData::Data {
 public:
  // Runs with the list, or with std::nullopt if it's unavailable: if the
  // cached assets couldn't be listed, e.g., within kTimeout, or the list is
  // for another cache. splash.html then reads the cache itself, rather than
  // miss an asset that the WebApp cached.
  using Callback = base::OnceCallback<void(std::optional<std::string> json)>;
  // Binds `receiver` to the Cache API of `splash_domain`.
  using CacheStorageBinder = base::RepeatingCallback<void(
      const std::string& splash_domain,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver)>;

  // How long listing may take before the list is unavailable.
  static constexpr base::TimeDelta kTimeout = base::Seconds(1);

  // Returns the list of `browser_context`, which must not be null, creating
  // it if needed.
  static SplashAssetList* GetOrCreateForBrowserContext(
      BrowserContext* browser_context);

  // Lists the cached assets through `bind_cache_storage`. Public for testing:
  // see GetOrCreateForBrowserContext().
  explicit SplashAssetList(CacheStorageBinder bind_cache_storage);

  SplashAssetList(const SplashAssetList&) = delete;
  SplashAssetList& operator=(const SplashAssetList&) = delete;

  ~SplashAssetList() override;

  // Starts listing the assets of the cache `cache_name`, e.g., "default",
  // unless it already is listing. A list that's ready is listed again, so that
  // each splash screen gets the assets of its launch.
  void Start(const std::string& cache_name);

  // Runs `callback` with the list of the cache `cache_name` once it's ready,
  // right away if it is, or with std::nullopt right away if the list is for
  // another cache. Starts listing if it hasn't started.
  void Get(const std::string& cache_name, Callback callback);

 private:
  enum class State { kIdle, kListing, kReady };

  void OnCacheMatched(size_t index, blink::mojom::MatchResultPtr result);
  void OnTimeout();
  void OnDisconnect();
  void Finish(bool complete);
  std::string ToJson() const;

  SEQUENCE_CHECKER(sequence_checker_);

  const CacheStorageBinder bind_cache_storage_;

  State state_ = State::kIdle;
  // The cache whose assets are, or are being, listed.
  std::string cache_name_;
  // Whether the listing reads the cache, with kEnableCachedSplashFile.
  bool reads_cache_ = false;
  base::TimeTicks start_time_;
  mojo::Remote<blink::mojom::CacheStorage> cache_storage_;
  base::OneShotTimer timeout_;
  // The size of each asset that splash.html reads from the cache, or
  // std::nullopt if the loader wouldn't serve it from there.
  std::vector<std::optional<uint64_t>> cached_sizes_;
  size_t pending_lookups_ = 0;
  // Once the state is kReady.
  std::optional<std::string> json_;
  std::vector<Callback> callbacks_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_ASSET_LIST_H_
