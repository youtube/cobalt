// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_store.h"

#import <optional>
#import <set>
#import <string>
#import <utility>
#import <vector>

#import "base/files/file_path.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/test/test_future.h"
#import "base/threading/sequence_bound.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_query.h"
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

// `created_time` is stamped only at first insert. Any later `UpdateRecord`
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

// Fixture for the flag-ON cache-merge readers.
class DownloadRecordStorePaginationMergeTest : public DownloadRecordStoreTest {
 protected:
  void SetUp() override {
    DownloadRecordStoreTest::SetUp();
    store_.Reset();
    task_environment_.RunUntilIdle();
    store_ = base::SequenceBound<DownloadRecordStore>(
        db_task_runner_, /*pagination_enabled=*/true);
    InitializeWithPaginationStartup();
  }

  bool Insert(const DownloadRecord& record) {
    base::test::TestFuture<bool> future;
    store_.AsyncCall(&DownloadRecordStore::InsertRecord)
        .WithArgs(record)
        .Then(future.GetCallback());
    return future.Get();
  }

  std::vector<DownloadRecord> Page(const DownloadRecordQuery& query) {
    base::test::TestFuture<std::vector<DownloadRecord>> future;
    store_.AsyncCall(&DownloadRecordStore::GetDownloadsPage)
        .WithArgs(query)
        .Then(future.GetCallback());
    return future.Get();
  }

  size_t Count(const DownloadRecordQuery& query) {
    base::test::TestFuture<size_t> future;
    store_.AsyncCall(&DownloadRecordStore::GetDownloadsCount)
        .WithArgs(query)
        .Then(future.GetCallback());
    return future.Get();
  }
};

// Cache copy wins over the stale DB row's volatile fields.
TEST_F(DownloadRecordStorePaginationMergeTest,
       GetDownloadsPage_ActiveCacheOverridesDbVolatileFields) {
  DownloadRecord record = MakeRecord("id_active");
  record.state = web::DownloadTask::State::kInProgress;
  record.progress_percent = 10;
  ASSERT_TRUE(Insert(record));

  // `progress_percent`-only diff stays cache-only (no DB write).
  DownloadRecord progressed = record;
  progressed.progress_percent = 87;
  base::test::TestFuture<std::optional<DownloadRecord>> update_future;
  store_.AsyncCall(&DownloadRecordStore::UpdateRecord)
      .WithArgs(progressed)
      .Then(update_future.GetCallback());
  ASSERT_TRUE(update_future.Get().has_value());

  DownloadRecordQuery query;
  std::vector<DownloadRecord> page = Page(query);
  ASSERT_EQ(1u, page.size());
  EXPECT_EQ("id_active", page[0].download_id);
  EXPECT_EQ(87, page[0].progress_percent);
}

// Incognito rows merge into the page in `(created_time DESC, id DESC)` order.
TEST_F(DownloadRecordStorePaginationMergeTest,
       GetDownloadsPage_IncognitoMergedInChronologicalOrder) {
  const base::Time base_time = base::Time::Now();

  DownloadRecord persisted = MakeRecord("id_persisted");
  persisted.created_time = base_time;
  ASSERT_TRUE(Insert(persisted));

  DownloadRecord incog = MakeRecord("id_incognito", /*is_incognito=*/true);
  incog.created_time = base_time + base::Seconds(10);
  ASSERT_TRUE(Insert(incog));

  DownloadRecordQuery query;
  std::vector<DownloadRecord> page = Page(query);
  ASSERT_EQ(2u, page.size());
  EXPECT_EQ("id_incognito", page[0].download_id);
  EXPECT_TRUE(page[0].is_incognito);
  EXPECT_EQ("id_persisted", page[1].download_id);
  EXPECT_FALSE(page[1].is_incognito);
}

// Count includes incognito rows.
TEST_F(DownloadRecordStorePaginationMergeTest,
       GetDownloadsCount_IncludesIncognitoRecords) {
  ASSERT_TRUE(Insert(MakeRecord("p1")));
  ASSERT_TRUE(Insert(MakeRecord("p2")));
  ASSERT_TRUE(Insert(MakeRecord("ig1", /*is_incognito=*/true)));
  ASSERT_TRUE(Insert(MakeRecord("ig2", /*is_incognito=*/true)));

  DownloadRecordQuery query;
  EXPECT_EQ(4u, Count(query));
}

