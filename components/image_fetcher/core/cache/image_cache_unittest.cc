// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_cache.h"

#include <map>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/image_fetcher/core/cache/image_data_store_disk.h"
#include "components/image_fetcher/core/cache/image_metadata_store_leveldb.h"
#include "components/image_fetcher/core/cache/proto/cached_image_metadata.pb.h"
#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using testing::Mock;

namespace image_fetcher {

namespace {

constexpr char kPrefLastStartupEviction[] =
    "cached_image_fetcher_last_startup_eviction_time";
constexpr char kImageFetcherEventHistogramName[] = "ImageFetcher.Events";
constexpr char kImageUrl[] = "http://gstatic.img.com/foo.jpg";
constexpr char kOtherImageUrl[] = "http://gstatic.img.com/bar.jpg";
constexpr char kImageUrlHashed[] = "3H7UODDH3WKDWK6FQ3IZT3LQMVBPYJ4M";
constexpr char kImageData[] = "data";
const int kOverMaxCacheSize = 65 * 1024 * 1024;

}  // namespace

class CachedImageFetcherImageCacheTest : public testing::Test {
 public:
  CachedImageFetcherImageCacheTest() {}

  CachedImageFetcherImageCacheTest(const CachedImageFetcherImageCacheTest&) =
      delete;
  CachedImageFetcherImageCacheTest& operator=(
      const CachedImageFetcherImageCacheTest&) = delete;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void CreateImageCache() {
    clock_.SetNow(base::Time());

    auto db = std::make_unique<FakeDB<CachedImageMetadataProto>>(&db_store_);
    db_ = db.get();
    auto metadata_store =
        std::make_unique<ImageMetadataStoreLevelDB>(std::move(db), &clock_);
    metadata_store_ = metadata_store.get();

    auto data_store = std::make_unique<ImageDataStoreDisk>(
        temp_dir_.GetPath(), base::SequencedTaskRunner::GetCurrentDefault());
    data_store_ = data_store.get();

    ImageCache::RegisterProfilePrefs(test_prefs_.registry());
    image_cache_ = base::MakeRefCounted<ImageCache>(
        std::move(data_store), std::move(metadata_store), &test_prefs_, &clock_,
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void InitializeImageCache() {
    image_cache_->MaybeStartInitialization();
    db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    RunUntilIdle();
    ASSERT_TRUE(metadata_store()->IsInitialized());
  }

  void PrepareImageCache(bool needs_transcoding) {
    CreateImageCache();
    InitializeImageCache();

    image_cache()->SaveImage(kImageUrl, kImageData, needs_transcoding,
                             absl::nullopt /* expiration_interval */);
    RunUntilIdle();

    ASSERT_TRUE(IsMetadataPresent(kImageUrlHashed));
  }

  bool IsCacheInitialized() {
    return image_cache()->AreAllDependenciesInitialized();
  }

  void RunEvictionOnStartup(bool success) {
    image_cache()->RunEvictionOnStartup();

    if (success) {
      db()->LoadCallback(true);
      db()->UpdateCallback(true);
      db()->LoadKeysCallback(true);
    }

    RunUntilIdle();
  }

  void RunEvictionWhenFull(bool success) {
    image_cache()->RunEvictionWhenFull();

    if (success) {
      db()->LoadCallback(true);
      db()->UpdateCallback(true);
    }

    RunUntilIdle();
  }

  // Loads the image and verify the data callback.
  void LoadImage(const std::string& url, const std::string& expected_data) {
    EXPECT_CALL(*this, DataCallback(false, expected_data));
    image_cache()->LoadImage(
        false, url,
        base::BindOnce(&CachedImageFetcherImageCacheTest::DataCallback,
                       base::Unretained(this)));
    db()->LoadCallback(true);
    RunUntilIdle();
  }

  bool IsMetadataPresent(const std::string& key) {
    return db_store_.find(key) != db_store_.end();
  }

  CachedImageMetadataProto GetMetadata(const std::string& key) {
    if (!IsMetadataPresent(key)) {
      return CachedImageMetadataProto();
    }

    return CachedImageMetadataProto(db_store_[key]);
  }

  bool IsMetadataEqual(const CachedImageMetadataProto lhs,
                       const CachedImageMetadataProto rhs) {
    std::string lhs_str;
    lhs.SerializeToString(&lhs_str);
    std::string rhs_str;
    rhs.SerializeToString(&rhs_str);

    return lhs_str == rhs_str;
  }

  void RunReconciliation() {
    image_cache()->RunReconciliation();
    db()->LoadKeysCallback(true);
    RunUntilIdle();
    db()->UpdateCallback(true);
    RunUntilIdle();
  }

  void InjectMetadata(std::string key, int data_size, bool needs_transcoding) {
    metadata_store_->SaveImageMetadata(key, data_size, needs_transcoding,
                                       absl::nullopt /* expiration_interval */);
  }

  void InjectData(std::string key, std::string data, bool needs_transcoding) {
    data_store_->SaveImage(key, data, needs_transcoding);
    RunUntilIdle();
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  TestingPrefServiceSimple* prefs() { return &test_prefs_; }
  base::SimpleTestClock* clock() { return &clock_; }
  ImageCache* image_cache() { return image_cache_.get(); }
  ImageDataStoreDisk* data_store() { return data_store_; }
  ImageMetadataStoreLevelDB* metadata_store() { return metadata_store_; }
  FakeDB<CachedImageMetadataProto>* db() { return db_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  MOCK_METHOD2(DataCallback, void(bool, std::string));

 private:
  scoped_refptr<ImageCache> image_cache_;
  raw_ptr<ImageMetadataStoreLevelDB> metadata_store_;
  raw_ptr<ImageDataStoreDisk> data_store_;
  base::SimpleTestClock clock_;

  TestingPrefServiceSimple test_prefs_;
  base::ScopedTempDir temp_dir_;
  raw_ptr<FakeDB<CachedImageMetadataProto>> db_;
  std::map<std::string, CachedImageMetadataProto> db_store_;

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(CachedImageFetcherImageCacheTest, HashUrlToKeyTest) {
  ASSERT_EQ(ImageCache::HashUrlToKey("foo"), ImageCache::HashUrlToKey("foo"));
  ASSERT_NE(ImageCache::HashUrlToKey("foo"), ImageCache::HashUrlToKey("bar"));
}

TEST_F(CachedImageFetcherImageCacheTest, SanityTest) {
  CreateImageCache();
  InitializeImageCache();

  image_cache()->SaveImage(kImageUrl, kImageData,
                           /* needs_transcoding */ false,
                           /* expiration_interval */ absl::nullopt);
  RunUntilIdle();

  LoadImage(kImageUrl, kImageData);
  image_cache()->DeleteImage(kImageUrl);
  RunUntilIdle();

  EXPECT_CALL(*this, DataCallback(false, std::string()));
  image_cache()->LoadImage(
      false, kImageUrl,
      base::BindOnce(&CachedImageFetcherImageCacheTest::DataCallback,
                     base::Unretained(this)));
  db()->LoadCallback(true);
  RunUntilIdle();
}

TEST_F(CachedImageFetcherImageCacheTest, SaveCallsInitialization) {
  CreateImageCache();

  ASSERT_FALSE(IsCacheInitialized());
  image_cache()->SaveImage(kImageUrl, kImageData,
                           /* needs_transcoding */ false,
                           /* expiration_interval */ absl::nullopt);
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  RunUntilIdle();

  ASSERT_TRUE(IsCacheInitialized());
}

TEST_F(CachedImageFetcherImageCacheTest, Save) {
  CreateImageCache();
  InitializeImageCache();

  image_cache()->SaveImage(kImageUrl, kImageData,
                           /* needs_transcoding */ false,
                           /* expiration_interval */ absl::nullopt);
  LoadImage(kImageUrl, kImageData);
}

TEST_F(CachedImageFetcherImageCacheTest, Load) {
  PrepareImageCache(false);
  auto metadata_before = GetMetadata(kImageUrlHashed);

  clock()->SetNow(clock()->Now() + base::Hours(1));
  LoadImage(kImageUrl, kImageData);
  db()->LoadCallback(true);
  db()->UpdateCallback(true);
  RunUntilIdle();

  auto metadata_after = GetMetadata(kImageUrlHashed);
  ASSERT_FALSE(IsMetadataEqual(metadata_before, metadata_after));
}

TEST_F(CachedImageFetcherImageCacheTest, LoadReadOnly) {
  PrepareImageCache(false);
  auto metadata_before = GetMetadata(kImageUrlHashed);

  clock()->SetNow(clock()->Now() + base::Hours(1));
  LoadImage(kImageUrl, kImageData);

  auto metadata_after = GetMetadata(kImageUrlHashed);
  ASSERT_TRUE(IsMetadataEqual(metadata_before, metadata_after));
}

TEST_F(CachedImageFetcherImageCacheTest, Delete) {
  PrepareImageCache(false);

  LoadImage(kImageUrl, kImageData);
  image_cache()->DeleteImage(kImageUrl);
  RunUntilIdle();

  LoadImage(kImageUrl, "");
}

TEST_F(CachedImageFetcherImageCacheTest, Eviction) {
  PrepareImageCache(false);

  clock()->SetNow(clock()->Now() + base::Days(7));
  RunEvictionOnStartup(/* success */ true);
  ASSERT_EQ(clock()->Now(), prefs()->GetTime(kPrefLastStartupEviction));

  LoadImage(kImageUrl, "");
  histogram_tester().ExpectBucketCount(
      kImageFetcherEventHistogramName,
      ImageFetcherEvent::kCacheStartupEvictionStarted, 1);
  histogram_tester().ExpectBucketCount(
      kImageFetcherEventHistogramName,
      ImageFetcherEvent::kCacheStartupEvictionFinished, 1);
}

// Verifies eviction for CacheStrategy::HOLD_UNTIL_EXPIRED.
TEST_F(CachedImageFetcherImageCacheTest, EvictionHoldUtilExpires) {
  PrepareImageCache(false);
  clock()->SetNow(clock()->Now() + base::Days(2));

  image_cache()->SaveImage(kImageUrl, "image_data", false, base::Days(10));

  image_cache()->SaveImage(kOtherImageUrl, "other_image_data", false,
                           base::Hours(1));
  RunUntilIdle();

  // Forward the clock to make image with |kOtherImageUrl| expired.
  clock()->SetNow(clock()->Now() + base::Hours(3));
  RunEvictionOnStartup(/* success */ true);
  LoadImage(kImageUrl, "image_data");
  LoadImage(kOtherImageUrl, "");
}

TEST_F(CachedImageFetcherImageCacheTest, EvictionWhenFull) {
  PrepareImageCache(false);
  InjectMetadata(kImageUrl, kOverMaxCacheSize, /* needs_transcoding */ false);
  clock()->SetNow(clock()->Now() + base::Days(6));
  RunEvictionWhenFull(/* success */ true);

  // The data should be removed because it's over the allowed limit.
  LoadImage(kImageUrl, "");
}

TEST_F(CachedImageFetcherImageCacheTest, EvictionTooSoon) {
  PrepareImageCache(false);

  clock()->SetNow(clock()->Now() + base::Days(6));
  RunEvictionOnStartup(/* success */ true);

  LoadImage(kImageUrl, kImageData);
}

TEST_F(CachedImageFetcherImageCacheTest, EvictionWhenEvictionAlreadyPerformed) {
  PrepareImageCache(false);

  prefs()->SetTime("cached_image_fetcher_last_startup_eviction_time",
                   clock()->Now());
  clock()->SetNow(clock()->Now() + base::Hours(23));
  RunEvictionOnStartup(/* success */ false);
  LoadImage(kImageUrl, kImageData);
}

TEST_F(CachedImageFetcherImageCacheTest, Reconciliation) {
  CreateImageCache();
  InitializeImageCache();

  // Inject differing keys so they mismatch, then run reconciliation.
  InjectData("foo", "z", /* needs_transcoding */ false);
  InjectMetadata("bar", 10, /* needs_transcoding */ false);
  RunReconciliation();

  // Data should be gone.
  LoadImage("foo", "");

  // Metadata should be gone.
  ASSERT_FALSE(IsMetadataPresent("bar"));
}

TEST_F(CachedImageFetcherImageCacheTest, ReconciliationMismatchData) {
  CreateImageCache();
  InitializeImageCache();

  // Inject differing keys so they mismatch, then run reconciliation.
  InjectData("foo", "z", /* needs_transcoding */ false);
  InjectData("bar", "z", /* needs_transcoding */ false);
  InjectMetadata("foo", 10, /* needs_transcoding */ false);
  RunReconciliation();

  // Data should be gone.
  LoadImage("bar", "");
}

TEST_F(CachedImageFetcherImageCacheTest, ReconciliationMismatchMetadata) {
  CreateImageCache();
  InitializeImageCache();

  // Inject differing keys so they mismatch, then run reconciliation.
  InjectData("foo", "z", /* needs_transcoding */ false);
  InjectMetadata("foo", 10, /* needs_transcoding */ false);
  InjectMetadata("bar", 10, /* needs_transcoding */ false);
  RunReconciliation();

  // Metadata should be gone.
  ASSERT_FALSE(IsMetadataPresent("bar"));
}

}  // namespace image_fetcher
