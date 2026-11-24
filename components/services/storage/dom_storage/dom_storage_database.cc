// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_database.h"

#include "base/byte_size.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/types/expected_macros.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/session_storage_leveldb.h"
#include "components/services/storage/dom_storage/sqlite/local_storage_sqlite.h"
#include "components/services/storage/dom_storage/sqlite/session_storage_sqlite.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"
#include "components/services/storage/public/cpp/constants.h"
#include "sql/database.h"

namespace storage {

namespace {

// Records the on-disk size of the database at `db_path` to the
// `DatabaseOnDiskSizeKB` histogram for `storage_type` (`.OnDiskExperimental`
// suffix for `kOnDiskExperimental`, unsuffixed for `kOnDisk`). LevelDB stores
// its data in a directory, while SQLite stores a single file plus a `-wal` file
// that may be absent depending on checkpoint state.
void RecordDatabaseOnDiskSizeKB(StorageType storage_type,
                                const base::FilePath& db_path,
                                bool is_sqlite,
                                DatabaseMetricsType metrics_type) {
  int64_t size_bytes = 0;
  if (is_sqlite) {
    size_bytes += base::GetFileSize(db_path).value_or(0);
    size_bytes += base::GetFileSize(sql::Database::WriteAheadLogPath(db_path))
                      .value_or(0);
  } else {
    size_bytes = base::ComputeDirectorySize(db_path);
  }
  std::string_view name_prefix =
      storage_type == StorageType::kLocalStorage
          ? "LocalStorage.DatabaseOnDiskSizeKB"
          : "Storage.SessionStorage.DatabaseOnDiskSizeKB";
  base::UmaHistogramMemoryKB(
      base::StrCat(
          {name_prefix, metrics_type == DatabaseMetricsType::kOnDiskExperimental
                            ? ".OnDiskExperimental"
                            : ""}),
      base::ByteSize(base::checked_cast<uint64_t>(size_bytes)));
}

// Records all open-time telemetry for a database open attempt:
//   * `Storage.{LocalStorage,SessionStorage}.OpenDatabase` status.
//   * `Storage.{LocalStorage,SessionStorage}.Duration.OpenDatabase2` duration.
//   * The on-disk size histogram, named `LocalStorage.DatabaseOnDiskSizeKB` for
//     LocalStorage and `Storage.SessionStorage.DatabaseOnDiskSizeKB` for
//     SessionStorage.
//
// The size histogram needs a blocking filesystem read. It is posted to a
// `base::ThreadPool` sequence to avoid blocking DOMStorage database
// operations.
void RecordOpenDatabaseHistograms(StorageType storage_type,
                                  DatabaseMetricsType metrics_type,
                                  const base::FilePath& database_path,
                                  bool is_sqlite,
                                  base::TimeTicks start_time,
                                  const DbStatus& status) {
  const std::string_view prefix = storage_type == StorageType::kLocalStorage
                                      ? "Storage.LocalStorage"
                                      : "Storage.SessionStorage";
  base::UmaHistogramTimes(base::StrCat({prefix, ".Duration.OpenDatabase2",
                                        GetHistogramSuffix(metrics_type)}),
                          base::TimeTicks::Now() - start_time);
  status.Log(base::StrCat({prefix, ".OpenDatabase"}), metrics_type);

  if (!database_path.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&RecordDatabaseOnDiskSizeKB, storage_type, database_path,
                       is_sqlite, metrics_type));
  }
}

}  // namespace

DomStorageDatabase::KeyValuePair::KeyValuePair() = default;

DomStorageDatabase::KeyValuePair::~KeyValuePair() = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(KeyValuePair&&) = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(const KeyValuePair&) = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(Key key, Value value)
    : key(std::move(key)), value(std::move(value)) {}

DomStorageDatabase::KeyValuePair& DomStorageDatabase::KeyValuePair::operator=(
    KeyValuePair&&) = default;

DomStorageDatabase::KeyValuePair& DomStorageDatabase::KeyValuePair::operator=(
    const KeyValuePair&) = default;

bool DomStorageDatabase::KeyValuePair::operator==(
    const KeyValuePair& rhs) const {
  return std::tie(key, value) == std::tie(rhs.key, rhs.value);
}

DomStorageDatabase::MapLocator::MapLocator(blink::StorageKey storage_key)
    : storage_key_(storage_key) {}

