// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_database.h"

#include <inttypes.h>

#include <algorithm>
#include <climits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_database_migrations.h"
#include "components/services/storage/shared_storage/shared_storage_options.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

// Because each entry is a key-value pair, and both keys and values are
// std::u16strings and bounded by `max_string_length_`, the total bytes used per
// entry is at most 2 * 2 * `max_string_length_`.
const int kSharedStorageEntryTotalBytesMultiplier = 4;

// Version number of the database.
//
// Version 1 - https://crrev.com/c/3112567
//              * initial commit
//             https://crrev.com/c/3491742
//              * add `budget_mapping` table
// Version 2 - https://crrev.com/c/4029459
//              * add `last_used_time` to `values_mapping`
//              * rename `last_used_time` in `per_origin_mapping` to
//                `creation_time`
// Version 3 - https://crrev.com/c/4463360
//              * store `key` and `value` as BLOB instead of TEXT in order to
//                prevent roundtrip conversion to UTF-8 and back, which is
//                lossy if the original UTF-16 string contains unpaired
//                surrogates
// Version 4 - https://crrev.com/c/4879582
//              * rename `context_origin` column in `budget_mapping` to
//                `context_site`, converting existing data in this column from
//                origins to sites

const int SharedStorageDatabase::kCurrentVersionNumber = 4;

// Earliest version which can use a `kCurrentVersionNumber` database
// without failing.
const int SharedStorageDatabase::kCompatibleVersionNumber = 4;

// Latest version of the database that cannot be upgraded to
// `kCurrentVersionNumber` without razing the database.
const int SharedStorageDatabase::kDeprecatedVersionNumber = 0;

namespace {

std::string SerializeOrigin(const url::Origin& origin) {
  DCHECK(!origin.opaque());
  DCHECK_NE(url::kFileScheme, origin.scheme());
  return origin.Serialize();
}

std::string SerializeSite(const net::SchemefulSite& site) {
  DCHECK(!site.opaque());
  DCHECK_NE(url::kFileScheme, site.GetURL().scheme());
  return site.Serialize();
}

[[nodiscard]] bool InitSchema(sql::Database& db, sql::MetaTable& meta_table) {
  static constexpr char kValuesMappingSql[] =
      "CREATE TABLE IF NOT EXISTS values_mapping("
      "context_origin TEXT NOT NULL,"
      "key BLOB NOT NULL,"
      "value BLOB NOT NULL,"
      "last_used_time INTEGER NOT NULL,"
      "PRIMARY KEY(context_origin,key)) WITHOUT ROWID";
  if (!db.Execute(kValuesMappingSql))
    return false;

  // Note that `length` tracks the total number of entries in `values_mapping`
  // for `context_origin`, including any expired by not yet purged entries. The
  // `Length()` method below returns the count of only the unexpired entries.
  static constexpr char kPerOriginMappingSql[] =
      "CREATE TABLE IF NOT EXISTS per_origin_mapping("
      "context_origin TEXT NOT NULL PRIMARY KEY,"
      "creation_time INTEGER NOT NULL,"
      "length INTEGER NOT NULL) WITHOUT ROWID";
  if (!db.Execute(kPerOriginMappingSql))
    return false;

  static constexpr char kBudgetMappingSql[] =
      "CREATE TABLE IF NOT EXISTS budget_mapping("
      "id INTEGER NOT NULL PRIMARY KEY,"
      "context_site TEXT NOT NULL,"
      "time_stamp INTEGER NOT NULL,"
      "bits_debit REAL NOT NULL)";
  if (!db.Execute(kBudgetMappingSql))
    return false;

  if (meta_table.GetVersionNumber() >= 4) {
    static constexpr char kSiteTimeIndexSql[] =
        "CREATE INDEX IF NOT EXISTS budget_mapping_site_time_stamp_idx "
        "ON budget_mapping(context_site,time_stamp)";
    if (!db.Execute(kSiteTimeIndexSql)) {
      return false;
    }
  }

  if (meta_table.GetVersionNumber() >= 2) {
    static constexpr char kValuesLastUsedTimeIndexSql[] =
        "CREATE INDEX IF NOT EXISTS values_mapping_last_used_time_idx "
        "ON values_mapping(last_used_time)";
    if (!db.Execute(kValuesLastUsedTimeIndexSql))
      return false;

    static constexpr char kCreationTimeIndexSql[] =
        "CREATE INDEX IF NOT EXISTS per_origin_mapping_creation_time_idx "
        "ON per_origin_mapping(creation_time)";
    if (!db.Execute(kCreationTimeIndexSql))
      return false;
  }

  return true;
}

}  // namespace

SharedStorageDatabase::GetResult::GetResult() = default;

SharedStorageDatabase::GetResult::GetResult(GetResult&&) = default;

SharedStorageDatabase::GetResult::GetResult(OperationResult result)
    : result(result) {}

SharedStorageDatabase::GetResult::GetResult(std::u16string data,
                                            base::Time last_used_time,
                                            OperationResult result)
    : data(data), last_used_time(last_used_time), result(result) {}

SharedStorageDatabase::GetResult::~GetResult() = default;

SharedStorageDatabase::GetResult& SharedStorageDatabase::GetResult::operator=(
    GetResult&&) = default;

SharedStorageDatabase::BudgetResult::BudgetResult(BudgetResult&&) = default;

SharedStorageDatabase::BudgetResult::BudgetResult(double bits,
                                                  OperationResult result)
    : bits(bits), result(result) {}

SharedStorageDatabase::BudgetResult::~BudgetResult() = default;

SharedStorageDatabase::BudgetResult&
SharedStorageDatabase::BudgetResult::operator=(BudgetResult&&) = default;

SharedStorageDatabase::TimeResult::TimeResult() = default;

SharedStorageDatabase::TimeResult::TimeResult(TimeResult&&) = default;

SharedStorageDatabase::TimeResult::TimeResult(OperationResult result)
    : result(result) {}

SharedStorageDatabase::TimeResult::~TimeResult() = default;

SharedStorageDatabase::TimeResult& SharedStorageDatabase::TimeResult::operator=(
    TimeResult&&) = default;

