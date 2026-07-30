// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_store.h"

#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/files/file_path.h"
#import "base/files/scoped_temp_dir.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/test/test_future.h"
#import "base/threading/sequence_bound.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Helper: build a non-incognito DownloadRecord with a fixed id and a few
// minimal fields populated so DB serialization round-trips can compare
// against the original.
DownloadRecord MakeRecord(const std::string& id, bool is_incognito = false) {
  DownloadRecord record;
  record.download_id = id;
  record.file_name = id + ".pdf";
  record.original_url = "https://example.com/" + id;
  record.mime_type = "application/pdf";
  record.state = web::DownloadTask::State::kComplete;
  record.created_time = base::Time::Now();
  record.is_incognito = is_incognito;
  return record;
}

}  // namespace

// Exercises DownloadRecordStore directly through `base::SequenceBound` so
// the SEQUENCE_CHECKER and SQLite I/O are verified end-to-end. These tests
// intentionally bypass DownloadRecordServiceImpl — they cover the store's
// public surface that the service drives via AsyncCall. Behavior already
// validated indirectly through DownloadRecordServiceImplTest is re-asserted
// here at the layer where the logic lives.
class DownloadRecordStoreTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    store_ = base::SequenceBound<DownloadRecordStore>(
        db_task_runner_, /*pagination_enabled=*/false);
  }

  void TearDown() override {
    // `base::SequenceBound` posts the store's destruction onto the bound
    // sequence on its own. Drain pending work so the destructor runs
    // before the task environment goes away.
    store_.Reset();
    task_environment_.RunUntilIdle();
    PlatformTest::TearDown();
  }

  // Convenience: posts `Initialize -> LoadHistoricalRecords` (the legacy
  // flag-OFF startup pair) and waits for both to drain.
  void InitializeWithLegacyStartup() {
    store_.AsyncCall(&DownloadRecordStore::InitializeDatabase)
        .WithArgs(temp_dir_.GetPath());
    base::test::TestFuture<void> done;
    store_.AsyncCall(&DownloadRecordStore::LoadHistoricalRecords)
        .Then(done.GetCallback());
    ASSERT_TRUE(done.Wait());
  }

  // Convenience: posts `Initialize -> MarkUnfinishedDownloadsAsFailed` (the
  // pagination-aware flag-ON startup pair).
  void InitializeWithPaginationStartup() {
    store_.AsyncCall(&DownloadRecordStore::InitializeDatabase)
        .WithArgs(temp_dir_.GetPath());
    base::test::TestFuture<void> done;
    store_.AsyncCall(&DownloadRecordStore::MarkUnfinishedDownloadsAsFailed)
        .Then(done.GetCallback());
    ASSERT_TRUE(done.Wait());
  }

  web::WebTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  base::SequenceBound<DownloadRecordStore> store_;
};

