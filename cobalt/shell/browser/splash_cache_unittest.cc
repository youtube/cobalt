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

#include "cobalt/shell/browser/splash_cache.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

using blink::mojom::CacheStorageError;

constexpr char kSplashDomain[] = "https://www.youtube.com/";
constexpr char kHistogram[] = "Cobalt.SplashScreen.ClearCachedFilesResult";

// A CacheStorageCache that deletes from |entries|, or fails or never replies.
class FakeCacheStorageCache : public blink::mojom::CacheStorageCache {
 public:
  FakeCacheStorageCache(std::set<GURL>* entries,
                        CacheStorageError batch_error,
                        bool hang_batch,
                        int* batch_calls)
      : entries_(entries),
        batch_error_(batch_error),
        hang_batch_(hang_batch),
        batch_calls_(batch_calls) {}

  mojo::PendingAssociatedRemote<blink::mojom::CacheStorageCache> Bind() {
    return receiver_.BindNewEndpointAndPassRemote();
  }

  void Disconnect() {
    receiver_.reset();
    pending_batches_.clear();
  }

  // blink::mojom::CacheStorageCache:
  void Match(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::CacheQueryOptionsPtr query_options,
             bool in_related_fetch_event,
             bool in_range_fetch_event,
             int64_t trace_id,
             MatchCallback callback) override {
    NOTREACHED();
  }
  void MatchAll(blink::mojom::FetchAPIRequestPtr request,
                blink::mojom::CacheQueryOptionsPtr query_options,
                int64_t trace_id,
                MatchAllCallback callback) override {
    NOTREACHED();
  }
  void GetAllMatchedEntries(blink::mojom::FetchAPIRequestPtr request,
                            blink::mojom::CacheQueryOptionsPtr query_options,
                            int64_t trace_id,
                            GetAllMatchedEntriesCallback callback) override {
    NOTREACHED();
  }
  void Keys(blink::mojom::FetchAPIRequestPtr request,
            blink::mojom::CacheQueryOptionsPtr query_options,
            int64_t trace_id,
            KeysCallback callback) override {
    NOTREACHED();
  }
  void Batch(std::vector<blink::mojom::BatchOperationPtr> batch_operations,
             int64_t trace_id,
             BatchCallback callback) override {
    ++*batch_calls_;
    ASSERT_EQ(1u, batch_operations.size());
    EXPECT_EQ(blink::mojom::OperationType::kDelete,
              batch_operations[0]->operation_type);
    EXPECT_TRUE(
        batch_operations[0]->match_options.Equals(CreateSplashQueryOptions()));
    if (hang_batch_) {
      pending_batches_.push_back(std::move(callback));
      return;
    }
    if (batch_error_ == CacheStorageError::kSuccess) {
      entries_->erase(batch_operations[0]->request->url);
    }
    std::move(callback).Run(blink::mojom::CacheStorageVerboseError::New(
        batch_error_, std::nullopt));
  }
  void WriteSideData(const GURL& url,
                     base::Time expected_response_time,
                     mojo_base::BigBuffer data,
                     int64_t trace_id,
                     WriteSideDataCallback callback) override {
    NOTREACHED();
  }

 private:
  const raw_ptr<std::set<GURL>> entries_;
  const CacheStorageError batch_error_;
  const bool hang_batch_;
  const raw_ptr<int> batch_calls_;
  std::vector<BatchCallback> pending_batches_;
  mojo::AssociatedReceiver<blink::mojom::CacheStorageCache> receiver_{this};
};

// A CacheStorage with the entries in |caches_|, which can fail or never reply.
class FakeCacheStorage : public blink::mojom::CacheStorage {
 public:
  mojo::PendingRemote<blink::mojom::CacheStorage> Bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Disconnect() {
    receiver_.reset();
    pending_matches_.clear();
  }