SharedStorageDatabase::MetadataResult::MetadataResult() = default;

SharedStorageDatabase::MetadataResult::MetadataResult(MetadataResult&&) =
    default;

SharedStorageDatabase::MetadataResult::~MetadataResult() = default;

SharedStorageDatabase::MetadataResult&
SharedStorageDatabase::MetadataResult::operator=(MetadataResult&&) = default;

SharedStorageDatabase::EntriesResult::EntriesResult() = default;

SharedStorageDatabase::EntriesResult::EntriesResult(EntriesResult&&) = default;

SharedStorageDatabase::EntriesResult::~EntriesResult() = default;

SharedStorageDatabase::EntriesResult&
SharedStorageDatabase::EntriesResult::operator=(EntriesResult&&) = default;

SharedStorageDatabase::SharedStorageDatabase(
    base::FilePath db_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<SharedStorageDatabaseOptions> options)
    : db_({// Run the database in exclusive mode. Nobody else should be
           // accessing the database while we're running, and this will give
           // somewhat improved perf.
           .exclusive_locking = true,
           // We DCHECK that the page size is valid in the constructor for
           // `SharedStorageOptions`.
           .page_size = options->max_page_size,
           .cache_size = options->max_cache_size}),
      db_path_(std::move(db_path)),
      special_storage_policy_(std::move(special_storage_policy)),
      // We DCHECK that these `options` fields are all positive in the
      // constructor for `SharedStorageOptions`.
      max_entries_per_origin_(int64_t{options->max_entries_per_origin}),
      max_string_length_(static_cast<size_t>(options->max_string_length)),
      max_init_tries_(static_cast<size_t>(options->max_init_tries)),
      max_iterator_batch_size_(
          static_cast<size_t>(options->max_iterator_batch_size)),
      bit_budget_(static_cast<double>(options->bit_budget)),
      budget_interval_(options->budget_interval),
      staleness_threshold_(options->staleness_threshold),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK(!is_filebacked() || db_path_.IsAbsolute());
  db_file_status_ = is_filebacked() ? DBFileStatus::kNotChecked
                                    : DBFileStatus::kNoPreexistingFile;
}

SharedStorageDatabase::~SharedStorageDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool SharedStorageDatabase::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.is_open() && !db_.RazeAndPoison()) {
    return false;
  }

  // The file already doesn't exist.
  if (!is_filebacked())
    return true;

  return sql::Database::Delete(db_path_);
}

void SharedStorageDatabase::TrimMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_.TrimMemory();
}

