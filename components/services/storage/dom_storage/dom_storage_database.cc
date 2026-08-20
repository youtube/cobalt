// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_database.h"

#include "components/services/storage/dom_storage/dom_storage_database_leveldb.h"

namespace storage {

<<<<<<< HEAD
=======
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
leveldb_env::Options MakeOptions() {
  leveldb_env::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // use minimum
  // Default write_buffer_size is 4 MB but that might leave a 3.999
  // memory allocation in RAM from a log file recovery.
  options.write_buffer_size = 64 * 1024;

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
#endif

std::unique_ptr<leveldb::DB> TryOpenDB(
    const leveldb_env::Options& options,
    const std::string& name,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    DomStorageDatabase::StatusCallback callback) {
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status = leveldb_env::OpenDB(options, name, &db);
  callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), FromLevelDBStatus(status)));
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
DbStatus ForEachWithPrefix(leveldb::DB* db,
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
  return FromLevelDBStatus(iter->status());
}

}  // namespace

>>>>>>> parent of 7d60f81f606 (CONFLICTED Chromium Cherry pick: Revert Cobalt.)
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

// static
void DomStorageDatabaseFactory::OpenDirectory(
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OpenCallback callback) {
  DomStorageDatabaseLevelDB::OpenDirectory(directory, name, memory_dump_id,
                                           std::move(blocking_task_runner),
                                           std::move(callback));
}

// static
void DomStorageDatabaseFactory::OpenInMemory(
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OpenCallback callback) {
  DomStorageDatabaseLevelDB::OpenInMemory(name, memory_dump_id,
                                          std::move(blocking_task_runner),
                                          std::move(callback));
}

// static
void DomStorageDatabaseFactory::Destroy(
    const base::FilePath& directory,
    const std::string& name,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
<<<<<<< HEAD
    base::OnceCallback<void(DbStatus)> callback) {
  DomStorageDatabaseLevelDB::Destroy(
      directory, name, std::move(blocking_task_runner), std::move(callback));
=======
    StatusCallback callback) {
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const std::string& db_name,
             scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
             StatusCallback callback) {
            callback_task_runner->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback),
                                          FromLevelDBStatus(leveldb::DestroyDB(
                                              db_name, MakeOptions()))));
          },
          MakeFullPersistentDBName(directory, name),
          base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback)));
}

DbStatus DomStorageDatabase::Get(KeyView key, Value* out_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return DbStatus::IOError(kInvalidDatabaseMessage);
  std::string value;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), MakeSlice(key), &value);
  *out_value = Value(value.begin(), value.end());
  return FromLevelDBStatus(status);
}

DbStatus DomStorageDatabase::Put(KeyView key, ValueView value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return DbStatus::IOError(kInvalidDatabaseMessage);
#if BUILDFLAG(IS_COBALT)
  return FromLevelDBStatus(
      db_->Put(CreateSyncWriteOptions(), MakeSlice(key), MakeSlice(value)));
#else
  return FromLevelDBStatus(
      db_->Put(leveldb::WriteOptions(), MakeSlice(key), MakeSlice(value)));
#endif
}

DbStatus DomStorageDatabase::GetPrefixed(
    KeyView prefix,
    std::vector<KeyValuePair>* entries) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return DbStatus::IOError(kInvalidDatabaseMessage);
  return ForEachWithPrefix(
      db_.get(), prefix,
      [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        entries->push_back(MakeKeyValuePair(key, value));
      });
}

DbStatus DomStorageDatabase::DeletePrefixed(KeyView prefix,
                                            leveldb::WriteBatch* batch) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return DbStatus::IOError(kInvalidDatabaseMessage);
  DbStatus status = ForEachWithPrefix(
      db_.get(), prefix,
      [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        batch->Delete(key);
      });
  return status;
}

DbStatus DomStorageDatabase::CopyPrefixed(KeyView prefix,
                                          KeyView new_prefix,
                                          leveldb::WriteBatch* batch) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return DbStatus::IOError(kInvalidDatabaseMessage);
  Key new_key(new_prefix.begin(), new_prefix.end());
  DbStatus status = ForEachWithPrefix(
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

DbStatus DomStorageDatabase::Commit(leveldb::WriteBatch* batch) const {
  if (!db_)
    return DbStatus::IOError(kInvalidDatabaseMessage);
  if (fail_commits_for_testing_)
    return DbStatus::IOError("Simulated I/O Error");
#if BUILDFLAG(IS_COBALT)
  return FromLevelDBStatus(db_->Write(CreateSyncWriteOptions(), batch));
#else
  return FromLevelDBStatus(db_->Write(leveldb::WriteOptions(), batch));
#endif
}

DbStatus DomStorageDatabase::RewriteDB() {
  if (!db_)
    return DbStatus::IOError(kInvalidDatabaseMessage);
  leveldb::Status status = leveldb_env::RewriteDB(options_, name_, &db_);
  if (!status.ok())
    db_.reset();
  return FromLevelDBStatus(status);
}

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
>>>>>>> parent of 7d60f81f606 (CONFLICTED Chromium Cherry pick: Revert Cobalt.)
}

}  // namespace storage
