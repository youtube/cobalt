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

#include "cobalt/shell/browser/splash_asset_list.h"

#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kUserDataKey[] = "SplashAssetList";

// The built-in splash screen assets, embedded with splash.html.
struct BuiltInAsset {
  const char* name;
  bool is_video;
  const char* topic;
  // In pixels, or 0 if unknown.
  int width;
  int height;
};
constexpr BuiltInAsset kBuiltInAssets[] = {
    {"splash.webm", true, "default", 1920, 1080},
    {"splash_480.webm", true, "default", 853, 480},
    {"music_splash.webm", true, "music", 1920, 1080},
    {"music_splash_480.webm", true, "music", 853, 480},
    {"splash.png", false, "default", 0, 0},
    {"music_splash.png", false, "music", 0, 0},
};

// The assets that splash.html reads from the cache of the splash screen's
// topic, resolved against the splash domain as the loader does.
struct CachedAsset {
  const char* name;
  bool is_video;
};
constexpr CachedAsset kCachedAssets[] = {
    {"splash.webm", true},
    {"splash.png", false},
};
constexpr size_t kCachedAssetCount = std::size(kCachedAssets);

base::Value::Dict MakeAsset(std::string_view source,
                            std::string_view cache,
                            std::string_view name,
                            bool is_video,
                            std::string_view topic,
                            int width,
                            int height,
                            uint64_t bytes) {
  base::Value::Dict asset;
  asset.Set("source", source);
  asset.Set("cache", cache);
  asset.Set("name", name);
  asset.Set("kind", is_video ? "video" : "image");
  asset.Set("topic", topic);
  asset.Set("width", width);
  asset.Set("height", height);
  asset.Set("bytes", base::saturated_cast<int>(bytes));
  return asset;
}

}  // namespace

// static
SplashAssetList* SplashAssetList::GetOrCreateForBrowserContext(
    BrowserContext* browser_context) {
  CHECK(browser_context);
  auto* list =
      static_cast<SplashAssetList*>(browser_context->GetUserData(kUserDataKey));
  if (!list) {
    // Unretained: the list is the BrowserContext's, so it doesn't outlive it.
    auto owned_list = std::make_unique<SplashAssetList>(base::BindRepeating(
        &H5vccSchemeURLLoaderFactory::BindSplashCacheStorage,
        base::Unretained(browser_context)));
    list = owned_list.get();
    browser_context->SetUserData(kUserDataKey, std::move(owned_list));
  }
  return list;
}

SplashAssetList::SplashAssetList(CacheStorageBinder bind_cache_storage)
    : bind_cache_storage_(std::move(bind_cache_storage)) {}

SplashAssetList::~SplashAssetList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SplashAssetList::Start(const std::string& cache_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kListing) {
    return;
  }
  state_ = State::kListing;
  cache_name_ = cache_name;
  start_time_ = base::TimeTicks::Now();
  cached_sizes_.assign(kCachedAssetCount, std::nullopt);
  pending_lookups_ = 0;
  json_.reset();

  // splash.html only reads the cache with kEnableCachedSplashFile.
  reads_cache_ = H5vccSchemeURLLoaderFactory::IsCachedSplashFileEnabled();
  if (!reads_cache_) {
    Finish(/*complete=*/true);
    return;
  }

  const std::string splash_domain =
      H5vccSchemeURLLoaderFactory::GetSplashDomain();
  // Unretained: `this` owns `cache_storage_` and `timeout_`, whose callbacks
  // are dropped when Finish() resets them.
  bind_cache_storage_.Run(splash_domain,
                          cache_storage_.BindNewPipeAndPassReceiver());
  cache_storage_.set_disconnect_handler(
      base::BindOnce(&SplashAssetList::OnDisconnect, base::Unretained(this)));
  timeout_.Start(
      FROM_HERE, kTimeout,
      base::BindOnce(&SplashAssetList::OnTimeout, base::Unretained(this)));
  // Looks each asset up as the loader reads it, in the cache `cache_name`
  // only, which doesn't create the cache, as opening it would.
  pending_lookups_ = kCachedAssetCount;
  for (size_t index = 0; index < kCachedAssetCount; ++index) {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = GURL(splash_domain).Resolve(kCachedAssets[index].name);
    request->method = "GET";
    auto match_options = blink::mojom::MultiCacheQueryOptions::New();
    match_options->query_options = blink::mojom::CacheQueryOptions::New();
    match_options->cache_name = base::UTF8ToUTF16(cache_name_);
    cache_storage_->Match(std::move(request), std::move(match_options),
                          /*in_related_fetch_event=*/false,
                          /*in_range_fetch_event=*/false,
                          /*trace_id=*/0,
                          base::BindOnce(&SplashAssetList::OnCacheMatched,
                                         base::Unretained(this), index));
  }
}

