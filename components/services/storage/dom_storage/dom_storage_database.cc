// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/services/storage/dom_storage/dom_storage_database.h"

#include <algorithm>
#include <utility>

#include "base/debug/leak_annotations.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

#if BUILDFLAG(IS_COBALT)
#include "base/feature_list.h"
#include "base/features.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#endif  // BUILDFLAG(IS_COBALT)

namespace storage {

namespace {

// IOError message returned whenever a call is made on a DomStorageDatabase
// which has been invalidated (e.g. by a failed |RewriteDB()| operation).
const char kInvalidDatabaseMessage[] = "DomStorageDatabase no longer valid.";

class DomStorageDatabaseEnv : public leveldb_env::ChromiumEnv {
 public:
  DomStorageDatabaseEnv() : ChromiumEnv(CreateFilesystemProxy()) {}

  DomStorageDatabaseEnv(const DomStorageDatabaseEnv&) = delete;
  DomStorageDatabaseEnv& operator=(const DomStorageDatabaseEnv&) = delete;
};

std::string MakeFullPersistentDBName(const base::FilePath& directory,
                                     const std::string& db_name) {
  // ChromiumEnv treats DB name strings as UTF-8 file paths.
  return directory.Append(base::FilePath::FromUTF8Unsafe(db_name))
      .AsUTF8Unsafe();
}

// Used for disk DBs.
#if BUILDFLAG(IS_COBALT)
leveldb_env::Options MakeOptions(bool is_local_storage = false) {
#else
leveldb_env::Options MakeOptions() {
#endif
  leveldb_env::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // use minimum
  // Default write_buffer_size is 4 MB but that might leave a 3.999
  // memory allocation in RAM from a log file recovery.
  options.write_buffer_size = 64 * 1024;
#if BUILDFLAG(IS_COBALT)
  if (is_local_storage &&
      base::FeatureList::IsEnabled(
          base::features::kCobaltLocalStorageReuseLogsAndSkipEmptyCommits)) {
    // On low-end TV devices, leveldb_env::Options defaults `reuse_logs` to
    // false (forcing a synchronous .log -> .ldb compaction, MANIFEST rewrite,
    // and CURRENT rename on every DB::Open). Reusing the WAL on open avoids
    // cold-start compaction churn while keeping every 1s commit synchronous
    // (`sync = true`).
    options.reuse_logs = true;
  }
#endif

  // We disable caching because all reads are one-offs such as in
  // `LocalStorageImpl::OnDatabaseOpened()`, or they are bulk scans (as in
  // `ForEachWithPrefix`). In the case of bulk scans, they're either for
  // deletion (where caching doesn't make sense) or a mass-read, which we cache
  // in memory.
  options.block_cache = leveldb_chrome::GetSharedInMemoryBlockCache();

  static base::NoDestructor<DomStorageDatabaseEnv> env;
  options.env = env.get();
  return options;
}

#if BUILDFLAG(IS_COBALT)
leveldb::WriteOptions CreateSyncWriteOptions() {
  leveldb::WriteOptions options;
  options.sync = true;
  return options;
}

// Returns true if the Local Storage LevelDB directory contains surviving table
// (.ldb/.sst), log (.log), or manifest (MANIFEST-*) files, but lacks a valid
// CURRENT pointer file and does not have an interrupted RewriteDB() backup
// (`__tmp_for_rebuild`, which `leveldb_env::OpenDB()` restores atomically).
// Without intervention, upstream `DB::Open(create_if_missing=true)` treats a
// missing CURRENT as an empty DB, calls `NewDB()`, and deletes all existing SST
// files in `RemoveObsoleteFiles()`, silently wiping user data.
bool HasExistingLevelDBFilesWhenCurrentMissing(
    const leveldb_env::Options& options,
    const std::string& name) {
  if (name.empty() || !options.env || leveldb_chrome::IsMemEnv(options.env)) {
    return false;
  }
  base::FilePath db_path = base::FilePath::FromUTF8Unsafe(name);
  const std::string db_path_utf8 = db_path.AsUTF8Unsafe();
  if (db_path.BaseName() != base::FilePath(FILE_PATH_LITERAL("leveldb")) ||
      options.env->FileExists(db_path.AppendASCII("CURRENT").AsUTF8Unsafe()) ||
      !options.env->FileExists(db_path_utf8) ||
      options.env->FileExists(
          leveldb_env::DatabaseNameForRewriteDB(db_path_utf8))) {
    return false;
  }
  std::vector<std::string> children;
  if (!options.env->GetChildren(db_path_utf8, &children).ok()) {
    return false;
  }
  for (const std::string& child : children) {
    if (base::EndsWith(child, ".ldb") || base::EndsWith(child, ".sst") ||
        base::EndsWith(child, ".log") || base::StartsWith(child, "MANIFEST-")) {
      return true;
    }
  }
  return false;
}

// Removes the <dbname>/lost archive directory created by leveldb::RepairDB()
// after verifying repair succeeded, so converted .log and old MANIFEST files
// do not permanently leak USER_DATA disk space.
void RemoveRepairLostDirectory(const leveldb_env::Options& options,
                               const std::string& name) {
  if (name.empty() || !options.env) {
    return;
  }
  const std::string lost_dir =
      base::FilePath::FromUTF8Unsafe(name).AppendASCII("lost").AsUTF8Unsafe();
  if (!options.env->FileExists(lost_dir)) {
    return;
  }
  std::vector<std::string> children;
  if (options.env->GetChildren(lost_dir, &children).ok()) {
    for (const std::string& child : children) {
      if (child != "." && child != "..") {
        options.env->RemoveFile(
            base::FilePath::FromUTF8Unsafe(lost_dir)
                .Append(base::FilePath::FromUTF8Unsafe(child))
                .AsUTF8Unsafe());
      }
    }
  }
  options.env->RemoveDir(lost_dir);
}

// Restores files archived into <dbname>/lost/ back into <dbname>/ when
// leveldb::RepairDB() fails (e.g. ENOSPC or EACCES/EIO during
// ConvertLogToTable() when reuse_logs = true keeps all recent writes in .log),
// ensuring surviving WAL/SST files are neither deleted nor stranded in lost/.
void RestoreRepairLostDirectory(const leveldb_env::Options& options,
                                const std::string& name) {
  if (name.empty() || !options.env) {
    return;
  }
  const base::FilePath db_dir = base::FilePath::FromUTF8Unsafe(name);
  const base::FilePath lost_path = db_dir.AppendASCII("lost");
  const std::string lost_dir = lost_path.AsUTF8Unsafe();
  if (!options.env->FileExists(lost_dir)) {
    return;
  }
  std::vector<std::string> children;
  if (options.env->GetChildren(lost_dir, &children).ok()) {
    for (const std::string& child : children) {
      if (child == "." || child == "..") {
        continue;
      }
      const std::string archived_file =
          lost_path.Append(base::FilePath::FromUTF8Unsafe(child))
              .AsUTF8Unsafe();
      const std::string original_file =
          db_dir.Append(base::FilePath::FromUTF8Unsafe(child)).AsUTF8Unsafe();
      if (options.env->FileExists(original_file)) {
        options.env->RemoveFile(original_file);
      }
      options.env->RenameFile(archived_file, original_file);
    }
  }
  options.env->RemoveDir(lost_dir);
}

// Upstream Repairer::ConvertLogFilesToTables() (repair.cc:134-140) logs and
// ignores I/O errors from ConvertLogToTable() before moving .log into lost/.
// Track file read/write I/O errors during RepairDB() so failed WAL conversion
// is detected and rolled back rather than silently archiving/deleting .log.
class RepairIOErrorTrackingEnv : public leveldb::EnvWrapper {
 public:
  explicit RepairIOErrorTrackingEnv(leveldb::Env* target)
      : leveldb::EnvWrapper(target) {}

  const leveldb::Status& io_error() const { return first_io_error_; }

  leveldb::Status NewSequentialFile(const std::string& f,
                                    leveldb::SequentialFile** r) override {
    leveldb::SequentialFile* raw = nullptr;
    leveldb::Status s = target()->NewSequentialFile(f, &raw);
    if (!s.ok()) {
      RecordError(s);
      *r = nullptr;
      return s;
    }
    *r = new TrackingSequentialFile(raw, this);
    return s;
  }

  leveldb::Status NewRandomAccessFile(const std::string& f,
                                      leveldb::RandomAccessFile** r) override {
    leveldb::Status s = target()->NewRandomAccessFile(f, r);
    if (!s.ok()) {
      leveldb_env::MethodID method = leveldb_env::kNumEntries;
      base::File::Error bfe = base::File::FILE_OK;
      const bool is_ldb_probe_miss =
          base::EndsWith(f, ".ldb") &&
          leveldb_env::ParseMethodAndError(s, &method, &bfe) ==
              leveldb_env::METHOD_AND_BFE &&
          bfe == base::File::FILE_ERROR_NOT_FOUND;
      if (!is_ldb_probe_miss) {
        RecordError(s);
      }
    }
    return s;
  }

  leveldb::Status NewWritableFile(const std::string& f,
                                  leveldb::WritableFile** r) override {
    leveldb::WritableFile* raw = nullptr;
    leveldb::Status s = target()->NewWritableFile(f, &raw);
    if (!s.ok()) {
      RecordError(s);
      *r = nullptr;
      return s;
    }
    *r = new TrackingWritableFile(raw, this);
    return s;
  }

 private:
  class TrackingSequentialFile : public leveldb::SequentialFile {
   public:
    TrackingSequentialFile(leveldb::SequentialFile* raw,
                           RepairIOErrorTrackingEnv* owner)
        : raw_(raw), owner_(owner) {}
    ~TrackingSequentialFile() override = default;
    leveldb::Status Read(size_t n,
                         leveldb::Slice* result,
                         char* scratch) override {
      leveldb::Status s = raw_->Read(n, result, scratch);
      if (!s.ok() && s.IsIOError()) {
        owner_->RecordError(s);
      }
      return s;
    }
    leveldb::Status Skip(uint64_t n) override {
      leveldb::Status s = raw_->Skip(n);
      if (!s.ok() && s.IsIOError()) {
        owner_->RecordError(s);
      }
      return s;
    }

   private:
    std::unique_ptr<leveldb::SequentialFile> raw_;
    raw_ptr<RepairIOErrorTrackingEnv> owner_;
  };

  class TrackingWritableFile : public leveldb::WritableFile {
   public:
    TrackingWritableFile(leveldb::WritableFile* raw,
                         RepairIOErrorTrackingEnv* owner)
        : raw_(raw), owner_(owner) {}
    ~TrackingWritableFile() override = default;
    leveldb::Status Append(const leveldb::Slice& data) override {
      leveldb::Status s = raw_->Append(data);
      if (!s.ok()) {
        owner_->RecordError(s);
      }
      return s;
    }
    leveldb::Status Close() override {
      leveldb::Status s = raw_->Close();
      if (!s.ok()) {
        owner_->RecordError(s);
      }
      return s;
    }
    leveldb::Status Flush() override {
      leveldb::Status s = raw_->Flush();
      if (!s.ok()) {
        owner_->RecordError(s);
      }
      return s;
    }
    leveldb::Status Sync() override {
      leveldb::Status s = raw_->Sync();
      if (!s.ok()) {
        owner_->RecordError(s);
      }
      return s;
    }

   private:
    std::unique_ptr<leveldb::WritableFile> raw_;
    raw_ptr<RepairIOErrorTrackingEnv> owner_;
  };

  void RecordError(const leveldb::Status& s) {
    if (first_io_error_.ok() && !s.ok()) {
      first_io_error_ = s;
    }
  }

  leveldb::Status first_io_error_;
};

// Unlike DB::Open() and DestroyDB(), upstream leveldb::RepairDB() does not
// acquire <dbname>/LOCK. Acquire LOCK before repairing and only clean up
// `lost/` when RepairDB() fully succeeds without I/O errors.
leveldb::Status RepairDBWithLock(const leveldb_env::Options& options,
                                 const std::string& name) {
  if (name.empty() || !options.env) {
    return leveldb::Status::InvalidArgument("Invalid DB options or name");
  }
  const base::FilePath db_path = base::FilePath::FromUTF8Unsafe(name);
  const std::string lock_path = db_path.AppendASCII("LOCK").AsUTF8Unsafe();
  const std::string current_path =
      db_path.AppendASCII("CURRENT").AsUTF8Unsafe();
  const std::string manifest_1_path =
      db_path.AppendASCII("MANIFEST-000001").AsUTF8Unsafe();
  leveldb::FileLock* lock = nullptr;
  leveldb::Status lock_status = options.env->LockFile(lock_path, &lock);
  if (!lock_status.ok()) {
    return lock_status;
  }

  const bool current_existed_before = options.env->FileExists(current_path);
  const bool manifest_1_existed_before =
      options.env->FileExists(manifest_1_path);

  RepairIOErrorTrackingEnv tracking_env(options.env);
  leveldb_env::Options repair_options = options;
  repair_options.env = &tracking_env;
  leveldb::Status repair_status = leveldb::RepairDB(name, repair_options);
  const bool current_exists_after = options.env->FileExists(current_path);

  leveldb::Status final_status = repair_status;
  if (final_status.ok() && !tracking_env.io_error().ok()) {
    final_status = tracking_env.io_error();
  }
  if (final_status.ok() && !current_exists_after) {
    final_status =
        leveldb::Status::Corruption("RepairDB did not reconstruct CURRENT");
  }

  if (final_status.ok()) {
    RemoveRepairLostDirectory(options, name);
  } else {
    if (!current_existed_before && current_exists_after) {
      options.env->RemoveFile(current_path);
    }
    if (!manifest_1_existed_before &&
        options.env->FileExists(manifest_1_path) &&
        !options.env->FileExists(current_path)) {
      options.env->RemoveFile(manifest_1_path);
    }
    RestoreRepairLostDirectory(options, name);
  }
  options.env->UnlockFile(lock);
  return final_status;
}

bool ShouldAttemptRepairOnOpenError(const leveldb::Status& status) {
  if (status.IsCorruption()) {
    return true;
  }
  // ChromiumEnv::NewSequentialFile maps base::File::FILE_ERROR_NOT_FOUND to
  // Status::IOError rather than Status::NotFound (crbug.com/760362), so when
  // CURRENT points to a missing MANIFEST-* or .log file, VersionSet::Recover()
  // returns an IOError wrapping kNewSequentialFile + FILE_ERROR_NOT_FOUND.
  leveldb_env::MethodID method = leveldb_env::kNumEntries;
  base::File::Error error = base::File::FILE_OK;
  return leveldb_env::ParseMethodAndError(status, &method, &error) ==
             leveldb_env::METHOD_AND_BFE &&
         method == leveldb_env::kNewSequentialFile &&
         error == base::File::FILE_ERROR_NOT_FOUND;
}

leveldb::Status OpenOrRepairLocalStorageDB(const leveldb_env::Options& options,
                                           const std::string& name,
                                           bool is_local_storage_disk_db,
                                           std::unique_ptr<leveldb::DB>* db) {
  const bool should_attempt_error_recovery =
      is_local_storage_disk_db &&
      base::FeatureList::IsEnabled(
          base::features::kCobaltLocalStorageErrorRecovery);
  if (is_local_storage_disk_db &&
      HasExistingLevelDBFilesWhenCurrentMissing(options, name)) {
    if (should_attempt_error_recovery) {
      // Reconstruct MANIFEST and CURRENT from existing .ldb/.log metadata in
      // place before OpenDB(create_if_missing=true) calls NewDB() and
      // RemoveObsoleteFiles() deletes all existing table files. If repair
      // fails (e.g. ENOSPC or LOCK contention), abort immediately instead of
      // falling through to OpenDB()'s silent NewDB() wipe.
      leveldb::Status repair_status = RepairDBWithLock(options, name);
      if (!repair_status.ok()) {
        base::UmaHistogramEnumeration(
            "Cobalt.LocalStorage.RecoveryEvent",
            LocalStorageRecoveryEvent::kRepairMissingCurrentFailed);
        return repair_status;
      }
      leveldb::Status open_status = leveldb_env::OpenDB(options, name, db);
      base::UmaHistogramEnumeration(
          "Cobalt.LocalStorage.RecoveryEvent",
          open_status.ok()
              ? LocalStorageRecoveryEvent::kRepairMissingCurrentSucceeded
              : LocalStorageRecoveryEvent::kRepairMissingCurrentFailed);
      return open_status;
    }
    // Without error recovery, OpenDB(create_if_missing=true) calls NewDB() and
    // RemoveObsoleteFiles() deletes all existing .ldb tables while returning
    // Status::OK(). Record DatabaseOpenError so this silent data wipe is
    // observable in UMA.
    base::UmaHistogramEnumeration("Cobalt.LocalStorage.DatabaseOpenError",
                                  leveldb_env::LEVELDB_STATUS_CORRUPTION,
                                  leveldb_env::LEVELDB_STATUS_MAX);
  }

  leveldb::Status status = leveldb_env::OpenDB(options, name, db);
  if (should_attempt_error_recovery && ShouldAttemptRepairOnOpenError(status)) {
    leveldb::Status repair_status = RepairDBWithLock(options, name);
    if (repair_status.ok()) {
      status = leveldb_env::OpenDB(options, name, db);
      base::UmaHistogramEnumeration(
          "Cobalt.LocalStorage.RecoveryEvent",
          status.ok()
              ? LocalStorageRecoveryEvent::kRepairOpenCorruptionSucceeded
              : LocalStorageRecoveryEvent::kRepairOpenCorruptionFailed);
    } else {
      base::UmaHistogramEnumeration(
          "Cobalt.LocalStorage.RecoveryEvent",
          LocalStorageRecoveryEvent::kRepairOpenCorruptionFailed);
      if (IsNonDestructiveLocalStorageOpenFailure(repair_status)) {
        status = repair_status;
      }
    }
  }
  return status;
}
#endif

std::unique_ptr<leveldb::DB> TryOpenDB(
    const leveldb_env::Options& options,
    const std::string& name,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    DomStorageDatabase::StatusCallback callback) {
  std::unique_ptr<leveldb::DB> db;
#if BUILDFLAG(IS_COBALT)
  const bool is_local_storage_disk_db =
      !name.empty() && options.env && !leveldb_chrome::IsMemEnv(options.env) &&
      base::FilePath::FromUTF8Unsafe(name).BaseName() ==
          base::FilePath(FILE_PATH_LITERAL("leveldb"));
  leveldb::Status status =
      OpenOrRepairLocalStorageDB(options, name, is_local_storage_disk_db, &db);
#else
  leveldb::Status status = leveldb_env::OpenDB(options, name, &db);
#endif  // BUILDFLAG(IS_COBALT)
  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), status));
  return db;
}

