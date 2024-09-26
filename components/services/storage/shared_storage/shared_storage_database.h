// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_DATABASE_H_

#include <inttypes.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/clock.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"

namespace base {
class FilePath;
class Time;
class TimeDelta;
}  // namespace base

namespace sql {
class Statement;
}  // namespace sql

namespace url {
class Origin;
}  // namespace url

namespace storage {
struct SharedStorageDatabaseOptions;
class SpecialStoragePolicy;

// Multiplier for determining the padded total size in bytes that an origin
// is using. Exposed to content/ for testing.
extern const int kSharedStorageEntryTotalBytesMultiplier;

// Wraps its own `sql::Database` instance on behalf of the Shared Storage
// backend implementation. This object is not sequence-safe and must be
// instantiated on a sequence which allows use of blocking file operations.
class SharedStorageDatabase {
 public:
  // A callback type to check if a given StorageKey matches a storage policy.
  // Can be passed empty/null where used, which means the StorageKey will always
  // match.
  using StorageKeyPolicyMatcherFunction =
      base::RepeatingCallback<bool(const blink::StorageKey&,
                                   SpecialStoragePolicy*)>;

  enum class InitStatus {
    kUnattempted =
        0,  // Status if `LazyInit()` has not yet been called or if `LazyInit()`
            // has early returned due to `DBCreationPolicy::kIgnoreIfAbsent`.
    kSuccess = 1,  // Status if `LazyInit()` was successful.
    kError = 2,    // Status if `LazyInit()` failed and a more specific error
                   // wasn't diagnosed.
    kTooNew = 3,   // Status if `LazyInit()` failed due to a compatible version
                   // number being too high.
    kTooOld = 4,  // Status if `LazyInit()` failed due to a version number being
                  // too low.
    kUpgradeFailed =
        5,  // Status if migration to current database version failed.
  };

  enum class DBFileStatus {
    kNotChecked = 0,  // Status if DB is file-backed and there hasn't been an
                      // attempt to open the SQL database for the given FilePath
                      // to see if it exists and contains data.
    kNoPreexistingFile =
        1,  // Status if the DB is in-memory or if the DB is file-backed but the
            // attempt to open it was unsuccessful or any pre-existing file
            // contained no data.
    kPreexistingFile =
        2,  // Status if there was a pre-existing file containing at least one
            // table that we were able to successfully open.
  };

  enum class SetBehavior {
    kDefault = 0,  // Sets entry regardless of whether one previously exists.
    kIgnoreIfPresent = 1,  // Does not set an entry if one previously exists.
  };

  enum class OperationResult {
    kSuccess = 0,      // Result if a non-setting operation is successful.
    kSet = 1,          // Result if value is set.
    kIgnored = 2,      // Result if value was present and ignored; no error.
    kSqlError = 3,     // Result if there is a SQL database error.
    kInitFailure = 4,  // Result if database initialization failed and a
                       // database is required.
    kNoCapacity = 5,   // Result if there was insufficient capacity for the
    // requesting origin.
    kInvalidAppend = 6,  // Result if the length of the value after appending
    // would exceed the maximum allowed length.
    kNotFound =
        7,  // Result if a key could not be retrieved via `Get()`, a creation
            // time could not be retrieved for an origin via
            // `GetCreationTime()`, or the data from `per_origin_mapping` could
            // not be found via `GetOriginInfo()`, because the key or origin
            // doesn't exist in the database.
    kTooManyFound = 8,  // Result if the number of keys/entries retrieved for
                        // `Keys()`/`Entries()` exceeds INT_MAX.
    kExpired = 9,       // Result if the retrieved entry is expired.
  };

