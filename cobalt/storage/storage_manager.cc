/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/storage/storage_manager.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/stringprintf.h"
#include "cobalt/storage/virtual_file.h"
#include "cobalt/storage/virtual_file_system.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"

namespace cobalt {
namespace storage {

namespace {

const char kDefaultSaveFile[] = "cobalt_save.bin";

void SqlDisableJournal(sql::Connection* connection) {
  // Disable journaling for our in-memory database.
  sql::Statement disable_journal(
      connection->GetUniqueStatement("PRAGMA journal_mode=OFF"));
  bool ok = disable_journal.Step();
  DCHECK(ok);
}

int SqlQueryUserVersion(sql::Connection* connection) {
  sql::Statement get_db_version(
      connection->GetUniqueStatement("PRAGMA user_version"));
  bool ok = get_db_version.Step();
  DCHECK(ok);
  return get_db_version.ColumnInt(0);
}

bool SqlQueryTableExists(sql::Connection* connection, const char* table_name) {
  sql::Statement get_exists(connection->GetUniqueStatement(
      "SELECT name FROM sqlite_master WHERE name = ? AND type = 'table'"));
  get_exists.BindString(0, table_name);
  return get_exists.Step();
}

int SqlQuerySchemaVersion(sql::Connection* connection, const char* table_name) {
  sql::Statement get_version(connection->GetUniqueStatement(
      "SELECT version FROM SchemaTable WHERE name = ?"));
  get_version.BindString(0, table_name);
  bool row_found = get_version.Step();
  if (row_found) {
    return get_version.ColumnInt(0);
  } else {
    return -1;
  }
}

void SqlUpdateSchemaVersion(sql::Connection* connection, const char* table_name,
                            int version) {
  sql::Statement update_version(connection->GetUniqueStatement(
      "INSERT INTO SchemaTable (name, version)"
      "VALUES (?, ?)"));
  update_version.BindString(0, table_name);
  update_version.BindInt(1, version);
  bool ok = update_version.Run();
  DCHECK(ok);
}

void SqlCreateSchemaTable(sql::Connection* connection) {
  // Create the schema table.
  sql::Statement create_table(connection->GetUniqueStatement(
      "CREATE TABLE IF NOT EXISTS SchemaTable ("
      "name TEXT, "
      "version INTEGER, "
      "UNIQUE(name, version) ON CONFLICT REPLACE)"));
  bool ok = create_table.Run();
  DCHECK(ok);
}

void SqlUpdateDatabaseUserVersion(sql::Connection* connection) {
  // Update the DB version which will be read in next time.
  // NOTE: Pragma statements cannot be bound, so we must construct the string
  // in full.
  std::string set_db_version_str = base::StringPrintf(
      "PRAGMA user_version = %d", StorageManager::kDatabaseUserVersion);
  sql::Statement set_db_version(
      connection->GetUniqueStatement(set_db_version_str.c_str()));
  bool ok = set_db_version.Run();
  DCHECK(ok);
}

}  // namespace

StorageManager::StorageManager(const Options& options)
    : options_(options),
      sql_thread_(new base::Thread("StorageManager")),
      io_thread_(new base::Thread("StorageIO")),
      ALLOW_THIS_IN_INITIALIZER_LIST(sql_context_(new SqlContext(this))),
      storage_ready_(true /* manual reset */, false /* initially signalled */),
      connection_(new sql::Connection()),
      loaded_database_version_(0),
      initialized_(false) {
  // Start the savegame load immediately.
  io_thread_->Start();
  io_message_loop_ = io_thread_->message_loop_proxy();
  io_message_loop_->PostTask(
      FROM_HERE, base::Bind(&StorageManager::OnInitIO, base::Unretained(this)));

  sql_thread_->Start();
  sql_message_loop_ = sql_thread_->message_loop_proxy();
}

StorageManager::~StorageManager() {
  // Destroy various objects on the proper thread.
  io_message_loop_->PostTask(FROM_HERE, base::Bind(&StorageManager::OnDestroyIO,
                                                   base::Unretained(this)));

  sql_message_loop_->PostTask(FROM_HERE, base::Bind(&StorageManager::OnDestroy,
                                                    base::Unretained(this)));

  // Force all tasks to finish. Then we can safely let the rest of our
  // member variables be destroyed.
  io_thread_.reset(NULL);
  sql_thread_.reset(NULL);
}

void StorageManager::GetSqlContext(const SqlCallback& callback) {
  sql_message_loop_->PostTask(FROM_HERE,
                              base::Bind(callback, sql_context_.get()));
}

void StorageManager::Flush(const base::Closure& callback) {
  // Make sure this runs on the correct thread.
  if (MessageLoop::current()->message_loop_proxy() != sql_message_loop_) {
    sql_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&StorageManager::Flush, base::Unretained(this), callback));
    return;
  }
  FinishInit();
  // Serialize the database into a buffer. Then send the bytes
  // to OnFlushIO for a blocking write to the savegame.
  scoped_ptr<Savegame::ByteVector> raw_bytes_ptr;
  int size = vfs_->Serialize(NULL, true /*dry_run*/);
  raw_bytes_ptr.reset(new Savegame::ByteVector(static_cast<size_t>(size)));
  if (size > 0) {
    Savegame::ByteVector& raw_bytes = *raw_bytes_ptr;
    vfs_->Serialize(&raw_bytes[0], false /*dry_run*/);
  }

