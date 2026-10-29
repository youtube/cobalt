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

#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

using blink::mojom::CacheStorageError;

constexpr char kSplashDomain[] = "https://splash.test/";
constexpr char kAssetListTime[] =
    "Cobalt.SplashScreen.WasmPolicy.AssetListTime";

// The built-in assets, as Describe() describes them, followed by
// `cached_assets`.
std::vector<std::string> BuiltInAssetsAnd(
    const std::vector<std::string>& cached_assets) {
  std::vector<std::string> assets = {
      "builtin:splash.webm:video:default:1920x1080",
      "builtin:splash_480.webm:video:default:853x480",
      "builtin:music_splash.webm:video:music:1920x1080",
      "builtin:music_splash_480.webm:video:music:853x480",
      "builtin:splash.png:image:default:0x0",
      "builtin:music_splash.png:image:music:0x0",
  };
  assets.insert(assets.end(), cached_assets.begin(), cached_assets.end());
  return assets;
}

// The Cache API of the splash domain, with the entries that the tests add.
class FakeCacheStorage : public blink::mojom::CacheStorage {
 public:
  FakeCacheStorage() = default;
  ~FakeCacheStorage() override {
    for (KeysCallback& callback : hung_keys_callbacks_) {
      std::move(callback).Run({});
    }
  }

  // Adds the entry `url`, of `size` bytes, to the cache `cache_name`.
  void AddEntry(const std::u16string& cache_name,
                const std::string& url,
                uint64_t size) {
    caches_[cache_name][GURL(url)] = size;
  }
  // Lists the cache `cache_name`, which then can't be found, as if it were
  // deleted meanwhile.
  void AddDeletedCache(const std::u16string& cache_name) {
    deleted_caches_.insert(cache_name);
  }
  // Fails each lookup with `error`.
  void FailLookups(CacheStorageError error) { lookup_error_ = error; }
  // Never lists the caches, as a storage that hangs.
  void Hang() { hangs_ = true; }

  void Bind(mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
    receivers_.Add(this, std::move(receiver));
  }
  // Disconnects, as a storage that goes away.
  void Disconnect() { receivers_.Clear(); }

  bool is_bound() const { return !receivers_.empty(); }
  // The lookups, as "<cache name>:<URL>".
  const std::vector<std::string>& lookups() const { return lookups_; }

  // blink::mojom::CacheStorage:
  void Has(const std::u16string& cache_name,
           int64_t trace_id,
           HasCallback callback) override {
    ADD_FAILURE() << "Unexpected Has()";
    std::move(callback).Run(CacheStorageError::kErrorNotImplemented);
  }
  void Delete(const std::u16string& cache_name,
              int64_t trace_id,
              DeleteCallback callback) override {
    ADD_FAILURE() << "Unexpected Delete()";
    std::move(callback).Run(CacheStorageError::kErrorNotImplemented);
  }
  void Keys(int64_t trace_id, KeysCallback callback) override {
    if (hangs_) {
      hung_keys_callbacks_.push_back(std::move(callback));
      return;
    }
    std::vector<std::u16string> cache_names;
    for (const auto& [cache_name, entries] : caches_) {
      cache_names.push_back(cache_name);
    }
    cache_names.insert(cache_names.end(), deleted_caches_.begin(),
                       deleted_caches_.end());
    std::move(callback).Run(cache_names);
  }
  void Match(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::MultiCacheQueryOptionsPtr match_options,
             bool in_related_fetch_event,
             bool in_range_fetch_event,
             int64_t trace_id,
             MatchCallback callback) override {
    if (!match_options->cache_name) {
      // Which would look the asset up in every cache.
      ADD_FAILURE() << "Lookup without a cache name";
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
          CacheStorageError::kErrorStorage));
      return;
    }
    lookups_.push_back(
        base::StrCat({base::UTF16ToUTF8(*match_options->cache_name), ":",
                      request->url.spec()}));
    EXPECT_EQ("GET", request->method);
    if (lookup_error_) {
      std::move(callback).Run(
          blink::mojom::MatchResult::NewStatus(*lookup_error_));
      return;
    }
    const auto cache = caches_.find(*match_options->cache_name);
    if (cache == caches_.end()) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
          CacheStorageError::kErrorCacheNameNotFound));
      return;
    }
    const auto entry = cache->second.find(request->url);
    if (entry == cache->second.end()) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }
    auto response = blink::mojom::FetchAPIResponse::New();
    response->blob = blink::mojom::SerializedBlob::New();
    response->blob->size = entry->second;
    std::ignore = response->blob->blob.InitWithNewPipeAndPassReceiver();
    std::move(callback).Run(
        blink::mojom::MatchResult::NewResponse(std::move(response)));
  }
  void Open(const std::u16string& cache_name,
            int64_t trace_id,
            OpenCallback callback) override {
    // Which would create the cache if it didn't exist.
    ADD_FAILURE() << "Unexpected Open()";
    std::move(callback).Run(
        blink::mojom::OpenResult::NewStatus(CacheStorageError::kErrorStorage));
  }

 private:
  std::map<std::u16string, std::map<GURL, uint64_t>> caches_;
  std::set<std::u16string> deleted_caches_;
  std::optional<CacheStorageError> lookup_error_;
  bool hangs_ = false;
  std::vector<KeysCallback> hung_keys_callbacks_;
  std::vector<std::string> lookups_;
  mojo::ReceiverSet<blink::mojom::CacheStorage> receivers_;
};

