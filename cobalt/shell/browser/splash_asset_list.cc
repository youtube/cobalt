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
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/document_isolation_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

namespace {

const char kUserDataKey[] = "SplashAssetList";

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

// Binds `receiver` to the Cache API of `splash_domain`, as the loader does to
// read the cached splash screen assets.
void BindCacheStorage(
    BrowserContext* browser_context,
    const std::string& splash_domain,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  StoragePartition* storage_partition = browser_context->GetStoragePartition(
      StoragePartitionConfig::CreateDefault(browser_context));
  const blink::StorageKey storage_key = blink::StorageKey::CreateFirstParty(
      url::Origin::Create(GURL(splash_domain)));
  storage_partition->GetCacheStorageControl()->AddReceiver(
      network::CrossOriginEmbedderPolicy(), mojo::NullRemote(),
      network::DocumentIsolationPolicy(), mojo::NullRemote(),
      storage::BucketLocator::ForDefaultBucket(storage_key),
      storage::mojom::CacheStorageOwner::kCacheAPI, std::move(receiver));
}

}  // namespace

// static
SplashAssetList* SplashAssetList::GetOrCreateForBrowserContext(
    BrowserContext* browser_context) {
  CHECK(browser_context);
  auto* list =
      static_cast<SplashAssetList*>(browser_context->GetUserData(kUserDataKey));
  if (!list) {
    const std::string splash_domain =
        H5vccSchemeURLLoaderFactory::GetSplashDomain();
    // The list is the BrowserContext's, so it doesn't outlive it.
    auto owned_list = std::make_unique<SplashAssetList>(
        splash_domain,
        base::BindRepeating(&BindCacheStorage,
                            base::Unretained(browser_context), splash_domain));
    list = owned_list.get();
    browser_context->SetUserData(kUserDataKey, std::move(owned_list));
  }
  return list;
}

SplashAssetList::SplashAssetList(const std::string& splash_domain,
                                 CacheStorageBinder bind_cache_storage)
    : splash_domain_(splash_domain),
      bind_cache_storage_(std::move(bind_cache_storage)) {}

SplashAssetList::~SplashAssetList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SplashAssetList::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kListing) {
    return;
  }
  state_ = State::kListing;
  start_time_ = base::TimeTicks::Now();
  cache_names_.clear();
  cached_sizes_.clear();
  pending_lookups_ = 0;
  json_.reset();

  // splash.html only reads the cache with kEnableCachedSplashFile.
  if (!H5vccSchemeURLLoaderFactory::IsCachedSplashFileEnabled()) {
    Finish(/*complete=*/true);
    return;
  }

  // Unretained: `this` owns `cache_storage_` and `timeout_`, whose callbacks
  // are dropped when Finish() resets them.
  bind_cache_storage_.Run(cache_storage_.BindNewPipeAndPassReceiver());
  cache_storage_.set_disconnect_handler(base::BindOnce(
      &SplashAssetList::Finish, base::Unretained(this), /*complete=*/false));
  timeout_.Start(FROM_HERE, kTimeout,
                 base::BindOnce(&SplashAssetList::Finish,
                                base::Unretained(this), /*complete=*/false));
  cache_storage_->Keys(
      /*trace_id=*/0,
      base::BindOnce(&SplashAssetList::OnCacheNames, base::Unretained(this)));
}

void SplashAssetList::Get(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kReady) {
    std::move(callback).Run(json_);
    return;
  }
  callbacks_.push_back(std::move(callback));
  if (state_ == State::kIdle) {
    Start();
  }
}

