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
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
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
// Only lookups in a given cache are expected, which neither create caches nor
// read other caches.
class FakeCacheStorage : public blink::mojom::CacheStorage {
 public:
  FakeCacheStorage() = default;
  ~FakeCacheStorage() override {
    // Answers the lookups that hang, which mojo expects of their callbacks.
    AnswerLookups();
  }

  // Adds the entry `url`, of `size` bytes, to the cache `cache_name`.
  void AddEntry(const std::u16string& cache_name,
                const std::string& url,
                uint64_t size) {
    caches_[cache_name][GURL(url)] = size;
  }
  // Adds the entry `url`, without a body, to the cache `cache_name`.
  void AddEntryWithoutBlob(const std::u16string& cache_name,
                           const std::string& url) {
    caches_[cache_name][GURL(url)] = std::nullopt;
  }
  // Fails each lookup with `error`.
  void FailLookups(CacheStorageError error) { lookup_error_ = error; }
  // Leaves each lookup unanswered until AnswerLookups(), as a storage that
  // hangs.
  void Hang() { hangs_ = true; }
  // Answers the lookups that hang, and those that follow.
  void AnswerLookups() {
    hangs_ = false;
    std::vector<base::OnceClosure> answers;
    answers.swap(hung_answers_);
    for (base::OnceClosure& answer : answers) {
      std::move(answer).Run();
    }
  }

  // Binds `receiver` to the Cache API of `splash_domain`, as
  // H5vccSchemeURLLoaderFactory::BindSplashCacheStorage() does.
  void Bind(const std::string& splash_domain,
            mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
    bound_domains_.push_back(splash_domain);
    receivers_.Add(this, std::move(receiver));
  }
  // Disconnects, as a storage that goes away.
  void Disconnect() { receivers_.Clear(); }

  bool is_bound() const { return !receivers_.empty(); }
  // The splash domains of the Cache APIs that were bound.
  const std::vector<std::string>& bound_domains() const {
    return bound_domains_;
  }
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
    // Which would read every cache.
    ADD_FAILURE() << "Unexpected Keys()";
    std::move(callback).Run({});
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
    if (hangs_) {
      hung_answers_.push_back(base::BindOnce(
          &FakeCacheStorage::Answer, base::Unretained(this), request->url,
          *match_options->cache_name, std::move(callback)));
      return;
    }
    Answer(request->url, *match_options->cache_name, std::move(callback));
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
  void Answer(const GURL& url,
              const std::u16string& cache_name,
              MatchCallback callback) {
    if (lookup_error_) {
      std::move(callback).Run(
          blink::mojom::MatchResult::NewStatus(*lookup_error_));
      return;
    }
    const auto cache = caches_.find(cache_name);
    if (cache == caches_.end()) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
          CacheStorageError::kErrorCacheNameNotFound));
      return;
    }
    const auto entry = cache->second.find(url);
    if (entry == cache->second.end()) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }
    auto response = blink::mojom::FetchAPIResponse::New();
    if (entry->second) {
      response->blob = blink::mojom::SerializedBlob::New();
      response->blob->size = *entry->second;
      std::ignore = response->blob->blob.InitWithNewPipeAndPassReceiver();
    }
    std::move(callback).Run(
        blink::mojom::MatchResult::NewResponse(std::move(response)));
  }

  // The size of each entry of each cache, or std::nullopt for an entry
  // without a body.
  std::map<std::u16string, std::map<GURL, std::optional<uint64_t>>> caches_;
  std::optional<CacheStorageError> lookup_error_;
  bool hangs_ = false;
  std::vector<base::OnceClosure> hung_answers_;
  std::vector<std::string> bound_domains_;
  std::vector<std::string> lookups_;
  mojo::ReceiverSet<blink::mojom::CacheStorage> receivers_;
};