// Describes each asset of the list `json`, as
// "builtin:<name>:<kind>:<topic>:<width>x<height>" or
// "cache:<cache>/<name>:<kind>:<topic>:<bytes>".
std::vector<std::string> Describe(const std::string& json) {
  std::vector<std::string> descriptions;
  std::optional<base::Value> list = base::JSONReader::Read(json);
  if (!list || !list->is_list()) {
    ADD_FAILURE() << "Not a list: " << json;
    return descriptions;
  }
  for (const base::Value& value : list->GetList()) {
    const base::Value::Dict& asset = value.GetDict();
    const std::string& source = *asset.FindString("source");
    if (source == "builtin") {
      EXPECT_EQ("", *asset.FindString("cache"));
      // The size of the embedded asset.
      EXPECT_GT(*asset.FindInt("bytes"), 0);
      descriptions.push_back(base::StrCat(
          {source, ":", *asset.FindString("name"), ":",
           *asset.FindString("kind"), ":", *asset.FindString("topic"), ":",
           base::NumberToString(*asset.FindInt("width")), "x",
           base::NumberToString(*asset.FindInt("height"))}));
    } else {
      descriptions.push_back(base::StrCat(
          {source, ":", *asset.FindString("cache"), "/",
           *asset.FindString("name"), ":", *asset.FindString("kind"), ":",
           *asset.FindString("topic"), ":",
           base::NumberToString(*asset.FindInt("bytes"))}));
    }
  }
  return descriptions;
}

class SplashAssetListTest : public testing::Test {
 protected:
  SplashAssetListTest() {
    H5vccSchemeURLLoaderFactory::SetCachedSplashFileEnabledForTesting(true);
  }
  ~SplashAssetListTest() override {
    H5vccSchemeURLLoaderFactory::SetCachedSplashFileEnabledForTesting(
        std::nullopt);
  }

  // Gets the list, once it's ready.
  std::optional<std::string> GetList() {
    base::test::TestFuture<std::optional<std::string>> future;
    list_.Get(future.GetCallback());
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  FakeCacheStorage cache_storage_;
  SplashAssetList list_{kSplashDomain,
                        base::BindRepeating(&FakeCacheStorage::Bind,
                                            base::Unretained(&cache_storage_))};
};

TEST_F(SplashAssetListTest, ListsBuiltInAssetsWithoutCachedSplashFile) {
  H5vccSchemeURLLoaderFactory::SetCachedSplashFileEnabledForTesting(false);
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);

  std::optional<std::string> json = GetList();

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({}), Describe(*json));
  // Without reading the cache.
  EXPECT_FALSE(cache_storage_.is_bound());
  histogram_tester_.ExpectTotalCount(kAssetListTime, 1);
}