// `filter_type` must apply to incognito rows, not just the DB stream.
TEST_F(DownloadRecordStorePaginationMergeTest,
       GetDownloadsPage_FilterAppliesToIncognito) {
  DownloadRecord pdf_persisted = MakeRecord("pdf_persisted");
  pdf_persisted.mime_type = "application/pdf";
  ASSERT_TRUE(Insert(pdf_persisted));

  DownloadRecord image_incog = MakeRecord("image_incog", /*is_incognito=*/true);
  image_incog.mime_type = "image/png";
  ASSERT_TRUE(Insert(image_incog));

  DownloadRecordQuery query;
  query.filter_type = DownloadFilterType::kPDF;

  std::vector<DownloadRecord> page = Page(query);
  ASSERT_EQ(1u, page.size());
  EXPECT_EQ("pdf_persisted", page[0].download_id);
  EXPECT_EQ(1u, Count(query));
}

// `name_query` matches incognito rows case- and accent-insensitively.
TEST_F(DownloadRecordStorePaginationMergeTest,
       GetDownloadsPage_NameQueryMatchesIncognitoCaseAndAccentInsensitively) {
  DownloadRecord incog = MakeRecord("id_acc", /*is_incognito=*/true);
  incog.file_name = "Résumé-2026.pdf";
  ASSERT_TRUE(Insert(incog));

  DownloadRecordQuery query;
  query.name_query = "resume";

  std::vector<DownloadRecord> page = Page(query);
  ASSERT_EQ(1u, page.size());
  EXPECT_EQ("id_acc", page[0].download_id);
  EXPECT_EQ(1u, Count(query));
}

// Non-matching incognito rows are excluded.
TEST_F(DownloadRecordStorePaginationMergeTest,
       GetDownloadsPage_NameQueryExcludesNonMatchingIncognito) {
  DownloadRecord incog = MakeRecord("id_other", /*is_incognito=*/true);
  incog.file_name = "vacation.jpg";
  ASSERT_TRUE(Insert(incog));

  DownloadRecord persisted = MakeRecord("id_invoice");
  persisted.file_name = "invoice.pdf";
  ASSERT_TRUE(Insert(persisted));

  DownloadRecordQuery query;
  query.name_query = "invoice";

  std::vector<DownloadRecord> page = Page(query);
  ASSERT_EQ(1u, page.size());
  EXPECT_EQ("id_invoice", page[0].download_id);
  EXPECT_EQ(1u, Count(query));
}

// Cursor predicate applies to incognito rows too.
TEST_F(DownloadRecordStorePaginationMergeTest,
       GetDownloadsPage_CursorPredicateAppliesToIncognito) {
  const base::Time cursor_time = base::Time::Now();

  DownloadRecord newer = MakeRecord("ig_newer", /*is_incognito=*/true);
  newer.created_time = cursor_time + base::Seconds(5);
  ASSERT_TRUE(Insert(newer));

  DownloadRecord older = MakeRecord("ig_older", /*is_incognito=*/true);
  older.created_time = cursor_time - base::Seconds(5);
  ASSERT_TRUE(Insert(older));

  DownloadRecordQuery query;
  query.cursor_created_time = cursor_time;
  query.cursor_download_id = std::string("zzz");

  std::vector<DownloadRecord> page = Page(query);
  ASSERT_EQ(1u, page.size());
  EXPECT_EQ("ig_older", page[0].download_id);
}

// Count ignores cursor.
TEST_F(DownloadRecordStorePaginationMergeTest,
       GetDownloadsCount_IsCursorAgnostic) {
  ASSERT_TRUE(Insert(MakeRecord("p1")));
  ASSERT_TRUE(Insert(MakeRecord("p2")));
  ASSERT_TRUE(Insert(MakeRecord("ig1", /*is_incognito=*/true)));

  DownloadRecordQuery query;
  query.cursor_created_time = base::Time::Now() + base::Hours(24);
  query.cursor_download_id = std::string("any");

  EXPECT_EQ(3u, Count(query));
}

