// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/browsing_topics_site_data_storage.h"

#include <functional>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

namespace {

int VersionFromMetaTable(sql::Database& db) {
  // Get version.
  sql::Statement s(
      db.GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
  if (!s.Step())
    return 0;
  return s.ColumnInt(0);
}

}  // namespace

class BrowsingTopicsSiteDataStorageTest : public testing::Test {
 public:
  BrowsingTopicsSiteDataStorageTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  base::FilePath DbPath() const {
    return temp_dir_.GetPath().AppendASCII("TestBrowsingTopicsSiteData.db");
  }

  base::FilePath GetSqlFilePath(base::StringPiece sql_filename) {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_path = file_path.AppendASCII("content/test/data/browsing_topics/");
    file_path = file_path.AppendASCII(sql_filename);
    EXPECT_TRUE(base::PathExists(file_path));
    return file_path;
  }

  size_t CountApiUsagesEntries(sql::Database& db) {
    static const char kCountSQL[] =
        "SELECT COUNT(*) FROM browsing_topics_api_usages";
    sql::Statement s(db.GetUniqueStatement(kCountSQL));
    EXPECT_TRUE(s.Step());
    return s.ColumnInt(0);
  }

  void OpenDatabase() {
    topics_storage_.reset();
    topics_storage_ = std::make_unique<BrowsingTopicsSiteDataStorage>(DbPath());
  }

  void CloseDatabase() { topics_storage_.reset(); }

  BrowsingTopicsSiteDataStorage* topics_storage() {
    return topics_storage_.get();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<BrowsingTopicsSiteDataStorage> topics_storage_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(BrowsingTopicsSiteDataStorageTest,
       DatabaseInitialized_TablesAndIndexesLazilyInitialized) {
  base::HistogramTester histograms;

  OpenDatabase();
  CloseDatabase();

  // An unused BrowsingTopicsSiteDataStorage instance should not create the
  // database.
  EXPECT_FALSE(base::PathExists(DbPath()));

  // DB init UMA should not be recorded.
  histograms.ExpectTotalCount("BrowsingTopics.SiteDataStorage.InitStatus", 0);

  OpenDatabase();
  // Trigger the lazy-initialization.
  topics_storage()->GetBrowsingTopicsApiUsage(
      /*begin_time=*/base::Time(), /*end_time=*/base::Time());
  CloseDatabase();

  EXPECT_TRUE(base::PathExists(DbPath()));

  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));

  // [browsing_topics_api_usages], [meta].
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));

  EXPECT_EQ(1, VersionFromMetaTable(db));

  // [sqlite_autoindex_browsing_topics_api_usages_1], [last_usage_time_idx],
  // and [sqlite_autoindex_meta_1].
  EXPECT_EQ(3u, sql::test::CountSQLIndices(&db));

  // `hashed_context_domain`, `hashed_main_frame_host`, and `last_usage_time`.
  EXPECT_EQ(3u,
            sql::test::CountTableColumns(&db, "browsing_topics_api_usages"));

  EXPECT_EQ(0u, CountApiUsagesEntries(db));

  histograms.ExpectUniqueSample("BrowsingTopics.SiteDataStorage.InitStatus",
                                true, /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsSiteDataStorageTest, LoadFromFile_CurrentVersion_Success) {
  base::HistogramTester histograms;

  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(DbPath(), GetSqlFilePath("v1.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  topics_storage()->GetBrowsingTopicsApiUsage(
      /*begin_time=*/base::Time(), /*end_time=*/base::Time());
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(1, VersionFromMetaTable(db));
  EXPECT_EQ(1u, CountApiUsagesEntries(db));

  histograms.ExpectUniqueSample("BrowsingTopics.SiteDataStorage.InitStatus",
                                true, /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsSiteDataStorageTest, LoadFromFile_VersionTooOld_Failure) {
  base::HistogramTester histograms;

  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      DbPath(), GetSqlFilePath("v0.init_too_old.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  topics_storage()->GetBrowsingTopicsApiUsage(
      /*begin_time=*/base::Time(), /*end_time=*/base::Time());
  CloseDatabase();

  // Expect that the initialization was unsuccessful. The original database was
  // unaffected.
  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(0, VersionFromMetaTable(db));
  EXPECT_EQ(1u, CountApiUsagesEntries(db));

  histograms.ExpectUniqueSample("BrowsingTopics.SiteDataStorage.InitStatus",
                                false, /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsSiteDataStorageTest, LoadFromFile_VersionTooNew_Failure) {
  base::HistogramTester histograms;

  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      DbPath(), GetSqlFilePath("v1.init_too_new.sql")));

  OpenDatabase();
  // Trigger the lazy-initialization.
  topics_storage()->GetBrowsingTopicsApiUsage(
      /*begin_time=*/base::Time(), /*end_time=*/base::Time());
  CloseDatabase();

  // Expect that the initialization was successful. The original database was
  // razed and re-initialized.
  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(1, VersionFromMetaTable(db));
  EXPECT_EQ(0u, CountApiUsagesEntries(db));

  histograms.ExpectUniqueSample("BrowsingTopics.SiteDataStorage.InitStatus",
                                true, /*expected_bucket_count=*/1);
}

TEST_F(BrowsingTopicsSiteDataStorageTest, OnBrowsingTopicsApiUsed_SingleEntry) {
  OpenDatabase();
  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(456)},
      base::Time::Now());
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(1u, CountApiUsagesEntries(db));

  const char kGetAllEntriesSql[] =
      "SELECT hashed_context_domain, hashed_main_frame_host, last_usage_time "
      "FROM "
      "browsing_topics_api_usages";
  sql::Statement s(db.GetUniqueStatement(kGetAllEntriesSql));
  EXPECT_TRUE(s.Step());

  int64_t hashed_context_domain = s.ColumnInt64(0);
  int64_t hashed_main_frame_host = s.ColumnInt64(1);
  base::Time time = s.ColumnTime(2);

  EXPECT_EQ(hashed_context_domain, 456);
  EXPECT_EQ(hashed_main_frame_host, 123);
  EXPECT_EQ(time, base::Time::Now());

  EXPECT_FALSE(s.Step());
}