leveldb::Slice MakeSlice(base::span<const uint8_t> data) {
  if (data.empty())
    return leveldb::Slice();
  return leveldb::Slice(reinterpret_cast<const char*>(data.data()),
                        data.size());
}

DomStorageDatabase::KeyValuePair MakeKeyValuePair(const leveldb::Slice& key,
                                                  const leveldb::Slice& value) {
  base::span key_span(key);
  base::span value_span(value);
  return DomStorageDatabase::KeyValuePair(
      DomStorageDatabase::Key(key_span.begin(), key_span.end()),
      DomStorageDatabase::Value(value_span.begin(), value_span.end()));
}

template <typename Func>
DomStorageDatabase::Status ForEachWithPrefix(leveldb::DB* db,
                                             DomStorageDatabase::KeyView prefix,
                                             Func function) {
  std::unique_ptr<leveldb::Iterator> iter(
      db->NewIterator(leveldb::ReadOptions()));
  const leveldb::Slice prefix_slice(MakeSlice(prefix));
  iter->Seek(prefix_slice);
  for (; iter->Valid(); iter->Next()) {
    if (!iter->key().starts_with(prefix_slice))
      break;
    function(iter->key(), iter->value());
  }
  return iter->status();
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

DomStorageDatabase::DomStorageDatabase(
    PassKey,
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    StatusCallback callback)
    : name_(MakeFullPersistentDBName(directory, name)),
#if BUILDFLAG(IS_COBALT)
      // `DomStorageDatabase` backs both persistent `Local Storage`
      // (`name == "leveldb"`, see `kLocalStorageLeveldbName` in
      // `local_storage_impl.cc`) and persistent `Session Storage`
      // (`name == "Session Storage"`, see `kSessionStorageDirectory` in
      // `session_storage_impl.cc`). Only `Local Storage` should enable
      // `reuse_logs = true` and `LocalStorage.CommitDuration` telemetry.
      options_(MakeOptions(name == "leveldb")),
#else
      options_(MakeOptions()),
#endif
      memory_dump_id_(memory_dump_id)
#if BUILDFLAG(IS_COBALT)
      ,
      is_local_storage_(name == "leveldb")
#endif  // BUILDFLAG(IS_COBALT)
{
  Init(std::move(callback_task_runner), std::move(callback));
}

DomStorageDatabase::DomStorageDatabase(
    PassKey,
    const std::string& tracking_name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    StatusCallback callback)
    : env_(leveldb_chrome::NewMemEnv(tracking_name)),
      memory_dump_id_(memory_dump_id) {
  options_.env = env_.get();
  Init(std::move(callback_task_runner), std::move(callback));
}

void DomStorageDatabase::Init(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    StatusCallback callback) {
  db_ = TryOpenDB(options_, name_, std::move(callback_task_runner),
                  std::move(callback));
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "MojoLevelDB", base::SequencedTaskRunner::GetCurrentDefault(),
          MemoryDumpProvider::Options());
}