DomStorageDatabase::MapLocator::MapLocator(blink::StorageKey storage_key,
                                           int64_t map_id)
    : storage_key_(storage_key), map_id_(map_id) {}

DomStorageDatabase::MapLocator::MapLocator(std::string session_id,
                                           blink::StorageKey storage_key)
    : storage_key_(storage_key) {
  session_ids_.push_back(std::move(session_id));
}

DomStorageDatabase::MapLocator::MapLocator(std::string session_id,
                                           blink::StorageKey storage_key,
                                           int64_t map_id)
    : storage_key_(storage_key), map_id_(map_id) {
  session_ids_.push_back(std::move(session_id));
}

DomStorageDatabase::MapLocator::~MapLocator() = default;

DomStorageDatabase::MapLocator::MapLocator(MapLocator&&) = default;

DomStorageDatabase::MapLocator& DomStorageDatabase::MapLocator::operator=(
    MapLocator&&) = default;

const blink::StorageKey& DomStorageDatabase::MapLocator::storage_key() const {
  return storage_key_;
}

const std::vector<std::string>& DomStorageDatabase::MapLocator::session_ids()
    const {
  return session_ids_;
}

std::optional<int64_t> DomStorageDatabase::MapLocator::map_id() const {
  return map_id_;
}

void DomStorageDatabase::MapLocator::AddSession(std::string session_id) {
  session_ids_.push_back(std::move(session_id));
}

void DomStorageDatabase::MapLocator::RemoveSession(
    const std::string& session_id) {
  std::erase(session_ids_, session_id);
}

DomStorageDatabase::MapLocator DomStorageDatabase::MapLocator::Clone() const {
  MapLocator clone;
  clone.session_ids_ = session_ids_;
  clone.storage_key_ = storage_key_;
  clone.map_id_ = map_id_;
  return clone;
}

std::string DomStorageDatabase::MapLocator::ToDebugString() const {
  std::string sessions = base::JoinString(session_ids_, /*separator=*/":");
  std::string map_id = map_id_ ? base::NumberToString(*map_id_) : "null";

  return base::StringPrintf("sessions_ids:%s, storage_key:%s, map_id:%s",
                            sessions, storage_key_.GetDebugString(), map_id);
}

DomStorageDatabase::MapLocator::MapLocator() = default;

DomStorageDatabase::SharedMapLocator::SharedMapLocator(MapLocator source)
    : MapLocator(std::move(source)) {}

DomStorageDatabase::SharedMapLocator::~SharedMapLocator() = default;

DomStorageDatabase::Metadata::Metadata() = default;

DomStorageDatabase::Metadata::Metadata(
    std::vector<MapMetadata> source_map_metadata)
    : map_metadata(std::move(source_map_metadata)) {}

DomStorageDatabase::Metadata::~Metadata() = default;

DomStorageDatabase::Metadata::Metadata(Metadata&&) = default;

DomStorageDatabase::Metadata& DomStorageDatabase::Metadata::operator=(
    Metadata&&) = default;

DomStorageDatabase::MapBatchUpdate::MapBatchUpdate(MapLocator map_to_update)
    : map_locator{std::move(map_to_update)} {}

DomStorageDatabase::MapBatchUpdate::~MapBatchUpdate() = default;

DomStorageDatabase::MapBatchUpdate::MapBatchUpdate(MapBatchUpdate&&) = default;

DomStorageDatabase::MapBatchUpdate&
DomStorageDatabase::MapBatchUpdate::operator=(MapBatchUpdate&&) = default;

DomStorageDatabaseFactory::OpenResult::OpenResult() = default;
DomStorageDatabaseFactory::OpenResult::~OpenResult() = default;
DomStorageDatabaseFactory::OpenResult::OpenResult(OpenResult&&) = default;
DomStorageDatabaseFactory::OpenResult&
DomStorageDatabaseFactory::OpenResult::operator=(OpenResult&&) = default;