// Describes each asset of the list `json`, as
// "builtin:<name>:<kind>:<topic>:<width>x<height>" or
// "cache:<cache>/<name>:<kind>:<topic>:<bytes>".
std::vector<std::string> Describe(const std::string& json) {
  SCOPED_TRACE(json);
  std::vector<std::string> descriptions;
  std::optional<base::Value> list = base::JSONReader::Read(json);
  if (!list || !list->is_list()) {
    ADD_FAILURE() << "Not a list";
    return descriptions;
  }
  for (const base::Value& value : list->GetList()) {
    const base::Value::Dict* asset = value.GetIfDict();
    if (!asset) {
      ADD_FAILURE() << "Not an asset: " << value;
      continue;
    }
    const std::string* source = asset->FindString("source");
    const std::string* cache = asset->FindString("cache");
    const std::string* name = asset->FindString("name");
    const std::string* kind = asset->FindString("kind");
    const std::string* topic = asset->FindString("topic");
    const std::optional<int> width = asset->FindInt("width");
    const std::optional<int> height = asset->FindInt("height");
    const std::optional<int> bytes = asset->FindInt("bytes");
    if (!source || !cache || !name || !kind || !topic || !width || !height ||
        !bytes) {
      ADD_FAILURE() << "Incomplete asset: " << value;
      continue;
    }
    if (*source == "builtin") {
      EXPECT_EQ("", *cache);
      // The size of the embedded asset.
      EXPECT_GT(*bytes, 0);
      descriptions.push_back(base::StrCat(
          {*source, ":", *name, ":", *kind, ":", *topic, ":",
           base::NumberToString(*width), "x", base::NumberToString(*height)}));
    } else {
      // Unknown without reading the asset.
      EXPECT_EQ(0, *width);
      EXPECT_EQ(0, *height);
      descriptions.push_back(
          base::StrCat({*source, ":", *cache, "/", *name, ":", *kind, ":",
                        *topic, ":", base::NumberToString(*bytes)}));
    }
  }
  return descriptions;
}

// Tests SplashAssetList with a fake Cache API of the splash domain, in mock
// time, with kEnableCachedSplashFile unless a test disables it. The built-in
// assets are the embedded ones.
class SplashAssetListTest : public testing::Test {
 protected:
  SplashAssetListTest() {
    H5vccSchemeURLLoaderFactory::SetCachedSplashFileEnabledForTesting(true);
    H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(kSplashDomain);
  }
  ~SplashAssetListTest() override {
    H5vccSchemeURLLoaderFactory::SetCachedSplashFileEnabledForTesting(
        std::nullopt);
    H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(std::nullopt);
    H5vccSchemeURLLoaderFactory::SetSplashContentSizeForTesting(std::nullopt);
  }

  // Gets the list of the cache `cache_name`, once it's ready.
  std::optional<std::string> GetList(const std::string& cache_name) {
    base::test::TestFuture<std::optional<std::string>> future;
    list_->Get(cache_name, future.GetCallback());
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  FakeCacheStorage cache_storage_;
  std::unique_ptr<SplashAssetList> list_ = std::make_unique<SplashAssetList>(
      base::BindRepeating(&FakeCacheStorage::Bind,
                          base::Unretained(&cache_storage_)));
};

TEST_F(SplashAssetListTest, ListsBuiltInAssetsWithoutCachedSplashFile) {
  H5vccSchemeURLLoaderFactory::SetCachedSplashFileEnabledForTesting(false);
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);

  std::optional<std::string> json = GetList("default");

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({}), Describe(*json));
  // Without reading the cache, which takes no time to list.
  EXPECT_FALSE(cache_storage_.is_bound());
  histogram_tester_.ExpectTotalCount(kAssetListTime, 0);
}