template <typename... Args>
void DomStorageDatabase::CreateSequenceBoundDomStorageDatabase(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    DomStorageDatabase::OpenCallback callback,
    Args&&... args) {
  auto database = std::make_unique<base::SequenceBound<DomStorageDatabase>>();

  // Subtle: We bind `database` as an unmanaged pointer during the async opening
  // operation so that it leaks in case the bound callback below never gets a
  // chance to run (because scheduler shutdown happens first).
  //
  // This is because the callback below is posted to
  // SequencedTaskRunner::GetCurrentDefault(), which may not itself be
  // shutdown-blocking; so if shutdown completes before the task runs, the
  // callback below is destroyed along with any of its owned arguments.
  // Meanwhile, SequenceBound destruction posts a task to its bound TaskRunner,
  // which in this case is one which runs shutdown-blocking tasks.
  //
  // The net result of all of this is that if the SequenceBound were an owned
  // argument, it might attempt to post a shutdown-blocking task after shutdown
  // has completed, which is not allowed and will DCHECK. Leaving the object
  // temporarily unmanaged during this window of potential failure avoids such a
  // DCHECK, and if shutdown does not happen during that window, the object's
  // ownership will finally be left to the caller's discretion.
  //
  // See https://crbug.com/1174179.
  auto* database_ptr = database.release();
  ANNOTATE_LEAKING_OBJECT_PTR(database_ptr);
  *database_ptr = base::SequenceBound<DomStorageDatabase>(
      blocking_task_runner, PassKey(), args...,
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          [](base::SequenceBound<DomStorageDatabase>* database_ptr,
             DomStorageDatabase::OpenCallback callback,
             leveldb::Status status) {
            auto database = base::WrapUnique(database_ptr);
            if (status.ok())
              std::move(callback).Run(std::move(*database), status);
            else
              std::move(callback).Run({}, status);
          },
          database_ptr, std::move(callback)));
}