void DomStorageDatabaseFactory::OpenResult::SetDatabase(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<DomStorageDatabase> database) {
  CHECK(!database_);
  // Hold the database with a deleter that destroys it on `task_runner` (its
  // backend sequence) if this result is dropped before `TakeDatabase()` (e.g.
  // the open reply is dropped because its owner was torn down). Without this we
  // leak the opened database, which keeps its file handle open and prevents
  // Windows from deleting TempDirs created during test runs.
  //
  // Unlike a `SequenceBound`, a `std::unique_ptr` with an `OnTaskRunnerDeleter`
  // relies on `DeleteSoon`, which closes the database's file handle on drop
  // while fizzling the destruction if posted after thread pool shutdown. See
  // https://crbug.com/40746642.
  database_ = std::unique_ptr<DomStorageDatabase, base::OnTaskRunnerDeleter>(
      database.release(), base::OnTaskRunnerDeleter(std::move(task_runner)));
}

base::SequenceBound<std::unique_ptr<DomStorageDatabase>>
DomStorageDatabaseFactory::OpenResult::TakeDatabase() {
  CHECK(database_);
  // Re-bind the opened database to its backend sequence for the caller.
  // Adopting the pointer does not touch the database off-sequence.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      database_.get_deleter().task_runner_;
  return base::SequenceBound<std::unique_ptr<DomStorageDatabase>>(
      std::move(task_runner), base::WrapUnique(database_.release()));
}

// static
base::FilePath DomStorageDatabase::GetLevelDbPath(
    StorageType storage_type,
    const base::FilePath& storage_partition_dir) {
  CHECK(!storage_partition_dir.empty());
  CHECK(storage_partition_dir.IsAbsolute());
  switch (storage_type) {
    case StorageType::kLocalStorage:
      return storage_partition_dir.AppendASCII("Local Storage")
          .AppendASCII("leveldb");
    case StorageType::kSessionStorage:
      return storage_partition_dir.AppendASCII("Session Storage");
  }
  NOTREACHED();
}

// static
base::FilePath DomStorageDatabase::GetSqlitePath(
    StorageType storage_type,
    const base::FilePath& storage_partition_dir) {
  CHECK(!storage_partition_dir.empty());
  CHECK(storage_partition_dir.IsAbsolute());
  switch (storage_type) {
    case StorageType::kLocalStorage:
      return storage_partition_dir.AppendASCII("LocalStorage");
    case StorageType::kSessionStorage:
      return storage_partition_dir.AppendASCII("SessionStorage");
  }
  NOTREACHED();
}

// static
void DomStorageDatabaseFactory::InitializeDatabase(
    StorageType storage_type,
    bool is_sqlite,
    bool write_exp_tag,
    DatabaseMetricsType metrics_type,
    base::FilePath database_path,
    std::optional<base::trace_event::MemoryAllocatorDumpGuid> memory_dump_id,
    OpenResultCallback callback) {
  CHECK_EQ(database_path.empty(),
           metrics_type == DatabaseMetricsType::kInMemory);

  // For on-disk databases hop to the backend's blocking sequence, if needed.
  if (!database_path.empty()) {
    scoped_refptr<base::SequencedTaskRunner> runner =
        GetTaskRunnerForDb(database_path);
    if (!runner->RunsTasksInCurrentSequence()) {
      runner->PostTask(
          FROM_HERE,
          base::BindOnce(&DomStorageDatabaseFactory::InitializeDatabase,
                         storage_type, is_sqlite, write_exp_tag, metrics_type,
                         std::move(database_path), std::move(memory_dump_id),
                         std::move(callback)));
      return;
    }
  }

  std::unique_ptr<DomStorageDatabase> db;
  switch (storage_type) {
    case StorageType::kLocalStorage:
      if (is_sqlite) {
        db = std::make_unique<LocalStorageSqlite>(PassKey());
      } else {
        db = std::make_unique<LocalStorageLevelDB>(PassKey(), write_exp_tag);
      }
      break;
    case StorageType::kSessionStorage:
      if (is_sqlite) {
        db = std::make_unique<SessionStorageSqlite>(PassKey());
      } else {
        db = std::make_unique<SessionStorageLevelDB>(PassKey(), write_exp_tag);
      }
      break;
  }
  CHECK(db);

  const base::TimeTicks start_time = base::TimeTicks::Now();
  DbStatus open_status = db->Open(database_path, memory_dump_id);
  RecordOpenDatabaseHistograms(storage_type, metrics_type, database_path,
                               is_sqlite, start_time, open_status);

  // We are now on the backend's blocking sequence, so bind the database to it.
  OpenResult result;
  result.SetDatabase(base::SequencedTaskRunner::GetCurrentDefault(),
                     std::move(db));
  result.database_path = std::move(database_path);
  result.metrics_type = metrics_type;
  result.is_sqlite = is_sqlite;
  result.open_status = std::move(open_status);
  std::move(callback).Run(std::move(result));
}