SharedStorageDatabase::GetResult SharedStorageDatabase::Get(
    url::Origin context_origin,
    std::u16string key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(key.size(), max_string_length_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return `OperationResult::kInitFailure` if the database doesn't
    // exist, but only if it pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return GetResult(OperationResult::kNotFound);
    return GetResult(OperationResult::kInitFailure);
  }

  // In theory, there ought to be at most one entry found. But we make no
  // assumption about the state of the disk. In the rare case that multiple
  // entries are found, we return only the value from the first entry found.
  static constexpr char kSelectSql[] =
      "SELECT value,last_used_time FROM values_mapping "
      "WHERE context_origin=? AND key=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  std::string origin_str(SerializeOrigin(context_origin));
  statement.BindString(0, origin_str);
  statement.BindBlob(1, key);

  if (statement.Step()) {
    base::Time last_used_time = statement.ColumnTime(1);
    OperationResult op_result =
        (last_used_time >= clock_->Now() - staleness_threshold_)
            ? OperationResult::kSuccess
            : OperationResult::kExpired;
    std::u16string value;
    if (!statement.ColumnBlobAsString16(0, &value)) {
      return GetResult();
    }
    return GetResult(value, last_used_time, op_result);
  }

  if (!statement.Succeeded())
    return GetResult();

  return GetResult(OperationResult::kNotFound);
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Set(
    url::Origin context_origin,
    std::u16string key,
    std::u16string value,
    SetBehavior behavior) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!key.empty());
  DCHECK_LE(key.size(), max_string_length_);
  DCHECK_LE(value.size(), max_string_length_);

  if (LazyInit(DBCreationPolicy::kCreateIfAbsent) != InitStatus::kSuccess)
    return OperationResult::kInitFailure;

  GetResult get_result = Get(context_origin, key);
  if (get_result.result != OperationResult::kSuccess &&
      get_result.result != OperationResult::kNotFound &&
      get_result.result != OperationResult::kExpired) {
    return OperationResult::kSqlError;
  }

  std::string origin_str(SerializeOrigin(context_origin));
  if (get_result.result == OperationResult::kSuccess &&
      behavior == SharedStorageDatabase::SetBehavior::kIgnoreIfPresent) {
    // We re-insert the old key-value pair with an updated `last_used_time`.
    if (!UpdateValuesMapping(origin_str, key, get_result.data,
                             /*key_exists=*/true)) {
      return OperationResult::kSqlError;
    }
    return OperationResult::kIgnored;
  }
  if (get_result.result == OperationResult::kNotFound &&
      !HasCapacity(origin_str)) {
    return OperationResult::kNoCapacity;
  }

  bool key_exists = get_result.result != OperationResult::kNotFound;
  if (!UpdateValuesMapping(origin_str, key, value, key_exists)) {
    return OperationResult::kSqlError;
  }

  return OperationResult::kSet;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Append(
    url::Origin context_origin,
    std::u16string key,
    std::u16string tail_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!key.empty());
  DCHECK_LE(key.size(), max_string_length_);
  DCHECK_LE(tail_value.size(), max_string_length_);

  if (LazyInit(DBCreationPolicy::kCreateIfAbsent) != InitStatus::kSuccess)
    return OperationResult::kInitFailure;

  GetResult get_result = Get(context_origin, key);
  if (get_result.result != OperationResult::kSuccess &&
      get_result.result != OperationResult::kNotFound &&
      get_result.result != OperationResult::kExpired) {
    return OperationResult::kSqlError;
  }

  std::u16string new_value;
  std::string origin_str(SerializeOrigin(context_origin));

  if (get_result.result == OperationResult::kSuccess) {
    new_value = std::move(get_result.data);
    new_value.append(tail_value);

    if (new_value.size() > max_string_length_)
      return OperationResult::kInvalidAppend;
  } else if (get_result.result == OperationResult::kExpired) {
    new_value = std::move(tail_value);
  } else {
    new_value = std::move(tail_value);

    if (!HasCapacity(origin_str))
      return OperationResult::kNoCapacity;
  }

  bool key_exists = get_result.result != OperationResult::kNotFound;
  if (!UpdateValuesMapping(origin_str, key, new_value, key_exists)) {
    return OperationResult::kSqlError;
  }

  return OperationResult::kSet;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Delete(
    url::Origin context_origin,
    std::u16string key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(key.size(), max_string_length_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  std::string origin_str(SerializeOrigin(context_origin));
  if (!HasEntryFor(origin_str, key))
    return OperationResult::kSuccess;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  static constexpr char kDeleteSql[] =
      "DELETE FROM values_mapping "
      "WHERE context_origin=? AND key=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, origin_str);
  statement.BindBlob(1, key);

  if (!statement.Run())
    return OperationResult::kSqlError;

  if (!UpdateLength(origin_str, /*delta=*/-1))
    return OperationResult::kSqlError;

  if (!transaction.Commit())
    return OperationResult::kSqlError;
  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Clear(
    url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  if (!Purge(SerializeOrigin(context_origin))) {
    return OperationResult::kSqlError;
  }
  return OperationResult::kSuccess;
}

int64_t SharedStorageDatabase::Length(url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return -1 (to signifiy an error) if the database doesn't exist,
    // but only if it pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return 0L;
    else
      return -1;
  }

  return NumEntriesManualCount(SerializeOrigin(context_origin));
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Keys(
    const url::Origin& context_origin,
    mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
        pending_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<blink::mojom::SharedStorageEntriesListener> keys_listener(
      std::move(pending_listener));

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted) {
      keys_listener->DidReadEntries(
          /*success=*/true,
          /*error_message=*/"", /*entries=*/{}, /*has_more_entries=*/false,
          /*total_queued_to_send=*/0);
      return OperationResult::kSuccess;
    } else {
      keys_listener->DidReadEntries(
          /*success=*/false, "SQL database had initialization failure.",
          /*entries=*/{}, /*has_more_entries=*/false,
          /*total_queued_to_send=*/0);
      return OperationResult::kInitFailure;
    }
  }

  std::string origin_str(SerializeOrigin(context_origin));
  int64_t key_count = NumEntriesManualCount(origin_str);

  if (key_count == -1) {
    keys_listener->DidReadEntries(
        /*success=*/false, "SQL database could not retrieve key count.",
        /*entries=*/{}, /*has_more_entries=*/false, /*total_queued_to_send=*/0);
    return OperationResult::kSqlError;
  }

  if (key_count > INT_MAX) {
    keys_listener->DidReadEntries(
        /*success=*/false, "Unexpectedly found more than INT_MAX keys.",
        /*entries=*/{}, /*has_more_entries=*/false, /*total_queued_to_send=*/0);
    return OperationResult::kTooManyFound;
  }

  if (!key_count) {
    keys_listener->DidReadEntries(
        /*success=*/true,
        /*error_message=*/"", /*entries=*/{}, /*has_more_entries=*/false,
        /*total_queued_to_send=*/0);
    return OperationResult::kSuccess;
  }

  static constexpr char kSelectSql[] =
      "SELECT key FROM values_mapping "
      "WHERE context_origin=? AND last_used_time>=? "
      "ORDER BY key";

  sql::Statement select_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  select_statement.BindString(0, origin_str);
  select_statement.BindTime(1, clock_->Now() - staleness_threshold_);

  bool has_more_entries = true;
  absl::optional<std::u16string> saved_first_key_for_next_batch;

  while (has_more_entries) {
    has_more_entries = false;
    std::vector<blink::mojom::SharedStorageKeyAndOrValuePtr> keys;

    if (saved_first_key_for_next_batch) {
      keys.push_back(blink::mojom::SharedStorageKeyAndOrValue::New(
          saved_first_key_for_next_batch.value(), u""));
      saved_first_key_for_next_batch.reset();
    }

    bool blob_retrieval_error = false;
    while (select_statement.Step()) {
      std::u16string key;
      if (!select_statement.ColumnBlobAsString16(0, &key)) {
        blob_retrieval_error = true;
        break;
      }
      if (keys.size() < max_iterator_batch_size_) {
        keys.push_back(
            blink::mojom::SharedStorageKeyAndOrValue::New(std::move(key), u""));
      } else {
        // Cache the current key to use as the start of the next batch, as we're
        // already passing through this step and the next iteration of
        // `statement.Step()`, if there is one, during the next iteration of the
        // outer while loop, will give us the subsequent key.
        saved_first_key_for_next_batch = std::move(key);
        has_more_entries = true;
        break;
      }
    }

    if (!select_statement.Succeeded() || blob_retrieval_error) {
      keys_listener->DidReadEntries(
          /*success=*/false,
          "SQL database encountered an error while retrieving keys.",
          /*entries=*/{}, /*has_more_entries=*/false,
          static_cast<int>(key_count));
      return OperationResult::kSqlError;
    }

    keys_listener->DidReadEntries(/*success=*/true, /*error_message=*/"",
                                  std::move(keys), has_more_entries,
                                  static_cast<int>(key_count));
  }

  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Entries(
    const url::Origin& context_origin,
    mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
        pending_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<blink::mojom::SharedStorageEntriesListener> entries_listener(
      std::move(pending_listener));

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted) {
      entries_listener->DidReadEntries(
          /*success=*/true,
          /*error_message=*/"", /*entries=*/{}, /*has_more_entries=*/false,
          /*total_queued_to_send=*/0);
      return OperationResult::kSuccess;
    } else {
      entries_listener->DidReadEntries(
          /*success=*/false, "SQL database had initialization failure.",
          /*entries=*/{}, /*has_more_entries=*/false,
          /*total_queued_to_send=*/0);
      return OperationResult::kInitFailure;
    }
  }

  std::string origin_str(SerializeOrigin(context_origin));
  int64_t entry_count = NumEntriesManualCount(origin_str);

  if (entry_count == -1) {
    entries_listener->DidReadEntries(
        /*success=*/false, "SQL database could not retrieve entry count.",
        /*entries=*/{}, /*has_more_entries=*/false, /*total_queued_to_send=*/0);
    return OperationResult::kSqlError;
  }

  if (entry_count > INT_MAX) {
    entries_listener->DidReadEntries(
        /*success=*/false, "Unexpectedly found more than INT_MAX entries.",
        /*entries=*/{}, /*has_more_entries=*/false, /*total_queued_to_send=*/0);
    return OperationResult::kTooManyFound;
  }

  if (!entry_count) {
    entries_listener->DidReadEntries(
        /*success=*/true,
        /*error_message=*/"", /*entries=*/{}, /*has_more_entries=*/false,
        /*total_queued_to_send=*/0);
    return OperationResult::kSuccess;
  }

  static constexpr char kSelectSql[] =
      "SELECT key,value FROM values_mapping "
      "WHERE context_origin=? AND last_used_time>=? "
      "ORDER BY key";

  sql::Statement select_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  select_statement.BindString(0, origin_str);
  select_statement.BindTime(1, clock_->Now() - staleness_threshold_);

  bool has_more_entries = true;
  absl::optional<std::u16string> saved_first_key_for_next_batch;
  absl::optional<std::u16string> saved_first_value_for_next_batch;

  while (has_more_entries) {
    has_more_entries = false;
    std::vector<blink::mojom::SharedStorageKeyAndOrValuePtr> entries;

    if (saved_first_key_for_next_batch) {
      DCHECK(saved_first_value_for_next_batch);
      entries.push_back(blink::mojom::SharedStorageKeyAndOrValue::New(
          saved_first_key_for_next_batch.value(),
          saved_first_value_for_next_batch.value()));
      saved_first_key_for_next_batch.reset();
      saved_first_value_for_next_batch.reset();
    }

    bool blob_retrieval_error = false;
    while (select_statement.Step()) {
      std::u16string key;
      if (!select_statement.ColumnBlobAsString16(0, &key)) {
        blob_retrieval_error = true;
        break;
      }
      std::u16string value;
      if (!select_statement.ColumnBlobAsString16(1, &value)) {
        blob_retrieval_error = true;
        break;
      }
      if (entries.size() < max_iterator_batch_size_) {
        entries.push_back(blink::mojom::SharedStorageKeyAndOrValue::New(
            std::move(key), std::move(value)));
      } else {
        // Cache the current key and value to use as the start of the next
        // batch, as we're already passing through this step and the next
        // iteration of `statement.Step()`, if there is one, during the next
        // iteration of the outer while loop, will give us the subsequent
        // key-value pair.
        saved_first_key_for_next_batch = std::move(key);
        saved_first_value_for_next_batch = std::move(value);
        has_more_entries = true;
        break;
      }
    }

    if (!select_statement.Succeeded() || blob_retrieval_error) {
      entries_listener->DidReadEntries(
          /*success=*/false,
          "SQL database encountered an error while retrieving entries.",
          /*entries=*/{}, /*has_more_entries=*/false,
          static_cast<int>(entry_count));
      return OperationResult::kSqlError;
    }

    entries_listener->DidReadEntries(/*success=*/true, /*error_message=*/"",
                                     std::move(entries), has_more_entries,
                                     static_cast<int>(entry_count));
  }

  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult
SharedStorageDatabase::PurgeMatchingOrigins(
    StorageKeyPolicyMatcherFunction storage_key_matcher,
    base::Time begin,
    base::Time end,
    bool perform_storage_cleanup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(begin, end);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  static constexpr char kSelectSql[] =
      "SELECT distinct context_origin FROM values_mapping "
      "WHERE last_used_time BETWEEN ? AND ? ";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindTime(0, begin);
  statement.BindTime(1, end);

  std::vector<std::string> origins;

  while (statement.Step()) {
    origins.push_back(statement.ColumnString(0));
  }

  if (!statement.Succeeded())
    return OperationResult::kSqlError;

  if (origins.empty())
    return OperationResult::kSuccess;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  for (const auto& origin : origins) {
    if (storage_key_matcher &&
        !storage_key_matcher.Run(blink::StorageKey::CreateFirstParty(
                                     url::Origin::Create(GURL(origin))),
                                 special_storage_policy_.get())) {
      continue;
    }

    if (!Purge(origin))
      return OperationResult::kSqlError;
  }

  if (!transaction.Commit())
    return OperationResult::kSqlError;

  if (perform_storage_cleanup && !Vacuum())
    return OperationResult::kSqlError;

  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::PurgeStale() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(staleness_threshold_, base::TimeDelta());

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  static constexpr char kUpdateLengthsSql[] =
      "UPDATE per_origin_mapping SET length = length - counts.num_expired "
      "FROM "
      "    (SELECT context_origin, COUNT(context_origin) AS num_expired "
      "    FROM values_mapping WHERE last_used_time<? "
      "    GROUP BY context_origin) "
      "AS counts "
      "WHERE per_origin_mapping.context_origin = counts.context_origin";

  sql::Statement update_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kUpdateLengthsSql));
  base::Time cutoff_time = clock_->Now() - staleness_threshold_;
  update_statement.BindTime(0, cutoff_time);

  if (!update_statement.Run()) {
    return OperationResult::kSqlError;
  }

  static constexpr char kDeleteEntriesSql[] =
      "DELETE FROM values_mapping WHERE last_used_time<?";
  sql::Statement entries_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteEntriesSql));
  entries_statement.BindTime(0, cutoff_time);

  // Delete expired entries.
  if (!entries_statement.Run())
    return OperationResult::kSqlError;

  static constexpr char kDeleteOriginsSql[] =
      "DELETE FROM per_origin_mapping WHERE length<=0";
  sql::Statement origins_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteOriginsSql));

  // Delete empty origins.
  if (!origins_statement.Run()) {
    return OperationResult::kSqlError;
  }

  static constexpr char kDeleteWithdrawalsSql[] =
      "DELETE FROM budget_mapping WHERE time_stamp<?";

  sql::Statement withdrawals_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteWithdrawalsSql));
  withdrawals_statement.BindTime(0, clock_->Now() - budget_interval_);

  // Remove stale budget withdrawals.
  if (!withdrawals_statement.Run())
    return OperationResult::kSqlError;

  if (!transaction.Commit())
    return OperationResult::kSqlError;
  return OperationResult::kSuccess;
}