TEST_F(BrowsingTopicsSiteDataStorageTest,
       OnBrowsingTopicsApiUsed_MultipleEntries) {
  OpenDatabase();
  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(123)},
      base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(1));

  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/
      {browsing_topics::HashedDomain(456), browsing_topics::HashedDomain(789)},
      base::Time::Now());
  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(456),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(789)},
      base::Time::Now());
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(4u, CountApiUsagesEntries(db));

  const char kGetAllEntriesSql[] =
      "SELECT hashed_context_domain, hashed_main_frame_host, last_usage_time "
      "FROM "
      "browsing_topics_api_usages "
      "ORDER BY last_usage_time, hashed_main_frame_host, hashed_context_domain";

  sql::Statement s(db.GetUniqueStatement(kGetAllEntriesSql));

  {
    EXPECT_TRUE(s.Step());

    int64_t hashed_context_domain = s.ColumnInt64(0);
    int64_t hashed_main_frame_host = s.ColumnInt64(1);
    base::Time time = s.ColumnTime(2);

    EXPECT_EQ(hashed_context_domain, 123);
    EXPECT_EQ(hashed_main_frame_host, 123);
    EXPECT_EQ(time, base::Time::Now() - base::Seconds(1));
  }

  {
    EXPECT_TRUE(s.Step());

    int64_t hashed_context_domain = s.ColumnInt64(0);
    int64_t hashed_main_frame_host = s.ColumnInt64(1);
    base::Time time = s.ColumnTime(2);

    EXPECT_EQ(hashed_context_domain, 456);
    EXPECT_EQ(hashed_main_frame_host, 123);
    EXPECT_EQ(time, base::Time::Now());
  }

  {
    EXPECT_TRUE(s.Step());

    int64_t hashed_context_domain = s.ColumnInt64(0);
    int64_t hashed_main_frame_host = s.ColumnInt64(1);
    base::Time time = s.ColumnTime(2);

    EXPECT_EQ(hashed_context_domain, 789u);
    EXPECT_EQ(hashed_main_frame_host, 123);
    EXPECT_EQ(time, base::Time::Now());
  }

  {
    EXPECT_TRUE(s.Step());

    int64_t hashed_context_domain = s.ColumnInt64(0);
    int64_t hashed_main_frame_host = s.ColumnInt64(1);
    base::Time time = s.ColumnTime(2);

    EXPECT_EQ(hashed_context_domain, 789u);
    EXPECT_EQ(hashed_main_frame_host, 456);
    EXPECT_EQ(time, base::Time::Now());
  }

  EXPECT_FALSE(s.Step());
}

TEST_F(BrowsingTopicsSiteDataStorageTest, GetBrowsingTopicsApiUsage) {
  OpenDatabase();

  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(123)},
      base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(1));

  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(456)},
      base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(1));

  browsing_topics::ApiUsageContextQueryResult result =
      topics_storage()->GetBrowsingTopicsApiUsage(
          /*begin_time=*/base::Time::Now() - base::Seconds(2),
          /*end_time=*/base::Time::Now());
  CloseDatabase();

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.api_usage_contexts.size(), 2u);

  EXPECT_EQ(result.api_usage_contexts[0].hashed_main_frame_host,
            browsing_topics::HashedHost(123));
  EXPECT_EQ(result.api_usage_contexts[0].hashed_context_domain,
            browsing_topics::HashedDomain(456));
  EXPECT_EQ(result.api_usage_contexts[0].time,
            base::Time::Now() - base::Seconds(1));

  EXPECT_EQ(result.api_usage_contexts[1].hashed_main_frame_host,
            browsing_topics::HashedHost(123));
  EXPECT_EQ(result.api_usage_contexts[1].hashed_context_domain,
            browsing_topics::HashedDomain(123));
  EXPECT_EQ(result.api_usage_contexts[1].time,
            base::Time::Now() - base::Seconds(2));
}