// static
DomStorageDatabaseFactory::OpenCallback&
DomStorageDatabaseFactory::GetOpenCallback() {
  static base::NoDestructor<OpenCallback> callback(
      base::BindRepeating(&DomStorageDatabaseFactory::OpenImpl));
  return *callback;
}

// static
DomStorageDatabaseFactory::DestroyCallback&
DomStorageDatabaseFactory::GetDestroyCallback() {
  static base::NoDestructor<DestroyCallback> callback(
      base::BindRepeating(&DomStorageDatabaseFactory::DestroyImpl));
  return *callback;
}

// static
void DomStorageDatabaseFactory::Open(
    StorageType storage_type,
    const base::FilePath& storage_partition_dir,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    OpenResultCallback callback) {
  // Always reply on the caller's sequence, regardless of which sequence the
  // database was initialized on.
  GetOpenCallback().Run(
      storage_type, storage_partition_dir, memory_dump_id,
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

// static
void DomStorageDatabaseFactory::OpenImpl(
    StorageType storage_type,
    const base::FilePath& storage_partition_dir,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    OpenResultCallback callback) {
  // `storage_partition_dir` is empty for in-memory databases. No disk state
  // check needed.
  if (storage_partition_dir.empty()) {
    const bool is_sqlite = GetSqliteRolloutStage(/*in_memory=*/true) ==
                           DomStorageSqliteRolloutStage::kUseSqliteOnly;
    // For in-memory databases, `GetTaskRunnerForDb()` returns a fresh sequence
    // on each call, so we generate the runner once here and post to it.
    scoped_refptr<base::SequencedTaskRunner> runner =
        GetTaskRunnerForDb(base::FilePath());
    runner->PostTask(
        FROM_HERE,
        base::BindOnce(&DomStorageDatabaseFactory::InitializeDatabase,
                       storage_type, is_sqlite, /*write_exp_tag=*/false,
                       DatabaseMetricsType::kInMemory,
                       /*database_path=*/base::FilePath(), memory_dump_id,
                       std::move(callback)));
    return;
  }

  const DomStorageSqliteRolloutStage stage =
      GetSqliteRolloutStage(/*in_memory=*/false);

  // Non-experimental rollout stage: no disk check needed. Initialize directly
  // based on the rollout value.
  if (!IsExperimentalRolloutStage(stage)) {
    const bool is_sqlite =
        stage == DomStorageSqliteRolloutStage::kUseSqliteOnly;
    base::FilePath path = is_sqlite ? DomStorageDatabase::GetSqlitePath(
                                          storage_type, storage_partition_dir)
                                    : DomStorageDatabase::GetLevelDbPath(
                                          storage_type, storage_partition_dir);
    InitializeDatabase(storage_type, is_sqlite, /*write_exp_tag=*/false,
                       DatabaseMetricsType::kOnDisk, std::move(path),
                       memory_dump_id, std::move(callback));
    return;
  }

  // Experimental rollout stage: a single task on the LevelDB blocking
  // sequence inspects on-disk state, picks a backend, and then either
  // continues on that same sequence (LevelDB backend) or hops to the
  // SQLite blocking sequence.
  base::FilePath leveldb_path =
      DomStorageDatabase::GetLevelDbPath(storage_type, storage_partition_dir);
  scoped_refptr<base::SequencedTaskRunner> runner =
      GetTaskRunnerForDb(leveldb_path);
  runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DomStorageDatabaseFactory::CheckDiskStateAndInitializeDatabase,
          storage_type, storage_partition_dir, stage, memory_dump_id,
          std::move(callback)));
}