void SplashAssetList::Get(const std::string& cache_name, Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kIdle) {
    callbacks_.push_back(std::move(callback));
    Start(cache_name);
    return;
  }
  if (cache_name != cache_name_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  if (state_ == State::kReady) {
    std::move(callback).Run(json_);
    return;
  }
  callbacks_.push_back(std::move(callback));
}

void SplashAssetList::OnCacheMatched(size_t index,
                                     blink::mojom::MatchResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result->is_status()) {
    // As for the loader, a missing entry or cache is a cache miss, while
    // other errors leave the list incomplete.
    const blink::mojom::CacheStorageError status = result->get_status();
    if (status != blink::mojom::CacheStorageError::kErrorNotFound &&
        status != blink::mojom::CacheStorageError::kErrorCacheNameNotFound) {
      LOG(WARNING) << "Couldn't list the cached splash screen assets: looking "
                   << kCachedAssets[index].name << " up in cache "
                   << cache_name_ << " failed with " << static_cast<int>(status)
                   << ".";
      Finish(/*complete=*/false);
      return;
    }
  } else {
    const blink::mojom::FetchAPIResponsePtr& response =
        result->is_response() ? result->get_response()
                              : result->get_eager_response()->response;
    // Only an asset that the loader would serve, which serves the built-in
    // asset instead of an empty or oversize one.
    if (response->blob && response->blob->size > 0 &&
        response->blob->size <=
            H5vccSchemeURLLoaderFactory::GetSplashContentSizeLimit()) {
      cached_sizes_[index] = response->blob->size;
    }
  }
  CHECK_GT(pending_lookups_, 0u);
  if (!--pending_lookups_) {
    Finish(/*complete=*/true);
  }
}

void SplashAssetList::OnTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(WARNING) << "Couldn't list the cached splash screen assets within "
               << kTimeout.InMilliseconds() << " ms.";
  Finish(/*complete=*/false);
}

void SplashAssetList::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(WARNING) << "Couldn't list the cached splash screen assets: the Cache "
                  "API disconnected.";
  Finish(/*complete=*/false);
}

void SplashAssetList::Finish(bool complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(state_ == State::kListing);
  timeout_.Stop();
  cache_storage_.reset();
  if (complete) {
    json_ = ToJson();
    if (reads_cache_) {
      // How long the list took to be ready, when reading the cache.
      const base::TimeDelta elapsed = base::TimeTicks::Now() - start_time_;
      base::UmaHistogramTimes("Cobalt.SplashScreen.WasmPolicy.AssetListTime",
                              elapsed);
      VLOG(1) << "Listed the splash screen assets of cache " << cache_name_
              << " in " << elapsed.InMilliseconds() << " ms: " << *json_;
    } else {
      VLOG(1) << "Listed the built-in splash screen assets: " << *json_;
    }
  }
  state_ = State::kReady;

  // A callback may start listing again, which resets `json_`.
  const std::optional<std::string> json = json_;
  std::vector<Callback> callbacks;
  callbacks.swap(callbacks_);
  for (Callback& callback : callbacks) {
    std::move(callback).Run(json);
  }
}

std::string SplashAssetList::ToJson() const {
  base::Value::List assets;
  for (const BuiltInAsset& asset : kBuiltInAssets) {
    const size_t bytes =
        H5vccSchemeURLLoaderFactory::GetBuiltInResourceSize(asset.name);
    if (bytes) {
      assets.Append(MakeAsset("builtin", "", asset.name, asset.is_video,
                              asset.topic, asset.width, asset.height, bytes));
    }
  }
  for (size_t index = 0; index < kCachedAssetCount; ++index) {
    if (!cached_sizes_[index]) {
      continue;
    }
    const CachedAsset& asset = kCachedAssets[index];
    // The cache's name is the splash screen's topic, as for splash.html. The
    // asset's dimensions aren't known without reading it.
    assets.Append(MakeAsset("cache", cache_name_, asset.name, asset.is_video,
                            cache_name_, /*width=*/0, /*height=*/0,
                            *cached_sizes_[index]));
  }
  return base::WriteJson(assets).value_or("[]");
}

}  // namespace content