TEST_F(SplashAssetListTest, ListsAssetsThatSplashHtmlReadsInTheCache) {
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.png", 3);
  // Not where splash.html reads its assets.
  cache_storage_.AddEntry(u"default", "https://splash.test/music_splash.png",
                          5);
  cache_storage_.AddEntry(u"default", "https://splash.test/tv/splash.webm", 6);
  cache_storage_.AddEntry(u"default", "https://other.test/splash.webm", 7);
  cache_storage_.AddEntry(u"music", "https://splash.test/splash.png", 4);

  std::optional<std::string> json = GetList("default");

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({"cache:default/splash.webm:video:default:9",
                              "cache:default/splash.png:image:default:3"}),
            Describe(*json));
  // Only in the given cache.
  EXPECT_EQ(std::vector<std::string>({
                "default:https://splash.test/splash.webm",
                "default:https://splash.test/splash.png",
            }),
            cache_storage_.lookups());
  EXPECT_EQ(std::vector<std::string>({kSplashDomain}),
            cache_storage_.bound_domains());
  histogram_tester_.ExpectUniqueTimeSample(kAssetListTime, base::TimeDelta(),
                                           1);
}

TEST_F(SplashAssetListTest, ListsAssetsOfTheTopicsCache) {
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);
  cache_storage_.AddEntry(u"music", "https://splash.test/splash.png", 4);

  std::optional<std::string> json = GetList("music");

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({"cache:music/splash.png:image:music:4"}),
            Describe(*json));
  EXPECT_EQ(std::vector<std::string>({
                "music:https://splash.test/splash.webm",
                "music:https://splash.test/splash.png",
            }),
            cache_storage_.lookups());
}

// A cache that doesn't exist has no assets, as for the loader.
TEST_F(SplashAssetListTest, ListsBuiltInAssetsWithoutTheCache) {
  cache_storage_.AddEntry(u"music", "https://splash.test/splash.webm", 9);

  std::optional<std::string> json = GetList("default");

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({}), Describe(*json));
  EXPECT_EQ(2u, cache_storage_.lookups().size());
  histogram_tester_.ExpectTotalCount(kAssetListTime, 1);
}

// The loader serves the built-in asset instead of an empty one.
TEST_F(SplashAssetListTest, SkipsEmptyAssets) {
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 0);
  cache_storage_.AddEntryWithoutBlob(u"default",
                                     "https://splash.test/splash.png");

  std::optional<std::string> json = GetList("default");

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({}), Describe(*json));
  histogram_tester_.ExpectTotalCount(kAssetListTime, 1);
}

// The loader serves the built-in asset instead of one over its limit.
TEST_F(SplashAssetListTest, SkipsAssetsOverTheLoadersLimit) {
  H5vccSchemeURLLoaderFactory::SetSplashContentSizeForTesting(10);
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 11);
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.png", 10);

  std::optional<std::string> json = GetList("default");

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({"cache:default/splash.png:image:default:10"}),
            Describe(*json));
}

// The splash domain when listing starts, as for the loader.
TEST_F(SplashAssetListTest, ReadsSplashDomainWhenListing) {
  H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(
      "https://other.test/tv");
  cache_storage_.AddEntry(u"default", "https://other.test/splash.webm", 9);

  std::optional<std::string> json = GetList("default");

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({"cache:default/splash.webm:video:default:9"}),
            Describe(*json));
  EXPECT_EQ(std::vector<std::string>({"https://other.test/"}),
            cache_storage_.bound_domains());
  EXPECT_EQ(std::vector<std::string>({
                "default:https://other.test/splash.webm",
                "default:https://other.test/splash.png",
            }),
            cache_storage_.lookups());
}

TEST_F(SplashAssetListTest, RecordsHowLongListingTook) {
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);
  cache_storage_.Hang();
  base::test::TestFuture<std::optional<std::string>> future;
  list_->Get("default", future.GetCallback());

  task_environment_.FastForwardBy(base::Milliseconds(300));
  cache_storage_.AnswerLookups();

  std::optional<std::string> json = future.Take();
  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({"cache:default/splash.webm:video:default:9"}),
            Describe(*json));
  histogram_tester_.ExpectUniqueTimeSample(kAssetListTime,
                                           base::Milliseconds(300), 1);
}

// splash.html then reads the cache itself, rather than miss an asset.
TEST_F(SplashAssetListTest, IsUnavailableIfLookupFails) {
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);
  cache_storage_.FailLookups(CacheStorageError::kErrorStorage);

  EXPECT_FALSE(GetList("default").has_value());
  histogram_tester_.ExpectTotalCount(kAssetListTime, 0);
}