TEST_F(BrowsingTopicsSiteDataStorageTest,
       GetBrowsingTopicsApiUsage_AutoExpireDataBeforeBeginTime) {
  OpenDatabase();

  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(123)},
      base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(1));

  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(456)},
      base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(1));

  browsing_topics::ApiUsageContextQueryResult result =
      topics_storage()->GetBrowsingTopicsApiUsage(
          /*begin_time=*/base::Time::Now() - base::Seconds(1),
          /*end_time=*/base::Time::Now());
  CloseDatabase();

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.api_usage_contexts.size(), 1u);
  EXPECT_EQ(result.api_usage_contexts[0].hashed_main_frame_host,
            browsing_topics::HashedHost(123));
  EXPECT_EQ(result.api_usage_contexts[0].hashed_context_domain,
            browsing_topics::HashedDomain(456));
  EXPECT_EQ(result.api_usage_contexts[0].time,
            base::Time::Now() - base::Seconds(1));

  // The `GetBrowsingTopicsApiUsage()` should have deleted the first inserted
  // entry.
  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(1u, CountApiUsagesEntries(db));
}

TEST_F(BrowsingTopicsSiteDataStorageTest, ExpireDataBefore) {
  OpenDatabase();

  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(123)},
      base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(1));

  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(456)},
      base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(1));

  topics_storage()->ExpireDataBefore(base::Time::Now() - base::Seconds(1));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(1u, CountApiUsagesEntries(db));

  // The `ExpireDataBefore()` should have deleted the first inserted entry.
  const char kGetAllEntriesSql[] =
      "SELECT hashed_context_domain, hashed_main_frame_host, last_usage_time "
      "FROM "
      "browsing_topics_api_usages";
  sql::Statement s(db.GetUniqueStatement(kGetAllEntriesSql));
  EXPECT_TRUE(s.Step());

  int64_t hashed_context_domain = s.ColumnInt64(0);
  int64_t hashed_main_frame_host = s.ColumnInt64(1);
  base::Time time = s.ColumnTime(2);

  EXPECT_EQ(hashed_context_domain, 456);
  EXPECT_EQ(hashed_main_frame_host, 123);
  EXPECT_EQ(time, base::Time::Now() - base::Seconds(1));

  EXPECT_FALSE(s.Step());
}

TEST_F(BrowsingTopicsSiteDataStorageTest, ClearContextDomain) {
  OpenDatabase();

  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(123)},
      base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(1));

  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(456)},
      base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(1));

  topics_storage()->ClearContextDomain(browsing_topics::HashedDomain(123));
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(1u, CountApiUsagesEntries(db));

  // The `ExpireDataBefore()` should have deleted the first inserted entry.
  const char kGetAllEntriesSql[] =
      "SELECT hashed_context_domain, hashed_main_frame_host, last_usage_time "
      "FROM "
      "browsing_topics_api_usages";
  sql::Statement s(db.GetUniqueStatement(kGetAllEntriesSql));
  EXPECT_TRUE(s.Step());

  int64_t hashed_context_domain = s.ColumnInt64(0);
  int64_t hashed_main_frame_host = s.ColumnInt64(1);
  base::Time time = s.ColumnTime(2);

  EXPECT_EQ(hashed_context_domain, 456);
  EXPECT_EQ(hashed_main_frame_host, 123);
  EXPECT_EQ(time, base::Time::Now() - base::Seconds(1));

  EXPECT_FALSE(s.Step());
}

class BrowsingTopicsSiteDataStorageMaxEntriesToLoadTest
    : public BrowsingTopicsSiteDataStorageTest {
 public:
  BrowsingTopicsSiteDataStorageMaxEntriesToLoadTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kBrowsingTopics,
        {{"max_number_of_api_usage_context_entries_to_load_per_epoch", "1"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BrowsingTopicsSiteDataStorageMaxEntriesToLoadTest, MaxEntriesToLoad) {
  OpenDatabase();

  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(123)},
      base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(1));

  topics_storage()->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domains=*/{browsing_topics::HashedDomain(456)},
      base::Time::Now());

  task_environment_.FastForwardBy(base::Seconds(1));

  browsing_topics::ApiUsageContextQueryResult result =
      topics_storage()->GetBrowsingTopicsApiUsage(
          /*begin_time=*/base::Time::Now() - base::Seconds(2),
          /*end_time=*/base::Time::Now());
  CloseDatabase();

  // Only the latest entry is loaded; the storage should still have two entries.
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.api_usage_contexts.size(), 1u);

  EXPECT_EQ(result.api_usage_contexts[0].hashed_main_frame_host,
            browsing_topics::HashedHost(123));
  EXPECT_EQ(result.api_usage_contexts[0].hashed_context_domain,
            browsing_topics::HashedDomain(456));
  EXPECT_EQ(result.api_usage_contexts[0].time,
            base::Time::Now() - base::Seconds(1));

  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(2u, CountApiUsagesEntries(db));
}

}  // namespace content