  void AddEntry(const std::u16string& cache_name, const std::string& entry) {
    caches_[cache_name].insert(GURL(kSplashDomain).Resolve(entry));
  }
  bool HasEntry(const std::u16string& cache_name, const std::string& entry) {
    return caches_[cache_name].contains(GURL(kSplashDomain).Resolve(entry));
  }
  void DisconnectCaches() {
    for (auto& cache : opened_caches_) {
      cache->Disconnect();
    }
  }

  std::map<std::u16string, CacheStorageError> match_errors_;
  std::optional<CacheStorageError> open_error_;
  CacheStorageError batch_error_ = CacheStorageError::kSuccess;
  bool hang_match_ = false;
  bool hang_batch_ = false;
  int open_calls_ = 0;
  int batch_calls_ = 0;

  // blink::mojom::CacheStorage:
  void Has(const std::u16string& cache_name,
           int64_t trace_id,
           HasCallback callback) override {
    NOTREACHED();
  }
  void Delete(const std::u16string& cache_name,
              int64_t trace_id,
              DeleteCallback callback) override {
    NOTREACHED();
  }
  void Keys(int64_t trace_id, KeysCallback callback) override { NOTREACHED(); }
  void Match(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::MultiCacheQueryOptionsPtr match_options,
             bool in_related_fetch_event,
             bool in_range_fetch_event,
             int64_t trace_id,
             MatchCallback callback) override {
    if (hang_match_) {
      pending_matches_.push_back(std::move(callback));
      return;
    }
    const std::u16string& cache_name = *match_options->cache_name;
    if (auto it = match_errors_.find(cache_name); it != match_errors_.end()) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(it->second));
      return;
    }
    auto cache = caches_.find(cache_name);
    if (cache == caches_.end()) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
          CacheStorageError::kErrorCacheNameNotFound));
      return;
    }
    if (!cache->second.contains(request->url)) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }
    std::move(callback).Run(blink::mojom::MatchResult::NewResponse(
        blink::mojom::FetchAPIResponse::New()));
  }
  void Open(const std::u16string& cache_name,
            int64_t trace_id,
            OpenCallback callback) override {
    ++open_calls_;
    if (open_error_) {
      std::move(callback).Run(
          blink::mojom::OpenResult::NewStatus(*open_error_));
      return;
    }
    opened_caches_.push_back(std::make_unique<FakeCacheStorageCache>(
        &caches_[cache_name], batch_error_, hang_batch_, &batch_calls_));
    std::move(callback).Run(
        blink::mojom::OpenResult::NewCache(opened_caches_.back()->Bind()));
  }

 private:
  std::map<std::u16string, std::set<GURL>> caches_;
  std::vector<MatchCallback> pending_matches_;
  std::vector<std::unique_ptr<FakeCacheStorageCache>> opened_caches_;
  mojo::Receiver<blink::mojom::CacheStorage> receiver_{this};
};

class SplashCacheClearTest : public testing::Test {
 protected:
  // Starts clearing the cached splash files of |cache_storage_|.
  void StartClear() {
    ClearCachedSplashFiles(
        cache_storage_.Bind(), kSplashDomain,
        base::BindOnce([](int* calls) { ++*calls; }, &done_calls_));
  }

  // Clears the cached splash files, and checks that it records |result| and
  // runs the done callback, once.
  void ClearAndExpect(SplashCacheClearResult result) {
    StartClear();
    task_environment_.RunUntilIdle();
    ExpectFinished(result);
  }

  void ExpectFinished(SplashCacheClearResult result) {
    histogram_tester_.ExpectUniqueSample(kHistogram, result, 1);
    EXPECT_EQ(1, done_calls_);
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  FakeCacheStorage cache_storage_;
  int done_calls_ = 0;
};

}  // namespace

TEST(SplashCacheTest, CreateSplashRequest) {
  auto request = CreateSplashRequest("https://www.youtube.com/", "splash.webm");
  EXPECT_EQ(GURL("https://www.youtube.com/splash.webm"), request->url);
  EXPECT_EQ("GET", request->method);
}