DomStorageDatabase::~DomStorageDatabase() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  if (destruction_callback_)
    std::move(destruction_callback_).Run();
}

// static
void DomStorageDatabase::OpenDirectory(
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OpenCallback callback) {
  DCHECK(directory.IsAbsolute());
  CreateSequenceBoundDomStorageDatabase(std::move(blocking_task_runner),
                                        std::move(callback), directory, name,
                                        memory_dump_id);
}

// static
void DomStorageDatabase::OpenInMemory(
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OpenCallback callback) {
  CreateSequenceBoundDomStorageDatabase(std::move(blocking_task_runner),
                                        std::move(callback), name,
                                        memory_dump_id);
}

// static
void DomStorageDatabase::Destroy(
    const base::FilePath& directory,
    const std::string& name,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    StatusCallback callback) {
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const std::string& db_name,
             scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
             StatusCallback callback) {
            callback_task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               leveldb::DestroyDB(db_name, MakeOptions())));
          },
          MakeFullPersistentDBName(directory, name),
          base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback)));
}

DomStorageDatabase::Status DomStorageDatabase::Get(KeyView key,
                                                   Value* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
#if BUILDFLAG(IS_COBALT)
  if (fail_next_get_with_io_error_for_testing_) {
    fail_next_get_with_io_error_for_testing_ = false;
    return Status::IOError("Simulated Read I/O Error");
  }
#endif
  std::string value;
  Status status = db_->Get(leveldb::ReadOptions(), MakeSlice(key), &value);
  *out_value = Value(value.begin(), value.end());
  return status;
}