std::vector<mojom::StorageUsageInfoPtr> SharedStorageDatabase::FetchOrigins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess)
    return {};

  static constexpr char kSelectSql[] =
      "SELECT context_origin,creation_time,length "
      "FROM per_origin_mapping "
      "ORDER BY context_origin";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  std::vector<mojom::StorageUsageInfoPtr> fetched_origin_infos;

  while (statement.Step()) {
    fetched_origin_infos.emplace_back(mojom::StorageUsageInfo::New(
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(GURL(statement.ColumnString(0)))),
        statement.ColumnInt64(2) * kSharedStorageEntryTotalBytesMultiplier *
            max_string_length_,
        statement.ColumnTime(1)));
  }

  if (!statement.Succeeded())
    return {};

  return fetched_origin_infos;
}

SharedStorageDatabase::OperationResult
SharedStorageDatabase::MakeBudgetWithdrawal(net::SchemefulSite context_site,
                                            double bits_debit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(bits_debit, 0.0);

  if (LazyInit(DBCreationPolicy::kCreateIfAbsent) != InitStatus::kSuccess)
    return OperationResult::kInitFailure;

  static constexpr char kInsertSql[] =
      "INSERT INTO budget_mapping(context_site,time_stamp,bits_debit)"
      "VALUES(?,?,?)";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, SerializeSite(context_site));
  statement.BindTime(1, clock_->Now());
  statement.BindDouble(2, bits_debit);

  if (!statement.Run())
    return OperationResult::kSqlError;
  return OperationResult::kSuccess;
}

SharedStorageDatabase::BudgetResult SharedStorageDatabase::GetRemainingBudget(
    net::SchemefulSite context_site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return BudgetResult(bit_budget_, OperationResult::kSuccess);
    else
      return BudgetResult(0.0, OperationResult::kInitFailure);
  }

  static constexpr char kSelectSql[] =
      "SELECT SUM(bits_debit) FROM budget_mapping "
      "WHERE context_site=? AND time_stamp>=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, SerializeSite(context_site));
  statement.BindTime(1, clock_->Now() - budget_interval_);

  double total_debits = 0.0;
  if (statement.Step())
    total_debits = statement.ColumnDouble(0);

  if (!statement.Succeeded())
    return BudgetResult(0.0, OperationResult::kSqlError);

  return BudgetResult(bit_budget_ - total_debits, OperationResult::kSuccess);
}

SharedStorageDatabase::TimeResult SharedStorageDatabase::GetCreationTime(
    url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return TimeResult(OperationResult::kNotFound);
    else
      return TimeResult(OperationResult::kInitFailure);
  }

  TimeResult result;
  int64_t length = 0L;
  result.result =
      GetOriginInfo(SerializeOrigin(context_origin), &length, &result.time);

  return result;
}

SharedStorageDatabase::MetadataResult SharedStorageDatabase::GetMetadata(
    url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MetadataResult metadata;

  metadata.length = Length(context_origin);

  TimeResult time_result = GetCreationTime(context_origin);
  metadata.time_result = time_result.result;
  if (time_result.result == OperationResult::kSuccess)
    metadata.creation_time = time_result.time;

  BudgetResult budget_result =
      GetRemainingBudget(net::SchemefulSite(context_origin));
  metadata.budget_result = budget_result.result;
  if (budget_result.result == OperationResult::kSuccess)
    metadata.remaining_budget = budget_result.bits;

  return metadata;
}

SharedStorageDatabase::EntriesResult
SharedStorageDatabase::GetEntriesForDevTools(url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EntriesResult entries;

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted) {
      entries.result = OperationResult::kSuccess;
      return entries;
    } else {
      entries.result = OperationResult::kInitFailure;
      return entries;
    }
  }

  static constexpr char kSelectSql[] =
      "SELECT key,value FROM values_mapping "
      "WHERE context_origin=? AND last_used_time>=? "
      "ORDER BY key";

  sql::Statement select_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  std::string origin_str(SerializeOrigin(context_origin));
  select_statement.BindString(0, origin_str);
  select_statement.BindTime(1, clock_->Now() - staleness_threshold_);

  while (select_statement.Step()) {
    std::u16string key;
    if (!select_statement.ColumnBlobAsString16(0, &key)) {
      key = u"[[DATABASE_ERROR: unable to retrieve key]]";
    }
    std::u16string value;
    if (!select_statement.ColumnBlobAsString16(1, &value)) {
      value = u"[[DATABASE_ERROR: unable to retrieve value]]";
    }
    entries.entries.emplace_back(base::UTF16ToUTF8(key),
                                 base::UTF16ToUTF8(value));
  }

  if (!select_statement.Succeeded())
    return entries;

  entries.result = OperationResult::kSuccess;
  return entries;
}

SharedStorageDatabase::OperationResult
SharedStorageDatabase::ResetBudgetForDevTools(url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted) {
      return OperationResult::kSuccess;
    } else {
      return OperationResult::kInitFailure;
    }
  }

  static constexpr char kDeleteSql[] =
      "DELETE FROM budget_mapping WHERE context_site=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, SerializeSite(net::SchemefulSite(context_origin)));

  if (!statement.Run()) {
    return OperationResult::kSqlError;
  }
  return OperationResult::kSuccess;
}

bool SharedStorageDatabase::IsOpenForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_.is_open();
}

SharedStorageDatabase::InitStatus SharedStorageDatabase::DBStatusForTesting()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_status_;
}