// Verifies a fresh-on-disk store comes up healthy through `InitializeDatabase`
// and that the legacy startup loader leaves the cache empty when there is no
// prior data.
TEST_F(DownloadRecordStoreTest, InitializeDatabase_HappyPath_EmptyCache) {
  InitializeWithLegacyStartup();

  base::test::TestFuture<std::vector<DownloadRecord>> future;
  store_.AsyncCall(&DownloadRecordStore::GetAllFromCache)
      .Then(future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

// Non-incognito records must land in BOTH the in-memory cache and the
// underlying SQLite database. We assert both: the cache via
// GetById, and the DB via a re-init round-trip that drops and
// reloads the cache from disk.
TEST_F(DownloadRecordStoreTest, InsertRecord_NonIncognito_PersistsAndCaches) {
  InitializeWithLegacyStartup();

  DownloadRecord record = MakeRecord("non_incognito_id");
  base::test::TestFuture<bool> insert_future;
  store_.AsyncCall(&DownloadRecordStore::InsertRecord)
      .WithArgs(record)
      .Then(insert_future.GetCallback());
  EXPECT_TRUE(insert_future.Get());

  // Cache: present right after insert.
  base::test::TestFuture<std::optional<DownloadRecord>> cached_future;
  store_.AsyncCall(&DownloadRecordStore::GetById)
      .WithArgs("non_incognito_id")
      .Then(cached_future.GetCallback());
  std::optional<DownloadRecord> cached = cached_future.Get();
  ASSERT_TRUE(cached.has_value());
  EXPECT_EQ("non_incognito_id", cached->download_id);
  EXPECT_FALSE(cached->is_incognito);

  // DB: still present after the cache is cleared and reloaded from disk.
  base::test::TestFuture<void> reload_done;
  store_.AsyncCall(&DownloadRecordStore::LoadHistoricalRecords)
      .Then(reload_done.GetCallback());
  ASSERT_TRUE(reload_done.Wait());

  base::test::TestFuture<std::optional<DownloadRecord>> reloaded_future;
  store_.AsyncCall(&DownloadRecordStore::GetById)
      .WithArgs("non_incognito_id")
      .Then(reloaded_future.GetCallback());
  std::optional<DownloadRecord> reloaded = reloaded_future.Get();
  ASSERT_TRUE(reloaded.has_value());
  EXPECT_EQ("non_incognito_id", reloaded->download_id);
}

// Incognito records must never touch the DB; the store should hold them
// only in the in-memory cache and they must disappear when the cache is
// reloaded from disk.
TEST_F(DownloadRecordStoreTest, InsertRecord_Incognito_CachesOnly) {
  InitializeWithLegacyStartup();

  DownloadRecord record = MakeRecord("incognito_id", /*is_incognito=*/true);
  base::test::TestFuture<bool> insert_future;
  store_.AsyncCall(&DownloadRecordStore::InsertRecord)
      .WithArgs(record)
      .Then(insert_future.GetCallback());
  EXPECT_TRUE(insert_future.Get());

  base::test::TestFuture<std::optional<DownloadRecord>> cached_future;
  store_.AsyncCall(&DownloadRecordStore::GetById)
      .WithArgs("incognito_id")
      .Then(cached_future.GetCallback());
  std::optional<DownloadRecord> cached = cached_future.Get();
  ASSERT_TRUE(cached.has_value());
  EXPECT_TRUE(cached->is_incognito);

  // Reload cache from DB — incognito row must NOT come back.
  base::test::TestFuture<void> reload_done;
  store_.AsyncCall(&DownloadRecordStore::LoadHistoricalRecords)
      .Then(reload_done.GetCallback());
  ASSERT_TRUE(reload_done.Wait());

  base::test::TestFuture<std::optional<DownloadRecord>> reloaded_future;
  store_.AsyncCall(&DownloadRecordStore::GetById)
      .WithArgs("incognito_id")
      .Then(reloaded_future.GetCallback());
  EXPECT_FALSE(reloaded_future.Get().has_value());
}

// `created_time` is stamped only at first insert. Any later UpdateRecord
// must preserve the cached `created_time`, regardless of whatever value
// the caller put on the new record.
TEST_F(DownloadRecordStoreTest, UpdateRecord_PreservesCreatedTime) {
  InitializeWithLegacyStartup();

  DownloadRecord original = MakeRecord("update_id");
  base::Time first_created_time = original.created_time;

  base::test::TestFuture<bool> insert_future;
  store_.AsyncCall(&DownloadRecordStore::InsertRecord)
      .WithArgs(original)
      .Then(insert_future.GetCallback());
  EXPECT_TRUE(insert_future.Get());

  // Build an update that deliberately stomps `created_time`.
  DownloadRecord updated = original;
  updated.created_time = first_created_time + base::Hours(1);
  updated.state = web::DownloadTask::State::kFailed;

  base::test::TestFuture<std::optional<DownloadRecord>> update_future;
  store_.AsyncCall(&DownloadRecordStore::UpdateRecord)
      .WithArgs(updated)
      .Then(update_future.GetCallback());
  std::optional<DownloadRecord> merged = update_future.Get();
  ASSERT_TRUE(merged.has_value());
  EXPECT_EQ(first_created_time, merged->created_time);
  EXPECT_EQ(web::DownloadTask::State::kFailed, merged->state);
}

// DeleteRecord must remove the row from both the cache and the database;
// after the cache is reloaded from disk the record must still be gone.
TEST_F(DownloadRecordStoreTest, DeleteRecord_RemovesFromCacheAndDatabase) {
  InitializeWithLegacyStartup();

  DownloadRecord record = MakeRecord("delete_id");
  base::test::TestFuture<bool> insert_future;
  store_.AsyncCall(&DownloadRecordStore::InsertRecord)
      .WithArgs(record)
      .Then(insert_future.GetCallback());
  EXPECT_TRUE(insert_future.Get());

  base::test::TestFuture<bool> delete_future;
  store_.AsyncCall(&DownloadRecordStore::DeleteRecord)
      .WithArgs("delete_id")
      .Then(delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Get());

  base::test::TestFuture<std::optional<DownloadRecord>> cached_future;
  store_.AsyncCall(&DownloadRecordStore::GetById)
      .WithArgs("delete_id")
      .Then(cached_future.GetCallback());
  EXPECT_FALSE(cached_future.Get().has_value());

  // Reload cache from DB — row must NOT come back.
  base::test::TestFuture<void> reload_done;
  store_.AsyncCall(&DownloadRecordStore::LoadHistoricalRecords)
      .Then(reload_done.GetCallback());
  ASSERT_TRUE(reload_done.Wait());

  base::test::TestFuture<std::optional<DownloadRecord>> reloaded_future;
  store_.AsyncCall(&DownloadRecordStore::GetById)
      .WithArgs("delete_id")
      .Then(reloaded_future.GetCallback());
  EXPECT_FALSE(reloaded_future.Get().has_value());
}

// Legacy flag-OFF startup must rehydrate the cache from on-disk rows.
// We seed the DB through one store instance, destroy it, spin up a new
// store on the same temp dir, and assert the legacy startup path makes
// the seeded record visible via the cache.
TEST_F(DownloadRecordStoreTest,
       LoadHistoricalRecords_LegacyStartup_RehydratesCacheFromDisk) {
  // First instance: write one persistent row to disk.
  InitializeWithLegacyStartup();
  DownloadRecord record = MakeRecord("rehydrate_id");
  base::test::TestFuture<bool> insert_future;
  store_.AsyncCall(&DownloadRecordStore::InsertRecord)
      .WithArgs(record)
      .Then(insert_future.GetCallback());
  EXPECT_TRUE(insert_future.Get());

  // Tear down the first store on the bound sequence (SequenceBound::Reset
  // posts destruction onto `db_task_runner_`).
  store_.Reset();
  task_environment_.RunUntilIdle();

  // Second instance: same temp dir, legacy startup must repopulate cache.
  store_ = base::SequenceBound<DownloadRecordStore>(
      db_task_runner_, /*pagination_enabled=*/false);
  InitializeWithLegacyStartup();

  base::test::TestFuture<std::optional<DownloadRecord>> cached_future;
  store_.AsyncCall(&DownloadRecordStore::GetById)
      .WithArgs("rehydrate_id")
      .Then(cached_future.GetCallback());
  std::optional<DownloadRecord> cached = cached_future.Get();
  ASSERT_TRUE(cached.has_value());
  EXPECT_EQ("rehydrate_id", cached->download_id);
}

// Pagination-aware flag-ON startup must NOT populate the in-memory cache
// even when the underlying DB has rows. This is the key invariant that
// keeps service construction O(1) on persisted-row count.
TEST_F(DownloadRecordStoreTest,
       MarkUnfinishedDownloadsAsFailed_PaginationStartup_LeavesCacheEmpty) {
  // First instance: seed the DB via the legacy path so a prior session's
  // rows exist on disk for the second instance to ignore.
  InitializeWithLegacyStartup();
  DownloadRecord record = MakeRecord("seeded_id");
  base::test::TestFuture<bool> insert_future;
  store_.AsyncCall(&DownloadRecordStore::InsertRecord)
      .WithArgs(record)
      .Then(insert_future.GetCallback());
  EXPECT_TRUE(insert_future.Get());

  store_.Reset();
  task_environment_.RunUntilIdle();

  // Second instance: pagination-aware startup runs only the SQL UPDATE
  // and leaves the cache empty.
  store_ = base::SequenceBound<DownloadRecordStore>(
      db_task_runner_, /*pagination_enabled=*/true);
  InitializeWithPaginationStartup();

  base::test::TestFuture<std::vector<DownloadRecord>> future;
  store_.AsyncCall(&DownloadRecordStore::GetAllFromCache)
      .Then(future.GetCallback());

  EXPECT_TRUE(future.Get().empty())
      << "Pagination startup must keep the in-memory cache empty so "
         "construction stays O(1) on persisted-row count.";
}