  // Re-start the auto-flush timer.
  flush_timer_->Reset();

  io_message_loop_->PostTask(
      FROM_HERE, base::Bind(&StorageManager::OnFlushIO, base::Unretained(this),
                            callback, base::Passed(&raw_bytes_ptr)));
}

bool StorageManager::GetSchemaVersion(const char* table_name,
                                      int* schema_version) {
  DCHECK(sql_message_loop_->BelongsToCurrentThread());
  DCHECK(schema_version);

  if (!SqlQueryTableExists(sql_connection(), table_name)) {
    return false;
  }

  int found_version = SqlQuerySchemaVersion(sql_connection(), table_name);
  if (found_version != -1) {
    *schema_version = found_version;
  } else if (loaded_database_version_ != StorageManager::kDatabaseUserVersion) {
    // The schema table did not exist before this session, which is different
    // from the schema table being lost.
    *schema_version = StorageManager::kSchemaTableIsNew;
  } else {
    *schema_version = StorageManager::kSchemaVersionLost;
  }
  return true;
}

void StorageManager::UpdateSchemaVersion(const char* table_name, int version) {
  DCHECK(sql_message_loop_->BelongsToCurrentThread());
  DCHECK_GT(version, 0) << "Schema version numbers must be positive.";

  SqlUpdateSchemaVersion(sql_connection(), table_name, version);
}

sql::Connection* StorageManager::sql_connection() {
  FinishInit();
  return connection_.get();
}