bool SharedStorageDatabase::OverrideCreationTimeForTesting(
    url::Origin context_origin,
    base::Time new_creation_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess)
    return false;

  std::string origin_str = SerializeOrigin(context_origin);
  int64_t length = 0L;
  base::Time old_creation_time;
  OperationResult result =
      GetOriginInfo(origin_str, &length, &old_creation_time);

  if (result != OperationResult::kSuccess &&
      result != OperationResult::kNotFound) {
    return false;
  }

  // Don't override time for non-existent origin.
  if (result == OperationResult::kNotFound)
    return true;

  return UpdatePerOriginMapping(origin_str, new_creation_time, length,
                                /*origin_exists=*/true);
}

bool SharedStorageDatabase::OverrideLastUsedTimeForTesting(
    url::Origin context_origin,
    std::u16string key,
    base::Time new_last_used_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess)
    return false;

  GetResult result = Get(context_origin, key);
  if (result.result != OperationResult::kSuccess &&
      result.result != OperationResult::kNotFound) {
    return false;
  }

  // Don't override time for non-existent key.
  if (result.result == OperationResult::kNotFound)
    return true;

  if (!UpdateValuesMappingWithTime(SerializeOrigin(context_origin), key,
                                   result.data, new_last_used_time,
                                   /*key_exists=*/true)) {
    return false;
  }
  return true;
}

void SharedStorageDatabase::OverrideClockForTesting(base::Clock* clock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(clock);
  clock_ = clock;
}

void SharedStorageDatabase::OverrideSpecialStoragePolicyForTesting(
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  special_storage_policy_ = std::move(special_storage_policy);
}

int64_t SharedStorageDatabase::GetNumBudgetEntriesForTesting(
    net::SchemefulSite context_site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return 0;
    else
      return -1;
  }

  static constexpr char kSelectSql[] =
      "SELECT COUNT(*) FROM budget_mapping "
      "WHERE context_site=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, SerializeSite(context_site));

  if (statement.Step())
    return statement.ColumnInt64(0);

  return -1;
}

int64_t SharedStorageDatabase::GetTotalNumBudgetEntriesForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return 0;
    else
      return -1;
  }

  static constexpr char kSelectSql[] = "SELECT COUNT(*) FROM budget_mapping";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));

  if (statement.Step())
    return statement.ColumnInt64(0);

  return -1;
}

SharedStorageDatabase::InitStatus SharedStorageDatabase::LazyInit(
    DBCreationPolicy policy) {
  // Early return in case of previous failure, to prevent an unbounded
  // number of re-attempts.
  if (db_status_ != InitStatus::kUnattempted)
    return db_status_;

  if (policy == DBCreationPolicy::kIgnoreIfAbsent && !DBExists())
    return InitStatus::kUnattempted;

  for (size_t i = 0; i < max_init_tries_; ++i) {
    db_status_ = InitImpl();
    if (db_status_ == InitStatus::kSuccess)
      return db_status_;

    meta_table_.Reset();
    db_.Close();
  }

  return db_status_;
}

bool SharedStorageDatabase::DBExists() {
  DCHECK_EQ(InitStatus::kUnattempted, db_status_);

  if (db_file_status_ == DBFileStatus::kNoPreexistingFile)
    return false;

  // The in-memory case is included in `DBFileStatus::kNoPreexistingFile`.
  DCHECK(is_filebacked());

  // We do not expect `DBExists()` to be called in the case where
  // `db_file_status_ == DBFileStatus::kPreexistingFile`, as then
  // `db_status_ != InitStatus::kUnattempted`, which would force an early return
  // in `LazyInit()`.
  DCHECK_EQ(DBFileStatus::kNotChecked, db_file_status_);

  // The histogram tag must be set before opening.
  db_.set_histogram_tag("SharedStorage");

  if (!db_.Open(db_path_)) {
    db_file_status_ = DBFileStatus::kNoPreexistingFile;
    return false;
  }

  static const char kSelectSql[] =
      "SELECT COUNT(*) FROM sqlite_schema WHERE type=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindCString(0, "table");

  if (!statement.Step() || statement.ColumnInt(0) == 0) {
    db_file_status_ = DBFileStatus::kNoPreexistingFile;
    return false;
  }

  db_file_status_ = DBFileStatus::kPreexistingFile;
  return true;
}

bool SharedStorageDatabase::OpenDatabase() {
  // If the database is open, the histogram tag will have already been set in
  // `DBExists()`, since it must be set before opening.
  if (!db_.is_open())
    db_.set_histogram_tag("SharedStorage");

  // If this is not the first call to `OpenDatabase()` because we are re-trying
  // initialization, then the error callback will have previously been set.
  db_.reset_error_callback();

  // base::Unretained is safe here because this SharedStorageDatabase owns
  // the sql::Database instance that stores and uses the callback. So,
  // `this` is guaranteed to outlive the callback.
  db_.set_error_callback(base::BindRepeating(
      &SharedStorageDatabase::DatabaseErrorCallback, base::Unretained(this)));

  if (is_filebacked()) {
    if (!db_.is_open() && !db_.Open(db_path_))
      return false;

    db_.Preload();
  } else {
    if (!db_.OpenInMemory())
      return false;
  }

  return true;
}