DomStorageDatabase::Status DomStorageDatabase::Put(KeyView key,
                                                   ValueView value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
#if BUILDFLAG(IS_COBALT)
  return db_->Put(CreateSyncWriteOptions(), MakeSlice(key), MakeSlice(value));
#else
  return db_->Put(leveldb::WriteOptions(), MakeSlice(key), MakeSlice(value));
#endif
}

DomStorageDatabase::Status DomStorageDatabase::GetPrefixed(
    KeyView prefix,
    std::vector<KeyValuePair>* entries) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  return ForEachWithPrefix(
      db_.get(), prefix,
      [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        entries->push_back(MakeKeyValuePair(key, value));
      });
}

DomStorageDatabase::Status DomStorageDatabase::DeletePrefixed(
    KeyView prefix,
    leveldb::WriteBatch* batch) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  Status status = ForEachWithPrefix(
      db_.get(), prefix,
      [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        batch->Delete(key);
      });
  return status;
}

DomStorageDatabase::Status DomStorageDatabase::CopyPrefixed(
    KeyView prefix,
    KeyView new_prefix,
    leveldb::WriteBatch* batch) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  Key new_key(new_prefix.begin(), new_prefix.end());
  Status status = ForEachWithPrefix(
      db_.get(), prefix,
      [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        DCHECK_GE(key.size(), prefix.size());  // By definition.
        size_t suffix_length = key.size() - prefix.size();
        new_key.resize(new_prefix.size() + suffix_length);
        std::copy(key.data() + prefix.size(), key.data() + key.size(),
                  new_key.begin() + new_prefix.size());
        batch->Put(MakeSlice(new_key), value);
      });
  return status;
}

