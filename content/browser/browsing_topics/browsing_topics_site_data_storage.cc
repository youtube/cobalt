// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/browsing_topics_site_data_storage.h"

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

// Version number of the database.
const int kCurrentVersionNumber = 1;

void RecordInitializationStatus(bool successful) {
  base::UmaHistogramBoolean("BrowsingTopics.SiteDataStorage.InitStatus",
                            successful);
}

}  // namespace

BrowsingTopicsSiteDataStorage::BrowsingTopicsSiteDataStorage(
    const base::FilePath& path_to_database)
    : path_to_database_(path_to_database) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

BrowsingTopicsSiteDataStorage::~BrowsingTopicsSiteDataStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BrowsingTopicsSiteDataStorage::ExpireDataBefore(base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit())
    return;

  static constexpr char kDeleteApiUsageSql[] =
      // clang-format off
      "DELETE FROM browsing_topics_api_usages "
          "WHERE last_usage_time < ?";
  // clang-format on

  sql::Statement delete_api_usage_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteApiUsageSql));
  delete_api_usage_statement.BindTime(0, end_time);

  delete_api_usage_statement.Run();
}

void BrowsingTopicsSiteDataStorage::ClearContextDomain(
    const browsing_topics::HashedDomain& hashed_context_domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit())
    return;

  static constexpr char kDeleteContextDomainSql[] =
      // clang-format off
      "DELETE FROM browsing_topics_api_usages "
          "WHERE hashed_context_domain = ?";
  // clang-format on

  sql::Statement delete_context_domain_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteContextDomainSql));
  delete_context_domain_statement.BindInt64(0, hashed_context_domain.value());

  delete_context_domain_statement.Run();
}

browsing_topics::ApiUsageContextQueryResult
BrowsingTopicsSiteDataStorage::GetBrowsingTopicsApiUsage(base::Time begin_time,
                                                         base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit())
    return {};

  // Expire data before `begin_time`, as they are no longer needed.
  ExpireDataBefore(begin_time);

  static constexpr char kGetApiUsageSql[] =
      // clang-format off
      "SELECT hashed_context_domain,hashed_main_frame_host,last_usage_time "
          "FROM browsing_topics_api_usages "
          "WHERE last_usage_time>=? AND last_usage_time<? "
          "ORDER BY last_usage_time DESC "
          "LIMIT ?";
  // clang-format on

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetApiUsageSql));

  statement.BindTime(0, begin_time);
  statement.BindTime(1, end_time);
  statement.BindInt(
      2,
      blink::features::
          kBrowsingTopicsMaxNumberOfApiUsageContextEntriesToLoadPerEpoch.Get());

  std::vector<browsing_topics::ApiUsageContext> contexts;
  while (statement.Step()) {
    browsing_topics::ApiUsageContext usage_context;
    usage_context.hashed_context_domain =
        browsing_topics::HashedDomain(statement.ColumnInt64(0));
    usage_context.hashed_main_frame_host =
        browsing_topics::HashedHost(statement.ColumnInt64(1));
    usage_context.time = statement.ColumnTime(2);

    contexts.push_back(std::move(usage_context));
  }

  if (!statement.Succeeded())
    return {};

  return browsing_topics::ApiUsageContextQueryResult(std::move(contexts));
}

void BrowsingTopicsSiteDataStorage::OnBrowsingTopicsApiUsed(
    const browsing_topics::HashedHost& hashed_main_frame_host,
    const base::flat_set<browsing_topics::HashedDomain>& hashed_context_domains,
    base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit())
    return;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  for (const browsing_topics::HashedDomain& hashed_context_domain :
       hashed_context_domains) {
    static constexpr char kInsertApiUsageSql[] =
        // clang-format off
        "INSERT OR REPLACE INTO browsing_topics_api_usages "
            "(hashed_context_domain,hashed_main_frame_host,last_usage_time) "
            "VALUES (?,?,?)";
    // clang-format on

    sql::Statement insert_api_usage_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kInsertApiUsageSql));
    insert_api_usage_statement.BindInt64(0, hashed_context_domain.value());
    insert_api_usage_statement.BindInt64(1, hashed_main_frame_host.value());
    insert_api_usage_statement.BindTime(2, time);

    if (!insert_api_usage_statement.Run())
      return;
  }

  transaction.Commit();
}