TEST_F(SplashAssetListTest, ListsAssetsThatSplashHtmlReadsInEachCache) {
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.png", 3);
  cache_storage_.AddEntry(u"music", "https://splash.test/splash.png", 4);
  // Not where splash.html reads its assets.
  cache_storage_.AddEntry(u"music", "https://splash.test/music_splash.png", 5);
  cache_storage_.AddEntry(u"other", "https://splash.test/tv/splash.webm", 6);
  cache_storage_.AddEntry(u"other", "https://other.test/splash.webm", 7);

  std::optional<std::string> json = GetList();

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({"cache:default/splash.webm:video:default:9",
                              "cache:default/splash.png:image:default:3",
                              "cache:music/splash.png:image:music:4"}),
            Describe(*json));
  EXPECT_EQ(std::vector<std::string>({
                "default:https://splash.test/splash.webm",
                "default:https://splash.test/splash.png",
                "music:https://splash.test/splash.webm",
                "music:https://splash.test/splash.png",
                "other:https://splash.test/splash.webm",
                "other:https://splash.test/splash.png",
            }),
            cache_storage_.lookups());
  histogram_tester_.ExpectTotalCount(kAssetListTime, 1);
}

TEST_F(SplashAssetListTest, ListsBuiltInAssetsWithoutCaches) {
  std::optional<std::string> json = GetList();

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({}), Describe(*json));
}

// A cache that was deleted after it was listed has no assets, as for the
// loader.
TEST_F(SplashAssetListTest, IgnoresDeletedCache) {
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);
  cache_storage_.AddDeletedCache(u"music");

  std::optional<std::string> json = GetList();

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({"cache:default/splash.webm:video:default:9"}),
            Describe(*json));
}

// splash.html then reads the cache itself, rather than miss an asset.
TEST_F(SplashAssetListTest, IsUnavailableIfLookupFails) {
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);
  cache_storage_.FailLookups(CacheStorageError::kErrorStorage);

  EXPECT_FALSE(GetList().has_value());
  histogram_tester_.ExpectTotalCount(kAssetListTime, 1);
}

TEST_F(SplashAssetListTest, IsUnavailableIfStorageDisconnects) {
  cache_storage_.Hang();
  base::test::TestFuture<std::optional<std::string>> future;
  list_.Get(future.GetCallback());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(cache_storage_.is_bound());

  cache_storage_.Disconnect();

  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(SplashAssetListTest, IsUnavailableAfterTimeout) {
  cache_storage_.Hang();
  base::test::TestFuture<std::optional<std::string>> first;
  base::test::TestFuture<std::optional<std::string>> second;
  list_.Get(first.GetCallback());
  list_.Get(second.GetCallback());

  task_environment_.FastForwardBy(SplashAssetList::kTimeout -
                                  base::Milliseconds(1));
  EXPECT_FALSE(first.IsReady());
  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_FALSE(first.Take().has_value());
  EXPECT_FALSE(second.Take().has_value());
  histogram_tester_.ExpectUniqueTimeSample(kAssetListTime,
                                           SplashAssetList::kTimeout, 1);
}

TEST_F(SplashAssetListTest, KeepsListUntilStartedAgain) {
  list_.Start();
  task_environment_.RunUntilIdle();
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);

  std::optional<std::string> json = GetList();
  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({}), Describe(*json));

  // As when the next splash screen starts.
  list_.Start();
  json = GetList();
  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({"cache:default/splash.webm:video:default:9"}),
            Describe(*json));
  histogram_tester_.ExpectTotalCount(kAssetListTime, 2);
}

TEST_F(SplashAssetListTest, IgnoresStartWhileListing) {
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);
  list_.Start();
  list_.Start();

  std::optional<std::string> json = GetList();

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({"cache:default/splash.webm:video:default:9"}),
            Describe(*json));
  EXPECT_EQ(2u, cache_storage_.lookups().size());
  histogram_tester_.ExpectTotalCount(kAssetListTime, 1);
}

}  // namespace

}  // namespace content