TEST_F(SplashAssetListTest, IsUnavailableIfStorageDisconnects) {
  cache_storage_.Hang();
  const base::TimeTicks start = base::TimeTicks::Now();
  base::test::TestFuture<std::optional<std::string>> future;
  list_->Get("default", future.GetCallback());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(cache_storage_.is_bound());

  cache_storage_.Disconnect();

  EXPECT_FALSE(future.Take().has_value());
  // Once it disconnected, rather than at the timeout.
  EXPECT_LT(base::TimeTicks::Now() - start, SplashAssetList::kTimeout);
  histogram_tester_.ExpectTotalCount(kAssetListTime, 0);
}

TEST_F(SplashAssetListTest, IsUnavailableAfterTimeout) {
  cache_storage_.Hang();
  base::test::TestFuture<std::optional<std::string>> first;
  base::test::TestFuture<std::optional<std::string>> second;
  list_->Get("default", first.GetCallback());
  list_->Get("default", second.GetCallback());

  task_environment_.FastForwardBy(SplashAssetList::kTimeout -
                                  base::Milliseconds(1));
  EXPECT_FALSE(first.IsReady());
  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_FALSE(first.Take().has_value());
  EXPECT_FALSE(second.Take().has_value());
  histogram_tester_.ExpectTotalCount(kAssetListTime, 0);
}

// splash.html then reads that cache itself.
TEST_F(SplashAssetListTest, IsUnavailableForAnotherCache) {
  cache_storage_.AddEntry(u"music", "https://splash.test/splash.png", 4);
  list_->Start("default");

  // Right away, while listing, and once listed.
  base::test::TestFuture<std::optional<std::string>> while_listing;
  list_->Get("music", while_listing.GetCallback());
  ASSERT_TRUE(while_listing.IsReady());
  EXPECT_FALSE(while_listing.Get().has_value());
  EXPECT_TRUE(GetList("default").has_value());
  EXPECT_FALSE(GetList("music").has_value());

  // Without looking the cache up.
  EXPECT_EQ(std::vector<std::string>({
                "default:https://splash.test/splash.webm",
                "default:https://splash.test/splash.png",
            }),
            cache_storage_.lookups());
}

TEST_F(SplashAssetListTest, KeepsListUntilStartedAgain) {
  list_->Start("default");
  task_environment_.RunUntilIdle();
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);

  std::optional<std::string> json = GetList("default");
  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({}), Describe(*json));

  // As when the next splash screen starts.
  list_->Start("default");
  json = GetList("default");
  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({"cache:default/splash.webm:video:default:9"}),
            Describe(*json));
  histogram_tester_.ExpectTotalCount(kAssetListTime, 2);
}

TEST_F(SplashAssetListTest, IgnoresStartWhileListing) {
  cache_storage_.AddEntry(u"default", "https://splash.test/splash.webm", 9);
  list_->Start("default");
  list_->Start("music");

  std::optional<std::string> json = GetList("default");

  ASSERT_TRUE(json);
  EXPECT_EQ(BuiltInAssetsAnd({"cache:default/splash.webm:video:default:9"}),
            Describe(*json));
  EXPECT_EQ(2u, cache_storage_.lookups().size());
  histogram_tester_.ExpectTotalCount(kAssetListTime, 1);
}

TEST_F(SplashAssetListTest, CanBeDestroyedWhileListing) {
  cache_storage_.Hang();
  base::test::TestFuture<std::optional<std::string>> future;
  list_->Get("default", future.GetCallback());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(cache_storage_.is_bound());

  list_.reset();
  task_environment_.FastForwardBy(SplashAssetList::kTimeout);

  EXPECT_FALSE(cache_storage_.is_bound());
  EXPECT_FALSE(future.IsReady());
  histogram_tester_.ExpectTotalCount(kAssetListTime, 0);
}

}  // namespace

}  // namespace content