void SharedStorageDatabase::DatabaseErrorCallback(int extended_error,
                                                  sql::Statement* stmt) {
  base::UmaHistogramSparse("Storage.SharedStorage.Database.Error",
                           extended_error);

  if (sql::IsErrorCatastrophic(extended_error)) {
    bool success = Destroy();
    base::UmaHistogramBoolean("Storage.SharedStorage.Database.Destruction",
                              success);
    if (!success) {
      DLOG(FATAL) << "Database destruction failed after catastrophic error:\n"
                  << db_.GetErrorMessage();
    }
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db_.GetErrorMessage();
}

SharedStorageDatabase::InitStatus SharedStorageDatabase::InitImpl() {
  if (!OpenDatabase())
    return InitStatus::kError;

  // Database should now be open.
  DCHECK(db_.is_open());

  // Scope initialization in a transaction so we can't be partially initialized.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    LOG(WARNING) << "Shared storage database begin initialization failed.";
    db_.RazeAndPoison();
    return InitStatus::kError;
  }

  // Create the tables.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber) ||
      !InitSchema(db_, meta_table_)) {
    return InitStatus::kError;
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Shared storage database is too new.";
    db_.RazeAndPoison();
    return InitStatus::kTooNew;
  }

  int cur_version = meta_table_.GetVersionNumber();

  if (cur_version <= kDeprecatedVersionNumber) {
    LOG(WARNING) << "Shared storage database is too old to be compatible.";
    db_.RazeAndPoison();
    return InitStatus::kTooOld;
  }

  if (cur_version < kCurrentVersionNumber &&
      !UpgradeSharedStorageDatabaseSchema(db_, meta_table_, clock_)) {
    LOG(WARNING) << "Shared storage database upgrade failed.";
    db_.RazeAndPoison();
    return InitStatus::kUpgradeFailed;
  }

  // The initialization is complete.
  if (!transaction.Commit()) {
    LOG(WARNING) << "Shared storage database initialization commit failed.";
    db_.RazeAndPoison();
    return InitStatus::kError;
  }

  LogInitHistograms();
  return InitStatus::kSuccess;
}

bool SharedStorageDatabase::Vacuum() {
  DCHECK_EQ(InitStatus::kSuccess, db_status_);
  DCHECK_EQ(0, db_.transaction_nesting())
      << "Can not have a transaction when vacuuming.";
  return db_.Execute("VACUUM");
}

bool SharedStorageDatabase::Purge(const std::string& context_origin) {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kDeleteSql[] =
      "DELETE FROM values_mapping "
      "WHERE context_origin=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, context_origin);

  if (!statement.Run())
    return false;

  if (!DeleteFromPerOriginMapping(context_origin)) {
    return false;
  }

  return transaction.Commit();
}

int64_t SharedStorageDatabase::NumEntriesTotal(
    const std::string& context_origin) {
  // In theory, there ought to be at most one entry found. But we make no
  // assumption about the state of the disk. In the rare case that multiple
  // entries are found, we return only the `length` from the first entry found.
  static constexpr char kSelectSql[] =
      "SELECT length FROM per_origin_mapping "
      "WHERE context_origin=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, context_origin);

  int64_t num_entries = 0;
  if (statement.Step())
    num_entries = statement.ColumnInt64(0);

  return num_entries;
}

int64_t SharedStorageDatabase::NumEntriesManualCount(
    const std::string& context_origin) {
  static constexpr char kCountSql[] =
      "SELECT COUNT(*) FROM values_mapping "
      "WHERE context_origin=? AND last_used_time>=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kCountSql));
  statement.BindString(0, context_origin);
  statement.BindTime(1, clock_->Now() - staleness_threshold_);

  int64_t length = 0;
  if (statement.Step())
    length = statement.ColumnInt64(0);

  if (!statement.Succeeded())
    length = -1;

  return length;
}

bool SharedStorageDatabase::HasEntryFor(const std::string& context_origin,
                                        const std::u16string& key) {
  static constexpr char kSelectSql[] =
      "SELECT 1 FROM values_mapping "
      "WHERE context_origin=? AND key=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, context_origin);
  statement.BindBlob(1, key);

  return statement.Step();
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::GetOriginInfo(
    const std::string& context_origin,
    int64_t* out_length,
    base::Time* out_creation_time) {
  DCHECK(out_length);
  DCHECK(out_creation_time);

  // In theory, there ought to be at most one entry found. But we make no
  // assumption about the state of the disk. In the rare case that multiple
  // entries are found, we retrieve only the `length` and `creation_time`
  // from the first entry found.
  static constexpr char kSelectSql[] =
      "SELECT length,creation_time FROM per_origin_mapping "
      "WHERE context_origin=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, context_origin);

  if (statement.Step()) {
    *out_length = statement.ColumnInt64(0);
    *out_creation_time = statement.ColumnTime(1);
    return OperationResult::kSuccess;
  }

  if (!statement.Succeeded())
    return OperationResult::kSqlError;
  return OperationResult::kNotFound;
}

bool SharedStorageDatabase::UpdateLength(const std::string& context_origin,
                                         int64_t delta) {
  int64_t length = 0L;
  base::Time creation_time;
  OperationResult result =
      GetOriginInfo(context_origin, &length, &creation_time);

  if (result != OperationResult::kSuccess &&
      result != OperationResult::kNotFound) {
    return false;
  }

  bool origin_exists = true;
  if (result == OperationResult::kNotFound) {
    // Don't delete or insert for non-existent origin when we would have
    // decremented the length.
    if (delta < 0L)
      return true;

    // We are creating `context_origin` now.
    creation_time = clock_->Now();
    origin_exists = false;
  }

  int64_t new_length = (length + delta > 0L) ? length + delta : 0L;

  return UpdatePerOriginMapping(context_origin, creation_time, new_length,
                                origin_exists);
}

bool SharedStorageDatabase::UpdateValuesMappingWithTime(
    const std::string& context_origin,
    const std::u16string& key,
    const std::u16string& value,
    base::Time last_used_time,
    bool key_exists) {
  if (key_exists) {
    static constexpr char kUpdateSql[] =
        "UPDATE values_mapping SET value=?, last_used_time=? "
        "WHERE context_origin=? AND key=?";

    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kUpdateSql));
    statement.BindBlob(0, value);
    statement.BindTime(1, last_used_time);
    statement.BindString(2, context_origin);
    statement.BindBlob(3, key);

    return statement.Run();
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  static constexpr char kInsertSql[] =
      "INSERT INTO values_mapping(context_origin,key,value,last_used_time) "
      "VALUES(?,?,?,?)";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, context_origin);
  statement.BindBlob(1, key);
  statement.BindBlob(2, value);
  statement.BindTime(3, last_used_time);

  if (!statement.Run())
    return false;

  if (!UpdateLength(context_origin, /*delta=*/1))
    return false;

  return transaction.Commit();
}

bool SharedStorageDatabase::UpdateValuesMapping(
    const std::string& context_origin,
    const std::u16string& key,
    const std::u16string& value,
    bool key_exists) {
  return UpdateValuesMappingWithTime(context_origin, key, value, clock_->Now(),
                                     key_exists);
}