  // Bundles a retrieved string `data` and its last write time `last_used_time`
  // from the database along with a field `result` indicating whether the
  // transaction was free of SQL errors.
  struct GetResult {
    std::u16string data;
    base::Time last_used_time = base::Time::Min();
    OperationResult result = OperationResult::kSqlError;
    GetResult();
    GetResult(const GetResult&) = delete;
    GetResult(GetResult&&);
    explicit GetResult(OperationResult result);
    GetResult(std::u16string data,
              base::Time last_used_time,
              OperationResult result);
    ~GetResult();
    GetResult& operator=(const GetResult&) = delete;
    GetResult& operator=(GetResult&&);
  };

  // Bundles a double `bits` representing the available bits remaining for the
  // queried origin along with a field indicating whether the database retrieval
  // was free of SQL errors.
  struct BudgetResult {
    double bits = 0.0;
    OperationResult result = OperationResult::kSqlError;
    BudgetResult(const BudgetResult&) = delete;
    BudgetResult(BudgetResult&&);
    BudgetResult(double bits, OperationResult result);
    ~BudgetResult();
    BudgetResult& operator=(const BudgetResult&) = delete;
    BudgetResult& operator=(BudgetResult&&);
  };

  // Bundles a `time` with a field indicating whether the database retrieval
  // was free of SQL errors.
  struct TimeResult {
    base::Time time;
    OperationResult result = OperationResult::kSqlError;
    TimeResult();
    TimeResult(const TimeResult&) = delete;
    TimeResult(TimeResult&&);
    explicit TimeResult(OperationResult result);
    ~TimeResult();
    TimeResult& operator=(const TimeResult&) = delete;
    TimeResult& operator=(TimeResult&&);
  };

  // Bundles info about an origin's shared storage for DevTools integration.
  struct MetadataResult {
    int length = -1;
    base::Time creation_time = base::Time::Min();
    double remaining_budget = 0;
    OperationResult time_result = OperationResult::kSqlError;
    OperationResult budget_result = OperationResult::kSqlError;
    MetadataResult();
    MetadataResult(const MetadataResult&) = delete;
    MetadataResult(MetadataResult&&);
    ~MetadataResult();
    MetadataResult& operator=(const MetadataResult&) = delete;
    MetadataResult& operator=(MetadataResult&&);
  };

  // Bundles an origin's shared storage entries with an `OperationResult` for
  // DevTools integration.
  struct EntriesResult {
    std::vector<std::pair<std::string, std::string>> entries;
    OperationResult result = OperationResult::kSqlError;
    EntriesResult();
    EntriesResult(const EntriesResult&) = delete;
    EntriesResult(EntriesResult&&);
    ~EntriesResult();
    EntriesResult& operator=(const EntriesResult&) = delete;
    EntriesResult& operator=(EntriesResult&&);
  };

  // Exposed for testing.
  static const int kCurrentVersionNumber;
  static const int kCompatibleVersionNumber;
  static const int kDeprecatedVersionNumber;

  // When `db_path` is empty, the database will be opened in memory only.
  SharedStorageDatabase(
      base::FilePath db_path,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<SharedStorageDatabaseOptions> options);

  SharedStorageDatabase(const SharedStorageDatabase&) = delete;
  SharedStorageDatabase(const SharedStorageDatabase&&) = delete;

  ~SharedStorageDatabase();

  SharedStorageDatabase& operator=(const SharedStorageDatabase&) = delete;
  SharedStorageDatabase& operator=(const SharedStorageDatabase&&) = delete;

  // Deletes the database and returns whether the operation was successful.
  //
  // It is OK to call `Destroy()` regardless of whether `Init()` was successful.
  [[nodiscard]] bool Destroy();

  // Returns a pointer to the database containing the actual data.
  [[nodiscard]] sql::Database* db() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &db_;
  }