// static
void DomStorageDatabaseFactory::CheckDiskStateAndInitializeDatabase(
    StorageType storage_type,
    base::FilePath storage_partition_dir,
    DomStorageSqliteRolloutStage stage,
    std::optional<base::trace_event::MemoryAllocatorDumpGuid> memory_dump_id,
    OpenResultCallback callback) {
  const LevelDbOnDiskState leveldb_state =
      CheckOnDiskLevelDbState(storage_type, storage_partition_dir);
  const bool leveldb_exists = leveldb_state != LevelDbOnDiskState::kNone;
  const bool is_sqlite = ShouldUseSqlite(stage, leveldb_exists);
  const bool write_exp_tag = ShouldWriteExpTag(stage, leveldb_exists);
  const DatabaseMetricsType metrics_type = GetMetricsType(stage, leveldb_state);
  base::FilePath path = is_sqlite ? DomStorageDatabase::GetSqlitePath(
                                        storage_type, storage_partition_dir)
                                  : DomStorageDatabase::GetLevelDbPath(
                                        storage_type, storage_partition_dir);

  InitializeDatabase(storage_type, is_sqlite, write_exp_tag, metrics_type,
                     std::move(path), std::move(memory_dump_id),
                     std::move(callback));
}

// static
void DomStorageDatabaseFactory::Destroy(
    StorageType storage_type,
    const base::FilePath& storage_partition_dir,
    StatusCallback callback) {
  // Check which backend exists on disk, then destroy it in the callback.
  base::FilePath leveldb_path =
      DomStorageDatabase::GetLevelDbPath(storage_type, storage_partition_dir);
  scoped_refptr<base::SequencedTaskRunner> runner =
      GetTaskRunnerForDb(leveldb_path);
  runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CheckOnDiskLevelDbState, storage_type,
                     storage_partition_dir),
      base::BindOnce(&DomStorageDatabaseFactory::OnDestroyDiskStateChecked,
                     storage_type, storage_partition_dir, std::move(callback)));
}

// static
void DomStorageDatabaseFactory::OnDestroyDiskStateChecked(
    StorageType storage_type,
    const base::FilePath& storage_partition_dir,
    StatusCallback callback,
    LevelDbOnDiskState leveldb_state) {
  // If a LevelDB database exists, destroy it. Otherwise destroy the SQLite
  // path (which is a no-op if no SQLite database exists either).
  // TODO(crbug.com/377242771): When rolling out the LevelDB-to-SQLite
  // migration, remove any orphaned databases here.
  bool is_sqlite = (leveldb_state == LevelDbOnDiskState::kNone);
  base::FilePath path = is_sqlite ? DomStorageDatabase::GetSqlitePath(
                                        storage_type, storage_partition_dir)
                                  : DomStorageDatabase::GetLevelDbPath(
                                        storage_type, storage_partition_dir);
  GetDestroyCallback().Run(std::move(path), is_sqlite, std::move(callback));
}

// static
void DomStorageDatabaseFactory::DestroyImpl(const base::FilePath& database_path,
                                            bool is_sqlite,
                                            StatusCallback callback) {
  CHECK(!database_path.empty());
  CHECK(database_path.IsAbsolute());

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      GetTaskRunnerForDb(database_path);

  base::OnceCallback<DbStatus()> destroy_database_callback;
  if (is_sqlite) {
    destroy_database_callback =
        base::BindOnce(&sqlite::DestroyDatabase, database_path);
  } else {
    destroy_database_callback =
        base::BindOnce(&DomStorageDatabaseLevelDB::Destroy, database_path);
  }
  blocking_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(destroy_database_callback), std::move(callback));
}

base::PassKey<DomStorageDatabaseFactory>
DomStorageDatabaseFactory::CreatePassKeyForTesting() {
  return base::PassKey<DomStorageDatabaseFactory>();
}

scoped_refptr<base::SequencedTaskRunner> GetTaskRunnerForDb(
    const base::FilePath& database_path) {
  if (database_path.empty()) {
    // For the in-memory case, blocking shutdown is only important to avoid
    // leaking the SequenceBound on shutdown (and triggering ASAN failures).
    return base::ThreadPool::CreateSequencedTaskRunner(
        {base::WithBaseSyncPrimitives(),
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }

  //  This will always return the same task runner for a given `database_path`.
  return base::ThreadPool::CreateSequencedTaskRunnerForResource(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      database_path);
}

void ReportDatabaseMemoryUsage(
    sql::Database* database,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    base::trace_event::ProcessMemoryDump* pmd,
    std::string dump_name) {
  if (!database || !memory_dump_id) {
    return;
  }

  int memory_usage = database->GetMemoryUsage();
  if (memory_usage == 0) {
    return;
  }

  auto* db_dump = pmd->CreateAllocatorDump(dump_name);
  db_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                     base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                     memory_usage);
  auto* global_dump = pmd->CreateSharedGlobalAllocatorDump(*memory_dump_id);
  pmd->AddOwnershipEdge(global_dump->guid(), db_dump->guid());
  global_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                         base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                         memory_usage);
}