DomStorageDatabase::Status DomStorageDatabase::Commit(
    leveldb::WriteBatch* batch) const {
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
#if BUILDFLAG(IS_COBALT)
  if (fail_commits_with_corruption_for_testing_) {
    return Status::Corruption("Simulated Corruption");
  }
  if (remaining_commit_io_errors_for_testing_ > 0) {
    --remaining_commit_io_errors_for_testing_;
    return Status::IOError("Simulated Transient I/O Error");
  }
#endif
  if (fail_commits_for_testing_)
    return Status::IOError("Simulated I/O Error");
#if BUILDFLAG(IS_COBALT)
  static const size_t kEmptyWriteBatchSize =
      leveldb::WriteBatch().ApproximateSize();
  if (is_local_storage_ &&
      base::FeatureList::IsEnabled(
          base::features::kCobaltLocalStorageReuseLogsAndSkipEmptyCommits) &&
      batch->ApproximateSize() <= kEmptyWriteBatchSize) {
    return Status::OK();
  }
  base::ElapsedTimer commit_timer;
  Status status = db_->Write(CreateSyncWriteOptions(), batch);
  // Only record commit latency for successful persistent on-disk LocalStorage
  // LevelDB commits (excluding failed commits rejected in RAM by latched
  // bg_error_, in-memory DBs, and on-disk SessionStorage DBs).
  if (status.ok() && !env_ && is_local_storage_) {
    base::UmaHistogramTimes("Cobalt.Storage.LocalStorage.CommitDuration",
                            commit_timer.Elapsed());
  }
  ++total_commit_count_for_testing_;
  return status;
#else
  return db_->Write(leveldb::WriteOptions(), batch);
#endif
}