// Cache rows share PKs with DB rows — count must not double-count.
TEST_F(DownloadRecordStorePaginationMergeTest,
       GetDownloadsCount_DoesNotDoubleCountActiveCache) {
  DownloadRecord record = MakeRecord("dup_id");
  record.state = web::DownloadTask::State::kInProgress;
  ASSERT_TRUE(Insert(record));  // DB + `active_records_cache_`.

  // Cache-only volatile update so the row exists in both layers.
  DownloadRecord progressed = record;
  progressed.progress_percent = 50;
  base::test::TestFuture<std::optional<DownloadRecord>> update_future;
  store_.AsyncCall(&DownloadRecordStore::UpdateRecord)
      .WithArgs(progressed)
      .Then(update_future.GetCallback());
  ASSERT_TRUE(update_future.Get().has_value());

  DownloadRecordQuery query;
  EXPECT_EQ(1u, Count(query));
}

// Persisted rows squeezed off page 1 by interleaved incognito rows must
// resurface on page 2 with no gap or duplicate.
TEST_F(DownloadRecordStorePaginationMergeTest,
       GetDownloadsPage_MultiPageContinuationUnderSqueeze) {
  const base::Time base_time = base::Time::Now();

  const size_t kPersistedCount =
      static_cast<size_t>(kDownloadRecordsPageSize) + 5;
  std::vector<std::string> expected_ids_in_order;
  expected_ids_in_order.reserve(kPersistedCount + 5);
  for (size_t i = 0; i < kPersistedCount; ++i) {
    DownloadRecord record = MakeRecord("p_" + base::NumberToString(i));
    record.created_time =
        base_time - base::Seconds(static_cast<int64_t>(i * 2));
    ASSERT_TRUE(Insert(record));
  }

  // Odd-second offsets interleave between the persisted rows.
  for (size_t i = 0; i < 5; ++i) {
    DownloadRecord record =
        MakeRecord("ig_" + base::NumberToString(i), /*is_incognito=*/true);
    record.created_time =
        base_time - base::Seconds(static_cast<int64_t>(i * 2 + 1));
    ASSERT_TRUE(Insert(record));
  }

  // p_0, ig_0, p_1, ig_1, ..., p_4, ig_4, p_5, p_6, ...
  for (size_t i = 0; i < 5; ++i) {
    expected_ids_in_order.push_back("p_" + base::NumberToString(i));
    expected_ids_in_order.push_back("ig_" + base::NumberToString(i));
  }
  for (size_t i = 5; i < kPersistedCount; ++i) {
    expected_ids_in_order.push_back("p_" + base::NumberToString(i));
  }
  ASSERT_EQ(kPersistedCount + 5, expected_ids_in_order.size());

  DownloadRecordQuery first_page_query;
  std::vector<DownloadRecord> page1 = Page(first_page_query);
  ASSERT_EQ(static_cast<size_t>(kDownloadRecordsPageSize), page1.size());
  for (size_t i = 0; i < page1.size(); ++i) {
    EXPECT_EQ(expected_ids_in_order[i], page1[i].download_id)
        << "page1 i=" << i;
  }

  DownloadRecordQuery second_page_query;
  second_page_query.cursor_created_time = page1.back().created_time;
  second_page_query.cursor_download_id = page1.back().download_id;
  std::vector<DownloadRecord> page2 = Page(second_page_query);
  ASSERT_EQ(expected_ids_in_order.size() - page1.size(), page2.size());
  for (size_t i = 0; i < page2.size(); ++i) {
    EXPECT_EQ(expected_ids_in_order[page1.size() + i], page2[i].download_id)
        << "page2 i=" << i;
  }

  std::set<std::string> seen;
  for (const DownloadRecord& record : page1) {
    seen.insert(record.download_id);
  }
  for (const DownloadRecord& record : page2) {
    EXPECT_TRUE(seen.insert(record.download_id).second)
        << "duplicate across pages: " << record.download_id;
  }

  EXPECT_EQ(expected_ids_in_order.size(), Count(DownloadRecordQuery{}));
}