DbStatus PurgeOrigins(DomStorageDatabase& database,
                      std::set<url::Origin> origins) {
  ASSIGN_OR_RETURN(DomStorageDatabase::Metadata all_metadata,
                   database.ReadAllMetadata());

  std::vector<blink::StorageKey> metadata_to_delete;
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;

  for (const DomStorageDatabase::MapMetadata& metadata :
       all_metadata.map_metadata) {
    // Ideally we would be recording last_accessed instead, but there is no
    // historical data on that. Instead, we will use last_modified as a sanity
    // check against other data as we try to understand how many 'old' storage
    // buckets are still in use. This is split into two buckets for greater
    // resolution on near and far term ages.
    if (metadata.last_modified && *metadata.last_modified < base::Time::Now()) {
      const int days_since_last_modified =
          (base::Time::Now() - *metadata.last_modified).InDays();
      base::UmaHistogramCustomCounts("LocalStorage.DaysSinceLastModified",
                                     days_since_last_modified, 1,
                                     kLocalStorageStaleBucketCutoffInDays, 100);
    }

    const blink::StorageKey& storage_key = metadata.map_locator.storage_key();

    for (const url::Origin& origin : origins) {
      if (storage_key.origin() == origin ||
          (storage_key.IsThirdPartyContext() &&
           storage_key.top_level_site().IsSameSiteWith(origin))) {
        metadata_to_delete.push_back(storage_key);
        maps_to_delete.emplace_back(storage_key);
        break;
      }
    }
  }
  return database.DeleteStorageKeysFromSession(/*session_id=*/std::string(),
                                               std::move(metadata_to_delete),
                                               std::move(maps_to_delete));
}

DbStatus MigrateDatabase(DomStorageDatabase& source,
                         DomStorageDatabase& destination) {
  ASSIGN_OR_RETURN(DomStorageDatabase::Metadata source_metadata,
                   source.ReadAllMetadata());

  // Migrate each map in `source_metadata`.
  for (DomStorageDatabase::MapMetadata& source_map :
       source_metadata.map_metadata) {
    // Migrate the map's key/value pairs by reading all entries from
    // `source_map`.
    ASSIGN_OR_RETURN((std::map<DomStorageDatabase::Key,
                               DomStorageDatabase::Value> map_entries),
                     source.ReadMapKeyValues(source_map.map_locator.Clone()));

    // Then create a batch update to add all key/value pairs to `destination`.
    DomStorageDatabase::MapBatchUpdate update(source_map.map_locator.Clone());
    for (auto& [key, value] : map_entries) {
      update.entries_to_add.emplace_back(std::move(key), std::move(value));
    }

    // Migrate the map's usage metadata as  part of the batch update.
    bool has_access_metadata = source_map.last_accessed.has_value();
    bool has_write_metadata = source_map.last_modified && source_map.total_size;
    if (has_access_metadata || has_write_metadata) {
      DomStorageDatabase::MapBatchUpdate::Usage usage;
      if (has_access_metadata) {
        usage.SetLastAccessed(*source_map.last_accessed);
      }
      if (has_write_metadata) {
        usage.SetLastModifiedAndTotalSize(*source_map.last_modified,
                                          *source_map.total_size);
      }
      update.map_usage = std::move(usage);
    } else {
      // When no usage metadata exists, write the metadata separately to
      // associate this map's session IDs and storage key with its map ID.
      DomStorageDatabase::Metadata metadata_to_write;
      metadata_to_write.map_metadata.push_back(std::move(source_map));
      DB_RETURN_IF_ERROR(destination.PutMetadata(std::move(metadata_to_write)));
    }

    // Commit the batch update for `destination`, containing the key/value pairs
    // and optional usage metadata.
    std::vector<DomStorageDatabase::MapBatchUpdate> updates;
    updates.push_back(std::move(update));
    DB_RETURN_IF_ERROR(destination.UpdateMaps(std::move(updates)));
  }
  return DbStatus::OK();
}

}  // namespace storage