// The request is resolved against the origin, where the WebApp caches the
// splash assets, even if the splash domain has a path.
TEST(SplashCacheTest, CreateSplashRequestResolvesAgainstOrigin) {
  auto request =
      CreateSplashRequest("https://www.youtube.com/tv/", "splash.png");
  EXPECT_EQ(GURL("https://www.youtube.com/splash.png"), request->url);
}

TEST(SplashCacheTest, CreateSplashMatchOptions) {
  auto match_options = CreateSplashMatchOptions(u"music");
  EXPECT_EQ(u"music", match_options->cache_name);
  EXPECT_TRUE(match_options->query_options.Equals(CreateSplashQueryOptions()));
}

TEST_F(SplashCacheClearTest, NoCachedFiles) {
  ClearAndExpect(SplashCacheClearResult::kNoCachedFiles);
  EXPECT_EQ(0, cache_storage_.open_calls_);
}

TEST_F(SplashCacheClearTest, Cleared) {
  cache_storage_.AddEntry(u"default", "splash.webm");
  cache_storage_.AddEntry(u"default", "other.txt");
  cache_storage_.AddEntry(u"music", "splash.png");
  ClearAndExpect(SplashCacheClearResult::kCleared);
  EXPECT_EQ(2, cache_storage_.open_calls_);
  EXPECT_EQ(2, cache_storage_.batch_calls_);
  EXPECT_FALSE(cache_storage_.HasEntry(u"default", "splash.webm"));
  EXPECT_FALSE(cache_storage_.HasEntry(u"music", "splash.png"));
  EXPECT_TRUE(cache_storage_.HasEntry(u"default", "other.txt"));
}

// A failed lookup isn't a miss, as the entry may still be cached.
TEST_F(SplashCacheClearTest, MatchError) {
  cache_storage_.match_errors_[u"default"] = CacheStorageError::kErrorStorage;
  cache_storage_.match_errors_[u"music"] = CacheStorageError::kErrorStorage;
  ClearAndExpect(SplashCacheClearResult::kFailed);
  EXPECT_EQ(0, cache_storage_.open_calls_);
}

// The entries that are found are still deleted if other lookups fail.
TEST_F(SplashCacheClearTest, SomeMatchErrors) {
  cache_storage_.AddEntry(u"default", "splash.webm");
  cache_storage_.match_errors_[u"music"] = CacheStorageError::kErrorStorage;
  ClearAndExpect(SplashCacheClearResult::kFailed);
  EXPECT_FALSE(cache_storage_.HasEntry(u"default", "splash.webm"));
}

TEST_F(SplashCacheClearTest, OpenError) {
  cache_storage_.AddEntry(u"default", "splash.webm");
  cache_storage_.open_error_ = CacheStorageError::kErrorStorage;
  ClearAndExpect(SplashCacheClearResult::kFailed);
  EXPECT_TRUE(cache_storage_.HasEntry(u"default", "splash.webm"));
}

TEST_F(SplashCacheClearTest, BatchError) {
  cache_storage_.AddEntry(u"default", "splash.webm");
  cache_storage_.batch_error_ = CacheStorageError::kErrorStorage;
  ClearAndExpect(SplashCacheClearResult::kFailed);
  EXPECT_EQ(1, cache_storage_.batch_calls_);
}

TEST_F(SplashCacheClearTest, CacheStorageDisconnected) {
  cache_storage_.hang_match_ = true;
  StartClear();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, done_calls_);

  cache_storage_.Disconnect();
  task_environment_.RunUntilIdle();
  ExpectFinished(SplashCacheClearResult::kFailed);
}

TEST_F(SplashCacheClearTest, CacheDisconnected) {
  cache_storage_.AddEntry(u"default", "splash.webm");
  cache_storage_.AddEntry(u"music", "splash.webm");
  cache_storage_.hang_batch_ = true;
  StartClear();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, cache_storage_.batch_calls_);
  EXPECT_EQ(0, done_calls_);

  // Both caches disconnect, but it finishes once.
  cache_storage_.DisconnectCaches();
  task_environment_.RunUntilIdle();
  ExpectFinished(SplashCacheClearResult::kFailed);
}

}  // namespace content