void StorageManager::FinishInit() {
  DCHECK(sql_message_loop_->BelongsToCurrentThread());
  if (initialized_) {
    return;
  }
  storage_ready_.Wait();
  vfs_.reset(new VirtualFileSystem());
  sql_vfs_.reset(new SqlVfs("cobalt_vfs", vfs_.get()));

  flush_timer_.reset(new base::DelayTimer<StorageManager>(
      FROM_HERE, base::TimeDelta::FromSeconds(kFlushSeconds), this,
      &StorageManager::TimedFlush));

  // Savegame has finished loading. Now initialize the database connection.
  // Check if the savegame data contains a VFS header.
  // If so, proceed to deserialize it.
  // If not, load the file into the VFS directly.
  DCHECK(loaded_raw_bytes_);
  Savegame::ByteVector& raw_bytes = *loaded_raw_bytes_;
  VirtualFileSystem::SerializedHeader header = {};
  if (raw_bytes.size() > 0) {
    if (raw_bytes.size() >= sizeof(VirtualFileSystem::SerializedHeader)) {
      memcpy(&header, &raw_bytes[0],
             sizeof(VirtualFileSystem::SerializedHeader));
    }

    if (VirtualFileSystem::GetHeaderVersion(header) == -1) {
      VirtualFile* vf = vfs_->Open(kDefaultSaveFile);
      vf->Write(&raw_bytes[0], static_cast<int>(raw_bytes.size()),
                0 /* offset */);
    } else {
      vfs_->Deserialize(&raw_bytes[0], static_cast<int>(raw_bytes.size()));
    }
  }
  // Finished with this, so empty it out.
  loaded_raw_bytes_.reset(NULL);

  std::vector<std::string> filenames = vfs_->ListFiles();
  if (filenames.size() == 0) {
    filenames.push_back(kDefaultSaveFile);
  }

  // Not a limitation of the VFS- we just need to figure out how to handle
  // the case where there are multiple files in here.
  DCHECK_EQ(1, filenames.size());

  const std::string& save_name = filenames[0];
  bool ok = connection_->Open(FilePath(save_name));
  DCHECK(ok);

  // Open() is lazy. Run a quick check to see if the database is valid.
  int err = connection_->ExecuteAndReturnErrorCode("pragma schema_version;");
  if (err != SQLITE_OK) {
    // Database seems to be invalid.
    DLOG(WARNING) << "Database " << save_name << " appears to be corrupt.";
    // Try to start again. Delete the file in the VFS and make a
    // new connection.
    vfs_->Delete(save_name);
    vfs_->Open(save_name);
    connection_.reset(new sql::Connection());
    ok = connection_->Open(FilePath(save_name));
    DCHECK(ok);
    err = connection_->ExecuteAndReturnErrorCode("pragma schema_version;");
    DCHECK_EQ(SQLITE_OK, err);
  }

  // Configure our SQLite database now that it's open.
  SqlDisableJournal(connection_.get());
  loaded_database_version_ = SqlQueryUserVersion(connection_.get());
  SqlCreateSchemaTable(connection_.get());
  SqlUpdateDatabaseUserVersion(connection_.get());

  // Start the auto-flush timer going.
  flush_timer_->Reset();
  initialized_ = true;
}

void StorageManager::OnInitIO() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  // Create a savegame object on the storage I/O thread.
  savegame_ = options_.savegame_options.CreateSavegame();

  // Load the save data into our VFS, if it exists.
  loaded_raw_bytes_.reset(new Savegame::ByteVector());
  savegame_->Read(loaded_raw_bytes_.get());

  // Signal storage is ready. Anyone waiting for the load will now be unblocked.
  storage_ready_.Signal();
}

void StorageManager::OnFlushIO(const base::Closure& callback,
                               scoped_ptr<Savegame::ByteVector> raw_bytes_ptr) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  if (raw_bytes_ptr->size() > 0) {
    bool ret = savegame_->Write(*raw_bytes_ptr);
    DCHECK(ret);
  }

  if (!callback.is_null()) {
    callback.Run();
  }
}

void StorageManager::OnDestroy() {
  DCHECK(sql_message_loop_->BelongsToCurrentThread());
  // Ensure these objects are destroyed on the proper thread.
  flush_timer_.reset(NULL);
  sql_vfs_.reset(NULL);
  vfs_.reset(NULL);
}

void StorageManager::OnDestroyIO() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  // Ensure these objects are destroyed on the proper thread.
  savegame_.reset(NULL);
}

void StorageManager::TimedFlush() {
  DCHECK(sql_message_loop_->BelongsToCurrentThread());
  // Called by flush_timer_ to sync every so often.
  Flush(base::Closure());
}

}  // namespace storage
}  // namespace cobalt