DomStorageDatabase::Status DomStorageDatabase::RewriteDB() {
  if (!db_)
    return Status::IOError(kInvalidDatabaseMessage);
  Status status = leveldb_env::RewriteDB(options_, name_, &db_);
  if (!status.ok())
    db_.reset();
  return status;
}

#if BUILDFLAG(IS_COBALT)
bool IsNonDestructiveLocalStorageOpenFailure(const leveldb::Status& status) {
  if (status.ok() || status.IsCorruption() || status.IsInvalidArgument() ||
      status.IsNotSupportedError()) {
    return false;
  }
  return !ShouldAttemptRepairOnOpenError(status);
}

DomStorageDatabase::Status DomStorageDatabase::Reopen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (env_ || name_.empty()) {
    return Status::NotSupported("Cannot reopen in-memory database.");
  }
  ++reopen_count_for_testing_;
  db_.reset();
  return OpenOrRepairLocalStorageDB(options_, name_, is_local_storage_, &db_);
}
#endif  // BUILDFLAG(IS_COBALT)

bool DomStorageDatabase::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto* dump = leveldb_env::DBTracker::GetOrCreateAllocatorDump(pmd, db_.get());
  if (!dump)
    return true;
  auto* global_dump = pmd->CreateSharedGlobalAllocatorDump(*memory_dump_id_);
  pmd->AddOwnershipEdge(global_dump->guid(), dump->guid());
  // Add size to global dump to propagate the size of the database to the
  // client's dump.
  global_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                         base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                         dump->GetSizeInternal());
  return true;
}

}  // namespace storage