bool SharedStorageDatabase::DeleteFromPerOriginMapping(
    const std::string& context_origin) {
  static constexpr char kDeleteSql[] =
      "DELETE FROM per_origin_mapping "
      "WHERE context_origin=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, context_origin);

  return statement.Run();
}

bool SharedStorageDatabase::InsertIntoPerOriginMapping(
    const std::string& context_origin,
    base::Time creation_time,
    uint64_t length) {
  static constexpr char kInsertSql[] =
      "INSERT INTO per_origin_mapping(context_origin,creation_time,length) "
      "VALUES(?,?,?)";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, context_origin);
  statement.BindTime(1, creation_time);
  statement.BindInt64(2, static_cast<int64_t>(length));

  return statement.Run();
}

bool SharedStorageDatabase::UpdatePerOriginMapping(
    const std::string& context_origin,
    base::Time creation_time,
    uint64_t length,
    bool origin_exists) {
  DCHECK(length >= 0L);

  if (length && origin_exists) {
    static constexpr char kUpdateSql[] =
        "UPDATE per_origin_mapping SET creation_time=?, length=? "
        "WHERE context_origin=?";
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kUpdateSql));
    statement.BindTime(0, creation_time);
    statement.BindInt64(1, static_cast<int64_t>(length));
    statement.BindString(2, context_origin);

    return statement.Run();
  }
  if (length) {
    return InsertIntoPerOriginMapping(context_origin, creation_time, length);
  }
  if (origin_exists) {
    return DeleteFromPerOriginMapping(context_origin);
  }

  //  Origin does not exist and we are trying to set the length to 0, so this is
  //  a no-op.
  return true;
}

bool SharedStorageDatabase::HasCapacity(const std::string& context_origin) {
  return NumEntriesTotal(context_origin) < max_entries_per_origin_;
}

void SharedStorageDatabase::LogInitHistograms() {
  base::UmaHistogramBoolean("Storage.SharedStorage.Database.IsFileBacked",
                            is_filebacked());

  if (is_filebacked()) {
    int64_t file_size = 0L;
    if (base::GetFileSize(db_path_, &file_size)) {
      int64_t file_size_kb = file_size / 1024;
      base::UmaHistogramCounts10M(
          "Storage.SharedStorage.Database.FileBacked.FileSize.KB",
          file_size_kb);

      int64_t file_size_gb = file_size_kb / (1024 * 1024);
      if (file_size_gb) {
        base::UmaHistogramCounts1000(
            "Storage.SharedStorage.Database.FileBacked.FileSize.GB",
            file_size_gb);
      }
    }

    static constexpr char kOriginCountSql[] =
        "SELECT COUNT(*) FROM per_origin_mapping";

    sql::Statement origin_count_statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kOriginCountSql));

    if (origin_count_statement.Step()) {
      int64_t origin_count = origin_count_statement.ColumnInt64(0);
      base::UmaHistogramCounts100000(
          "Storage.SharedStorage.Database.FileBacked.NumOrigins", origin_count);

      static constexpr char kQuartileSql[] =
          "SELECT AVG(length) FROM "
          "(SELECT length FROM per_origin_mapping "
          "ORDER BY length LIMIT ? OFFSET ?)";

      sql::Statement median_statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kQuartileSql));
      median_statement.BindInt64(0, 2 - (origin_count % 2));
      median_statement.BindInt64(1, (origin_count - 1) / 2);

      if (median_statement.Step()) {
        base::UmaHistogramCounts100000(
            "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin."
            "Median",
            median_statement.ColumnInt64(0));
      }

      // We use Method 1 from https://en.wikipedia.org/wiki/Quartile to
      // calculate upper and lower quartiles.
      int64_t quartile_limit = 2 - (origin_count % 4) / 2;
      int64_t quartile_offset = (origin_count > 1) ? (origin_count - 2) / 4 : 0;
      sql::Statement q1_statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kQuartileSql));
      q1_statement.BindInt64(0, quartile_limit);
      q1_statement.BindInt64(1, quartile_offset);

      if (q1_statement.Step()) {
        base::UmaHistogramCounts100000(
            "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin.Q1",
            q1_statement.ColumnInt64(0));
      }

      // We use Method 1 from https://en.wikipedia.org/wiki/Quartile to
      // calculate upper and lower quartiles.
      static constexpr char kUpperQuartileSql[] =
          "SELECT AVG(length) FROM "
          "(SELECT length FROM per_origin_mapping "
          "ORDER BY length DESC LIMIT ? OFFSET ?)";

      sql::Statement q3_statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kUpperQuartileSql));
      q3_statement.BindInt64(0, quartile_limit);
      q3_statement.BindInt64(1, quartile_offset);

      if (q3_statement.Step()) {
        base::UmaHistogramCounts100000(
            "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin.Q3",
            q3_statement.ColumnInt64(0));
      }

      static constexpr char kMinSql[] =
          "SELECT MIN(length) FROM per_origin_mapping";

      sql::Statement min_statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kMinSql));

      if (min_statement.Step()) {
        base::UmaHistogramCounts100000(
            "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin."
            "Min",
            min_statement.ColumnInt64(0));
      }

      static constexpr char kMaxSql[] =
          "SELECT MAX(length) FROM per_origin_mapping";

      sql::Statement max_statement(
          db_.GetCachedStatement(SQL_FROM_HERE, kMaxSql));

      if (max_statement.Step()) {
        base::UmaHistogramCounts100000(
            "Storage.SharedStorage.Database.FileBacked.NumEntries.PerOrigin."
            "Max",
            max_statement.ColumnInt64(0));
      }
    }

    static constexpr char kValueCountSql[] =
        "SELECT COUNT(*) FROM values_mapping";

    sql::Statement value_count_statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kValueCountSql));

    if (value_count_statement.Step()) {
      base::UmaHistogramCounts10M(
          "Storage.SharedStorage.Database.FileBacked.NumEntries.Total",
          value_count_statement.ColumnInt64(0));
    }
  }
}

}  // namespace storage