void SplashAssetList::OnCacheNames(
    const std::vector<std::u16string>& cache_names) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_names_ = cache_names;
  cached_sizes_.assign(cache_names_.size() * kCachedAssetCount, std::nullopt);
  pending_lookups_ = cached_sizes_.size();
  if (!pending_lookups_) {
    Finish(/*complete=*/true);
    return;
  }
  // Looks each asset up as the loader reads it, in a given cache, which
  // neither creates the cache, as opening it would, nor reads every entry.
  for (size_t lookup = 0; lookup < cached_sizes_.size(); ++lookup) {
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url =
        splash_domain_.Resolve(kCachedAssets[lookup % kCachedAssetCount].name);
    request->method = "GET";
    auto match_options = blink::mojom::MultiCacheQueryOptions::New();
    match_options->query_options = blink::mojom::CacheQueryOptions::New();
    match_options->cache_name = cache_names_[lookup / kCachedAssetCount];
    cache_storage_->Match(std::move(request), std::move(match_options),
                          /*in_related_fetch_event=*/false,
                          /*in_range_fetch_event=*/false,
                          /*trace_id=*/0,
                          base::BindOnce(&SplashAssetList::OnCacheMatched,
                                         base::Unretained(this), lookup));
  }
}

void SplashAssetList::OnCacheMatched(size_t lookup,
                                     blink::mojom::MatchResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result->is_status()) {
    // As for the loader, a missing entry, or a cache deleted since it was
    // listed, is a cache miss, while other errors leave the list incomplete.
    const blink::mojom::CacheStorageError status = result->get_status();
    if (status != blink::mojom::CacheStorageError::kErrorNotFound &&
        status != blink::mojom::CacheStorageError::kErrorCacheNameNotFound) {
      LOG(WARNING) << "Failed to list the cached splash screen assets: "
                   << static_cast<int>(status);
      Finish(/*complete=*/false);
      return;
    }
  } else {
    const blink::mojom::FetchAPIResponsePtr& response =
        result->is_response() ? result->get_response()
                              : result->get_eager_response()->response;
    cached_sizes_[lookup] = response->blob ? response->blob->size : 0;
  }
  DCHECK_GT(pending_lookups_, 0u);
  if (!--pending_lookups_) {
    Finish(/*complete=*/true);
  }
}

void SplashAssetList::Finish(bool complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::kListing);
  timeout_.Stop();
  cache_storage_.reset();
  const base::TimeDelta elapsed = base::TimeTicks::Now() - start_time_;
  base::UmaHistogramTimes("Cobalt.SplashScreen.WasmPolicy.AssetListTime",
                          elapsed);
  if (complete) {
    json_ = ToJson();
    VLOG(1) << "Listed the splash screen assets in " << elapsed.InMilliseconds()
            << " ms: " << *json_;
  } else {
    LOG(WARNING) << "Couldn't list all the cached splash screen assets, in "
                 << elapsed.InMilliseconds() << " ms.";
  }
  state_ = State::kReady;

  std::vector<Callback> callbacks;
  callbacks.swap(callbacks_);
  for (Callback& callback : callbacks) {
    std::move(callback).Run(json_);
  }
}

std::string SplashAssetList::ToJson() const {
  base::Value::List assets;
  for (const BuiltInAsset& asset : kBuiltInAssets) {
    const size_t bytes =
        H5vccSchemeURLLoaderFactory::GetBuiltInResource(asset.name).size();
    if (bytes) {
      assets.Append(MakeAsset("builtin", "", asset.name, asset.is_video,
                              asset.topic, asset.width, asset.height, bytes));
    }
  }
  for (size_t lookup = 0; lookup < cached_sizes_.size(); ++lookup) {
    if (!cached_sizes_[lookup]) {
      continue;
    }
    const std::string cache_name =
        base::UTF16ToUTF8(cache_names_[lookup / kCachedAssetCount]);
    const CachedAsset& asset = kCachedAssets[lookup % kCachedAssetCount];
    // The cache's name is the splash screen's topic, as for splash.html. The
    // asset's dimensions aren't known without reading it.
    assets.Append(MakeAsset("cache", cache_name, asset.name, asset.is_video,
                            cache_name, /*width=*/0, /*height=*/0,
                            *cached_sizes_[lookup]));
  }
  std::string json;
  base::JSONWriter::Write(assets, &json);
  return json;
}

}  // namespace content