bool BrowsingTopicsSiteDataStorage::LazyInit() {
  if (db_init_status_ != InitStatus::kUnattempted)
    return db_init_status_ == InitStatus::kSuccess;

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
      .exclusive_locking = true, .page_size = 4096, .cache_size = 32});
  db_->set_histogram_tag("BrowsingTopics");

  // base::Unretained is safe here because this BrowsingTopicsSiteDataStorage
  // owns the sql::Database instance that stores and uses the callback. So,
  // `this` is guaranteed to outlive the callback.
  db_->set_error_callback(
      base::BindRepeating(&BrowsingTopicsSiteDataStorage::DatabaseErrorCallback,
                          base::Unretained(this)));

  if (!db_->Open(path_to_database_)) {
    HandleInitializationFailure();
    return false;
  }

  // TODO(yaoxia): measure metrics for the DB file size to have some idea if it
  // gets too big.

  if (!InitializeTables()) {
    HandleInitializationFailure();
    return false;
  }

  db_init_status_ = InitStatus::kSuccess;
  RecordInitializationStatus(true);
  return true;
}

bool BrowsingTopicsSiteDataStorage::InitializeTables() {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  if (!meta_table_.Init(db_.get(), kCurrentVersionNumber,
                        kCurrentVersionNumber)) {
    return false;
  }

  if (!CreateSchema())
    return false;

  // This is the first code version. No database version is expected to be
  // smaller. Fail when this happens.
  if (meta_table_.GetVersionNumber() < kCurrentVersionNumber)
    return false;

  if (!transaction.Commit())
    return false;

  // This is possible with code reverts. The DB will never work until Chrome
  // is re-upgraded. Assume the user will continue using this Chrome version
  // and raze the DB to get the feature working.
  if (meta_table_.GetVersionNumber() > kCurrentVersionNumber) {
    db_->Raze();
    meta_table_.Reset();
    return InitializeTables();
  }

  return true;
}

bool BrowsingTopicsSiteDataStorage::CreateSchema() {
  static constexpr char kBrowsingTopicsApiUsagesTableSql[] =
      // clang-format off
      "CREATE TABLE IF NOT EXISTS browsing_topics_api_usages("
          "hashed_context_domain INTEGER NOT NULL,"
          "hashed_main_frame_host INTEGER NOT NULL,"
          "last_usage_time INTEGER NOT NULL,"
          "PRIMARY KEY (hashed_context_domain,hashed_main_frame_host))";
  // clang-format on
  if (!db_->Execute(kBrowsingTopicsApiUsagesTableSql))
    return false;

  static constexpr char kLastUsageTimeIndexSql[] =
      // clang-format off
      "CREATE INDEX IF NOT EXISTS last_usage_time_idx "
          "ON browsing_topics_api_usages(last_usage_time)";
  // clang-format on
  if (!db_->Execute(kLastUsageTimeIndexSql))
    return false;

  return true;
}

void BrowsingTopicsSiteDataStorage::HandleInitializationFailure() {
  db_.reset();
  db_init_status_ = InitStatus::kFailure;
  RecordInitializationStatus(false);
}

void BrowsingTopicsSiteDataStorage::DatabaseErrorCallback(
    int extended_error,
    sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Attempt to recover a corrupt database.
  if (sql::Recovery::ShouldRecover(extended_error)) {
    // Prevent reentrant calls.
    db_->reset_error_callback();

    // After this call, the |db_| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabaseWithMetaVersion(db_.get(), path_to_database_);

    // The DLOG(FATAL) below is intended to draw immediate attention to errors
    // in newly-written code. Database corruption is generally a result of OS or
    // hardware issues, not coding errors at the client level, so displaying the
    // error would probably lead to confusion. The ignored call signals the
    // test-expectation framework that the error was handled.
    std::ignore = sql::Database::IsExpectedSqliteError(extended_error);
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db_->GetErrorMessage();

  // Consider the database closed if we did not attempt to recover so we did not
  // produce further errors.
  db_init_status_ = InitStatus::kFailure;
}

}  // namespace content
