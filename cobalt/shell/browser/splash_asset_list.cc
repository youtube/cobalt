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

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"
#include "cobalt/shell/browser/splash_cache_reader.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/document_isolation_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// The built-in splash screen assets, embedded with splash.html.
struct BuiltInAsset {
  const char* name;
  bool video;
  const char* topic;
  // The size of the video, or 0 if unknown.
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

// How long the listing may take before the list goes out as it is.
constexpr base::TimeDelta kListingTimeout = base::Seconds(2);

base::Value::Dict MakeAsset(std::string_view source,
                            std::string_view cache,
                            std::string_view name,
                            bool video,
                            std::string_view topic,
                            int width,
                            int height,
                            uint64_t bytes) {
  base::Value::Dict asset;
  asset.Set("source", source);
  asset.Set("cache", cache);
  asset.Set("name", name);
  asset.Set("kind", video ? "video" : "image");
  asset.Set("topic", topic);
  asset.Set("width", width);
  asset.Set("height", height);
  asset.Set("bytes", base::saturated_cast<int>(bytes));
  return asset;
}

// Lists the assets once per process. See SplashAssetList.
class Lister {
 public:
  static Lister& Get() {
    static base::NoDestructor<Lister> lister;
    return *lister;
  }

  void Start(BrowserContext* browser_context) {
    if (started_) {
      return;
    }
    started_ = true;
    start_time_ = base::TimeTicks::Now();
    LOG(INFO) << "NativeSplash: Listing the splash assets.";
    for (const BuiltInAsset& asset : kBuiltInAssets) {
      const size_t bytes =
          H5vccSchemeURLLoaderFactory::GetBuiltInResource(asset.name).size();
      if (bytes) {
        assets_.Append(MakeAsset("builtin", "", asset.name, asset.video,
                                 asset.topic, asset.width, asset.height,
                                 bytes));
      }
    }
    // As the reads of the cached assets.
    if (!browser_context || !IsCachedSplashFileEnabled()) {
      Finish();
      return;
    }

    StoragePartition* storage_partition = browser_context->GetStoragePartition(
        StoragePartitionConfig::CreateDefault(browser_context));
    const blink::StorageKey storage_key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(
            GURL(H5vccSchemeURLLoaderFactory::GetSplashDomain())));
    storage_partition->GetCacheStorageControl()->AddReceiver(
        network::CrossOriginEmbedderPolicy(), mojo::NullRemote(),
        network::DocumentIsolationPolicy(), mojo::NullRemote(),
        storage::BucketLocator::ForDefaultBucket(storage_key),
        storage::mojom::CacheStorageOwner::kCacheAPI,
        cache_storage_.BindNewPipeAndPassReceiver());
    cache_storage_.set_disconnect_handler(
        base::BindOnce(&Lister::Finish, base::Unretained(this)));
    timeout_.Start(FROM_HERE, kListingTimeout,
                   base::BindOnce(&Lister::Finish, base::Unretained(this)));
    cache_storage_->Keys(
        /*trace_id=*/0,
        base::BindOnce(&Lister::OnCacheNames, base::Unretained(this)));
  }

  void GetList(BrowserContext* browser_context,
               SplashAssetList::Callback callback) {
    Start(browser_context);
    if (done_) {
      std::move(callback).Run(json_);
      return;
    }
    waiting_.push_back(std::move(callback));
  }

 private:
  friend class base::NoDestructor<Lister>;

  Lister() = default;
  ~Lister() = default;

  void OnCacheNames(const std::vector<std::u16string>& names) {
    if (done_) {
      return;
    }
    pending_caches_ = names.size();
    if (!pending_caches_) {
      Finish();
      return;
    }
    for (const std::u16string& name : names) {
      cache_storage_->Open(
          name, /*trace_id=*/0,
          base::BindOnce(&Lister::OnCacheOpened, base::Unretained(this),
                         base::UTF16ToUTF8(name)));
    }
  }

  void OnCacheOpened(const std::string& cache_name,
                     blink::mojom::OpenResultPtr result) {
    if (done_) {
      return;
    }
    if (!result->is_cache()) {
      OnCacheListed();
      return;
    }
    auto& cache = caches_.emplace_back(
        std::make_unique<
            mojo::AssociatedRemote<blink::mojom::CacheStorageCache>>(
            std::move(result->get_cache())));
    (*cache)->GetAllMatchedEntries(
        /*request=*/nullptr, blink::mojom::CacheQueryOptions::New(),
        /*trace_id=*/0,
        base::BindOnce(&Lister::OnCacheEntries, base::Unretained(this),
                       cache_name));
  }

  void OnCacheEntries(const std::string& cache_name,
                      blink::mojom::GetAllMatchedEntriesResultPtr result) {
    if (done_) {
      return;
    }
    if (result->is_entries()) {
      for (const auto& entry : result->get_entries()) {
        const std::string path = entry->request->url.path();
        const std::string name = path.substr(path.rfind('/') + 1);
        const bool video = base::EndsWith(name, ".webm");
        if (!video && !base::EndsWith(name, ".png")) {
          continue;
        }
        const uint64_t bytes = entry->response && entry->response->blob
                                   ? entry->response->blob->size
                                   : 0;
        // The cache's name is the topic, as for splash.html.
        assets_.Append(MakeAsset("cache", cache_name, name, video, cache_name,
                                 0, 0, bytes));
        ++cached_assets_;
      }
    }
    OnCacheListed();
  }

  void OnCacheListed() {
    DCHECK_GT(pending_caches_, 0u);
    if (--pending_caches_ == 0) {
      Finish();
    }
  }

  void Finish() {
    if (done_) {
      return;
    }
    done_ = true;
    timeout_.Stop();
    caches_.clear();
    cache_storage_.reset();
    base::JSONWriter::Write(assets_, &json_);
    LOG(INFO) << "NativeSplash: Listed " << assets_.size() << " splash assets ("
              << cached_assets_ << " cached) in "
              << (base::TimeTicks::Now() - start_time_).InMilliseconds()
              << "ms.";
    std::vector<SplashAssetList::Callback> waiting = std::move(waiting_);
    for (auto& callback : waiting) {
      std::move(callback).Run(json_);
    }
  }

  bool started_ = false;
  bool done_ = false;
  base::TimeTicks start_time_;
  base::Value::List assets_;
  size_t cached_assets_ = 0;
  size_t pending_caches_ = 0;
  std::string json_;
  std::vector<SplashAssetList::Callback> waiting_;
  mojo::Remote<blink::mojom::CacheStorage> cache_storage_;
  std::vector<
      std::unique_ptr<mojo::AssociatedRemote<blink::mojom::CacheStorageCache>>>
      caches_;
  base::OneShotTimer timeout_;
};

}  // namespace

// static
void SplashAssetList::Start(BrowserContext* browser_context) {
  Lister::Get().Start(browser_context);
}

// static
void SplashAssetList::Get(BrowserContext* browser_context, Callback callback) {
  Lister::Get().GetList(browser_context, std::move(callback));
}

}  // namespace content