  // Returns whether or not the database is file-backed (rather than in-memory).
  [[nodiscard]] bool is_filebacked() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !db_path_.empty();
  }

  // Releases all non-essential memory associated with this database connection.
  void TrimMemory();

  // Retrieves the `value` for `context_origin` and `key`. Returns a
  // struct containing a `data` string if a `value` is found, with an
  // `OperationResult` indicating whether the value was found and the
  // transaction was free of errors.
  //
  // If an expired value is found, then `OperationResult::kExpired` is returned;
  // if no value is found, `OperationResult::kNotFound` is returned.
  //
  // Note that `key` is assumed to be of length at most
  // `max_string_length_`, with the burden on the caller to handle errors for
  // strings that exceed this length.
  [[nodiscard]] GetResult Get(url::Origin context_origin, std::u16string key);

  // Sets an entry for `context_origin` and `key` to have `value`.
  // If `behavior` is `kIgnoreIfPresent` and an unexpired entry already exists
  // for `context_origin` and `key`, then the table is not modified. If an
  // expired entry is found, the entry is replaced, regardless of `behavior`.
  // Returns an enum indicating whether or not a new entry is added, the request
  // is ignored, or if there is an error.
  //
  // Note that `key` and `value` assumed to be each of length at
  // most `max_string_length_`, with the burden on the caller to handle errors
  // for strings that exceed this length. Moreover, if `Length(context_origin)`
  // equals `max_entries_per_origin_`, `Set()` will return a value of
  // `OperationResult::kNoCapacity` and the table will not be modified.
  [[nodiscard]] OperationResult Set(
      url::Origin context_origin,
      std::u16string key,
      std::u16string value,
      SetBehavior behavior = SetBehavior::kDefault);

  // Appends `tail_value` to the end of the current `value`
  // for `context_origin` and `key`, if `key` exists and is not expired. If
  // `key` does not exist, creates an entry for `key` with value `tail_value`.
  // If `key` is expired, creates an entry for `key` with value `tail_value`
  // after deleting the expired entry. Returns an enum indicating whether or not
  // an entry is added/modified or if there is an error.
  //
  // Note that `key` and `value` are assumed to be each of length
  // at most `max_string_length_`, with the burden on the caller to handle
  // errors for strings that exceed this length. Moreover, if the length of the
  // string obtained by concatening the current `value` (if one exists)
  // and `tail_value` exceeds `max_string_length_`, or if
  // `Length(context_origin)` equals `max_entries_per_origin_`, `Append()` will
  // return a value of `OperationResult::kNoCapacity` and the table will not be
  // modified.
  [[nodiscard]] OperationResult Append(url::Origin context_origin,
                                       std::u16string key,
                                       std::u16string tail_value);

  // Deletes the entry for `context_origin` and `key`. Returns
  // whether the deletion is successful.
  //
  // Note that `key` is assumed to be of length at most
  // `max_string_length_`, with the burden on the caller to handle errors for
  // strings that exceed this length.
  [[nodiscard]] OperationResult Delete(url::Origin context_origin,
                                       std::u16string key);

  // Clears all entries for `context_origin`. Returns whether the operation is
  // successful.
  [[nodiscard]] OperationResult Clear(url::Origin context_origin);

  // Returns the number of unexpired entries for `context_origin` in the
  // database, or -1 on error.
  // TODO(crbug.com/1277662): Consider renaming to something more descriptive.
  [[nodiscard]] int64_t Length(url::Origin context_origin);

  // From a list of all the unexpired keys for `context_origin` taken in
  // lexicographic order, send batches of keys to the Shared Storage worklet's
  // async iterator via a remote that consumes `pending_listener`. Returns
  // whether the operation was successful.
  [[nodiscard]] OperationResult Keys(
      const url::Origin& context_origin,
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener);

  // From a list of all the unexpired key-value pairs for `context_origin` taken
  // in lexicographic order, send batches of key-value pairs to the Shared
  // Storage worklet's async iterator via a remote that consumes
  // `pending_listener`. Returns whether the operation was successful.
  [[nodiscard]] OperationResult Entries(
      const url::Origin& context_origin,
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener);

  // Clears all origins that match `storage_key_matcher` run on the owning
  // StoragePartition's `SpecialStoragePolicy` and have `creation_time` between
  // the times `begin` and `end`. If `perform_storage_cleanup` is true, vacuums
  // the database afterwards. Returns whether the transaction was successful.
  [[nodiscard]] OperationResult PurgeMatchingOrigins(
      StorageKeyPolicyMatcherFunction storage_key_matcher,
      base::Time begin,
      base::Time end,
      bool perform_storage_cleanup = false);

  // Clear all entries whose `last_used_time` (currently the last write access)
  // falls before `clock_->Now() - staleness_threshold_`. Also purges, for all
  // origins, all privacy budget withdrawals that have `time_stamps` older than
  // `clock_->Now() - budget_interval_`. Returns whether the transaction was
  // successful.
  [[nodiscard]] OperationResult PurgeStale();

  // Fetches a vector of `mojom::StorageUsageInfoPtr`, with one
  // `mojom::StorageUsageInfoPtr` for each origin currently using shared
  // storage in this profile.
  [[nodiscard]] std::vector<mojom::StorageUsageInfoPtr> FetchOrigins();

  // Makes a withdrawal of `bits_debit` stamped with the current time from the
  // privacy budget of `context_origin`.
  [[nodiscard]] OperationResult MakeBudgetWithdrawal(url::Origin context_origin,
                                                     double bits_debit);

  // Determines the number of bits remaining in the privacy budget of
  // `context_origin`, where only withdrawals within the most recent
  // `budget_interval_` are counted as still valid, and returns this information
  // bundled with an `OperationResult` value to indicate whether the database
  // retrieval was successful.
  [[nodiscard]] BudgetResult GetRemainingBudget(url::Origin context_origin);

  // Retrieves the most recent `creation_time` for `context_origin`.
  [[nodiscard]] TimeResult GetCreationTime(url::Origin context_origin);

  // Calls `Length()`, `GetRemainingBudget()`, and `GetCreationTime()`, then
  // bundles this info along with the accompanying `OperationResult`s into a
  // struct to send to the DevTools `StorageHandler`.
  [[nodiscard]] MetadataResult GetMetadata(url::Origin context_origin);

  // Returns an origin's entries in a vector bundled with an `OperationResult`.
  // To only be used by DevTools.
  [[nodiscard]] EntriesResult GetEntriesForDevTools(url::Origin context_origin);

  // Removes all budget withdrawals for `context_origin`. Intended as a
  // convenience for the DevTools UX.
  [[nodiscard]] OperationResult ResetBudgetForDevTools(
      url::Origin context_origin);

  // Returns whether the SQLite database is open.
  [[nodiscard]] bool IsOpenForTesting() const;

  // Returns the `db_status_` for tests.
  [[nodiscard]] InitStatus DBStatusForTesting() const;

  // Changes `creation_time` to `new_creation_time` for `context_origin`.
  [[nodiscard]] bool OverrideCreationTimeForTesting(
      url::Origin context_origin,
      base::Time new_creation_time);

  // Changes `last_used_time` to `new_last_used_time` for `context_origin` and
  // `key`.
  [[nodiscard]] bool OverrideLastUsedTimeForTesting(
      url::Origin context_origin,
      std::u16string key,
      base::Time new_last_used_time);

  // Overrides the clock used to check the time.
  void OverrideClockForTesting(base::Clock* clock);

  // Overrides the `SpecialStoragePolicy` for tests.
  void OverrideSpecialStoragePolicyForTesting(
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);

  // Gets the number of entries (including stale entries) in the table
  // `budget_mapping` for `context_origin`. Returns -1 in case of database
  // initialization failure or SQL error.
  [[nodiscard]] int64_t GetNumBudgetEntriesForTesting(
      url::Origin context_origin);

  // Returns the total number of entries in the table for all origins, or -1 in
  // case of database initialization failure or SQL error.
  [[nodiscard]] int64_t GetTotalNumBudgetEntriesForTesting();

  // Populates the database in order to test integration with
  // `content::StoragePartitionImpl` while keeping in this file the parts of
  // those tests that depend on implementation details of
  // `SharedStorageDatabase`.
  //
  // Sets two example key-value pairs for `origin1`, one example pair for
  // `origin2`, and two example pairs for `origin3`, while also overriding the
  // `creation_time` for `origin2` so that it is 1 day earlier and the
  // `creation_time` for `origin3` so that it is 60 days earlier.
  [[nodiscard]] bool PopulateDatabaseForTesting(url::Origin origin1,
                                                url::Origin origin2,
                                                url::Origin origin3);

 private:
  // Policy to tell `LazyInit()` whether or not to create a new database if a
  // pre-existing on-disk database is not found.
  enum class DBCreationPolicy {
    kIgnoreIfAbsent = 0,
    kCreateIfAbsent = 1,
  };

  // Called at the start of each public operation, and initializes the database
  // if it isn't already initialized (unless there is no pre-existing on-disk
  // database to initialize and `policy` is
  // `DBCreationPolicy::kIgnoreIfAbsent`).
  [[nodiscard]] InitStatus LazyInit(DBCreationPolicy policy)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Determines whether or not an uninitialized DB already exists on disk.
  [[nodiscard]] bool DBExists() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // If `db_path_` is empty, opens a temporary database in memory; otherwise
  // opens a persistent database with the absolute path `db_path`, creating the
  // file if it does not yet exist. Returns whether opening was successful.
  [[nodiscard]] bool OpenDatabase() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Callback for database errors. Schedules a call to Destroy() if the
  // error is catastrophic.
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Helper function to implement internals of `Init()`.  This allows
  // Init() to retry in case of failure, since some failures run
  // recovery code.
  [[nodiscard]] InitStatus InitImpl() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW. Returns whether the
  // operation was successful.
  [[nodiscard]] bool Vacuum() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Clears all entries for `context_origin`. Returns whether deletion is
  // successful. Not named `Clear()` to distinguish it from the public method
  // called via `SequenceBound::AsyncCall()`. We remove `context_origin` from
  // `per_origin_mapping` if the origin becomes empty.
  [[nodiscard]] bool Purge(const std::string& context_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns the total number of entries for `context_origin`, including any
  // expired entries for `context_origin` that have not yet been purged.
  [[nodiscard]] int64_t NumEntriesTotal(const std::string& context_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns the number of entries for `context_origin`, as determined by a
  // manual "COUNT(*)" query, rather than relying on the `length` recorded in
  // `per_origin_mapping`. Returns -1 if there is a database error.
  [[nodiscard]] int64_t NumEntriesManualCount(const std::string& context_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns whether an entry exists for `context_origin` and `key`.
  [[nodiscard]] bool HasEntryFor(const std::string& context_origin,
                                 const std::u16string& key)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Retrieves the `length` in `out_length`, and `creation_time` in
  // `out_creation_time`, of `context_origin`. Leaves the `out_*` parameters
  // unchanged if `context_origin` is not found in the database. Returns an
  // `OperationResult` indicating success, error, or that the origin was not
  // found.
  [[nodiscard]] OperationResult GetOriginInfo(const std::string& context_origin,
                                              int64_t* out_length,
                                              base::Time* out_creation_time)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Updates `length` by `delta` for `context_origin`. If
  // `length + delta == 0L`, then we remove `context_origin` from
  // `per_origin_mapping`.
  [[nodiscard]] bool UpdateLength(const std::string& context_origin,
                                  int64_t delta)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // If `key_exists`, updates the row for `context_origin` in `values_mapping`
  // to `(context_origin,key,value,last_used_time)`. Otherwise, inserts a tuple
  // for `(context_origin,key,value,last_used_time)` into `values_mapping` and
  // calls `UpdateLength()` with `delta=1`.
  //
  // Precondition: Must have called `Get()` synchronously beforehand to check
  // whether row already exists.
  [[nodiscard]] bool UpdateValuesMappingWithTime(
      const std::string& context_origin,
      const std::u16string& key,
      const std::u16string& value,
      base::Time last_used_time,
      bool key_exists) VALID_CONTEXT_REQUIRED(sequence_checker_);

  // If `key_exists`, updates the row for `context_origin` in `values_mapping`
  // to `(context_origin,key,value,clock_->Now())`  (i.e. uses the current time
  // as `last_used_time`). Otherwise, inserts a tuple for
  // `(context_origin,key,value,clock_->Now())` into `values_mapping` and calls
  // `UpdateLength()` with `delta=1`.
  //
  // Precondition: Must have called `Get()` synchronously beforehand to check
  // whether row already exists.
  [[nodiscard]] bool UpdateValuesMapping(const std::string& context_origin,
                                         const std::u16string& key,
                                         const std::u16string& value,
                                         bool key_exists)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Deletes the row for `context_origin` from `per_origin_mapping`.
  [[nodiscard]] bool DeleteFromPerOriginMapping(
      const std::string& context_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Inserts the triple for `(context_origin, creation_time, length)` into
  // `per_origin_mapping`.
  [[nodiscard]] bool InsertIntoPerOriginMapping(
      const std::string& context_origin,
      base::Time creation_time,
      uint64_t length) VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Updates the row for `context_origin` from `per_origin_mapping` with the
  // triple `(context_origin, creation_time, length)`, unless `length` is 0
  // and/or `context_origin` does not yet exist. In the case where `length` is 0
  // and `context_origin` exists, we simply delete the existing row. If
  // `context_origin` does not yet exist, and `length` is positive, we simply
  // insert the row instead of updating. `origin_exists` specifies whether or
  // not `context_origin` already exists in `per_origin_mapping`.
  //
  // Precondition: Must have called `GetOriginInfo()` synchronously beforehand
  // to determine whether origin already exists.
  [[nodiscard]] bool UpdatePerOriginMapping(const std::string& context_origin,
                                            base::Time creation_time,
                                            uint64_t length,
                                            bool origin_exists)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns whether the `length` for `context_origin` is less than
  // `max_entries_per_origin_`.
  [[nodiscard]] bool HasCapacity(const std::string& context_origin)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Logs following initialization various histograms, including e.g. the number
  // of origins currently in `per_origin_mapping`, as well as each of the
  // lengths listed in `per_origin_mapping`.
  void LogInitHistograms() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Database containing the actual data.
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Contains the version information.
  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Initialization status of `db_`.
  GUARDED_BY_CONTEXT(sequence_checker_)
  InitStatus db_status_ = InitStatus::kUnattempted;

  // Only set to true if `DBExists()
  DBFileStatus db_file_status_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Path to the database, if file-backed.
  base::FilePath db_path_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Owning partition's storage policy.
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Maximum allowed number of entries per origin.
  const int64_t max_entries_per_origin_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Maximum size of a string input from any origin's script. Applies
  // separately to both script keys and script values.
  size_t max_string_length_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Maxmium number of times that SQL database attempts to initialize.
  size_t max_init_tries_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Maximum number of keys or key-value pairs returned per batch by the
  // async `Keys()` and `Entries()` iterators, respectively.
  size_t max_iterator_batch_size_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Maximum number of bits of entropy allowed per origin to output via the
  // Shared Storage API.
  const double bit_budget_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Interval over which `bit_budget_` is defined.
  const base::TimeDelta budget_interval_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Length of time between last key write access and key expiration. When an
  // entry's data is older than this threshold, it will be auto-purged.
  const base::TimeDelta staleness_threshold_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Clock used to determine current time. Can be overridden in tests.
  raw_ptr<base::Clock> clock_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_DATABASE_H_
