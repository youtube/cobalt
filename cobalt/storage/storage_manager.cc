// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/storage/storage_manager.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/stringprintf.h"
#include "cobalt/storage/savegame_thread.h"
#include "cobalt/storage/upgrade/upgrade_reader.h"
#include "cobalt/storage/virtual_file.h"
#include "cobalt/storage/virtual_file_system.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"

namespace cobalt {
namespace storage {

namespace {

// FlushOnChange() delays for a while to avoid spamming writes to disk; we often
// get several FlushOnChange() calls in a row.
const int kDatabaseFlushOnLastChangeDelayMs = 500;
const int kDatabaseFlushOnChangeMaxDelayMs = 2000;

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

const std::string& GetFirstValidDatabaseFile(
    const std::vector<std::string>& filenames) {
  // Caller must ensure at least one file exists.
  DCHECK_GT(filenames.size(), size_t(0));

  for (size_t i = 0; i < filenames.size(); ++i) {
    sql::Connection connection;
    bool is_opened = connection.Open(FilePath(filenames[i]));
    if (!is_opened) {
      continue;
    }
    int err = connection.ExecuteAndReturnErrorCode("pragma schema_version;");
    if (err != SQLITE_OK) {
      continue;
    }
    // File can be opened as a database.
    return filenames[i];
  }

  // Caller must handle case where a valid database file cannot be found.
  DLOG(WARNING) << "Cannot find valid database file in save data";
  return filenames[0];
}

}  // namespace

StorageManager::StorageManager(scoped_ptr<UpgradeHandler> upgrade_handler,
                               const Options& options)
    : upgrade_handler_(upgrade_handler.Pass()),
      options_(options),
      sql_thread_(new base::Thread("StorageManager SQL")),
      ALLOW_THIS_IN_INITIALIZER_LIST(sql_context_(new SqlContext(this))),
      connection_(new sql::Connection()),
      loaded_database_version_(0),
      flush_processing_(false),
      flush_pending_(false),
      no_flushes_pending_(true /* manual reset */,
                          true /* initially signalled */) {
  DCHECK(upgrade_handler_);
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  savegame_thread_.reset(new SavegameThread(options_.savegame_options));
  // Start the savegame load immediately.
  sql_thread_->Start();
  sql_message_loop_ = sql_thread_->message_loop_proxy();

  flush_on_last_change_timer_.reset(new base::OneShotTimer<StorageManager>());
  flush_on_change_max_delay_timer_.reset(
      new base::OneShotTimer<StorageManager>());

  // Guarantee that initialization, including any upgrading, is complete before
  // returning from the constructor.
  sql_message_loop_->PostTask(FROM_HERE, base::Bind(&StorageManager::FinishInit,
                                                    base::Unretained(this)));
  FinishIO();
}

StorageManager::~StorageManager() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(!sql_message_loop_->BelongsToCurrentThread());

  // Wait for all I/O operations to complete.
  FinishIO();

  // Destroy various objects on the proper thread.
  sql_message_loop_->PostTask(FROM_HERE, base::Bind(&StorageManager::OnDestroy,
                                                    base::Unretained(this)));

  // Force all tasks to finish. Then we can safely let the rest of our
  // member variables be destroyed.
  sql_thread_.reset();
}

void StorageManager::GetSqlContext(const SqlCallback& callback) {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  sql_message_loop_->PostTask(FROM_HERE,
                              base::Bind(callback, sql_context_.get()));
}

void StorageManager::FlushOnChange() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  // Make sure this runs on the correct thread.
  if (MessageLoop::current()->message_loop_proxy() != sql_message_loop_) {
    sql_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&StorageManager::FlushOnChange, base::Unretained(this)));
    return;
  }

  // Only start the timers if there isn't already a flush pending.
  if (!flush_pending_) {
    // The last change timer is always re-started on any change.
    flush_on_last_change_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kDatabaseFlushOnLastChangeDelayMs),
        this, &StorageManager::OnFlushOnChangeTimerFired);
    // The max delay timer is never re-started after it starts running.
    if (!flush_on_change_max_delay_timer_->IsRunning()) {
      flush_on_change_max_delay_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kDatabaseFlushOnChangeMaxDelayMs),
          this, &StorageManager::OnFlushOnChangeTimerFired);
    }
  }
}

void StorageManager::FlushNow(const base::Closure& callback) {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  // Make sure this runs on the correct thread.
  if (MessageLoop::current()->message_loop_proxy() != sql_message_loop_) {
    sql_message_loop_->PostTask(
        FROM_HERE, base::Bind(&StorageManager::FlushNow, base::Unretained(this),
                              callback));
    return;
  }

  StopFlushOnChangeTimers();
  QueueFlush(callback);
}

bool StorageManager::GetSchemaVersion(const char* table_name,
                                      int* schema_version) {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
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
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(sql_message_loop_->BelongsToCurrentThread());
  DCHECK_GT(version, 0) << "Schema version numbers must be positive.";

  SqlUpdateSchemaVersion(sql_connection(), table_name, version);
}

sql::Connection* StorageManager::sql_connection() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  return connection_.get();
}

void StorageManager::FinishInit() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(sql_message_loop_->BelongsToCurrentThread());

  vfs_.reset(new VirtualFileSystem());
  sql_vfs_.reset(new SqlVfs("cobalt_vfs", vfs_.get()));
  // Savegame has finished loading. Now initialize the database connection.
  // Check if this is upgrade data, if so, handle it, otherwise:
  // Check if the savegame data contains a VFS header.
  // If so, proceed to deserialize it.
  // If not, load the file into the VFS directly.
  scoped_ptr<Savegame::ByteVector> loaded_raw_bytes =
      savegame_thread_->GetLoadedRawBytes();
  DCHECK(loaded_raw_bytes);
  Savegame::ByteVector& raw_bytes = *loaded_raw_bytes;
  VirtualFileSystem::SerializedHeader header = {};

  if (raw_bytes.size() > 0) {
    const char* buffer = reinterpret_cast<char*>(&raw_bytes[0]);
    int buffer_size = static_cast<int>(raw_bytes.size());
    // Is this upgrade data?
    if (upgrade::UpgradeReader::IsUpgradeData(buffer, buffer_size)) {
      upgrade_handler_->OnUpgrade(this, buffer, buffer_size);
    } else {
      if (raw_bytes.size() >= sizeof(VirtualFileSystem::SerializedHeader)) {
        memcpy(&header, &raw_bytes[0],
               sizeof(VirtualFileSystem::SerializedHeader));
      }

      if (!vfs_->Deserialize(&raw_bytes[0], buffer_size)) {
        VirtualFile* vf = vfs_->Open(kDefaultSaveFile);
        vf->Write(&raw_bytes[0], buffer_size, 0 /* offset */);
      }
    }
  }

  std::vector<std::string> filenames = vfs_->ListFiles();
  if (filenames.size() == 0) {
    filenames.push_back(kDefaultSaveFile);
  }

  // Legacy Steel save data may contain multiple files (e.g. db-journal as well
  // as db), so use the first one that looks like a valid database file.
  const std::string& save_name = GetFirstValidDatabaseFile(filenames);
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
}

void StorageManager::StopFlushOnChangeTimers() {
  if (flush_on_last_change_timer_->IsRunning()) {
    flush_on_last_change_timer_->Stop();
  }
  if (flush_on_change_max_delay_timer_->IsRunning()) {
    flush_on_change_max_delay_timer_->Stop();
  }
}

void StorageManager::OnFlushOnChangeTimerFired() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(sql_message_loop_->BelongsToCurrentThread());

  StopFlushOnChangeTimers();
  QueueFlush(base::Closure());
}

void StorageManager::OnFlushIOCompletedSQLCallback() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  // Make sure this runs on the SQL message loop.
  if (MessageLoop::current()->message_loop_proxy() != sql_message_loop_) {
    sql_message_loop_->PostTask(
        FROM_HERE, base::Bind(&StorageManager::OnFlushIOCompletedSQLCallback,
                              base::Unretained(this)));
    return;
  }

  flush_processing_ = false;

  // Fire all the callbacks waiting on the current flush to complete.
  for (size_t i = 0; i < flush_processing_callbacks_.size(); ++i) {
    DCHECK(!flush_processing_callbacks_[i].is_null());
    flush_processing_callbacks_[i].Run();
  }
  flush_processing_callbacks_.clear();

  if (flush_pending_) {
    // If another flush has been requested while we were processing the one that
    // just completed, start that next flush now.
    flush_processing_callbacks_.swap(flush_pending_callbacks_);
    flush_pending_ = false;
    FlushInternal();
    DCHECK(flush_processing_);
  } else {
    no_flushes_pending_.Signal();
  }
}

void StorageManager::QueueFlush(const base::Closure& callback) {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(sql_message_loop_->BelongsToCurrentThread());

  if (!flush_processing_) {
    // If no flush is currently in progress, flush immediately.
    if (!callback.is_null()) {
      flush_processing_callbacks_.push_back(callback);
    }
    FlushInternal();
    DCHECK(flush_processing_);
  } else {
    // Otherwise, indicate that we would like to re-flush as soon as the
    // current one completes.
    flush_pending_ = true;
    if (!callback.is_null()) {
      flush_pending_callbacks_.push_back(callback);
    }
  }
}

void StorageManager::FlushInternal() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(sql_message_loop_->BelongsToCurrentThread());

  flush_processing_ = true;
  no_flushes_pending_.Reset();

  // Serialize the database into a buffer. Then send the bytes
  // to OnFlushIO for a blocking write to the savegame.
  scoped_ptr<Savegame::ByteVector> raw_bytes_ptr;
  int size = vfs_->Serialize(NULL, true /*dry_run*/);
  raw_bytes_ptr.reset(new Savegame::ByteVector(static_cast<size_t>(size)));
  if (size > 0) {
    Savegame::ByteVector& raw_bytes = *raw_bytes_ptr;
    vfs_->Serialize(&raw_bytes[0], false /*dry_run*/);
  }

  // Send the savegame bytes off to the SavegameThread object to be
  // asynchronously written to the savegame file.
  savegame_thread_->Flush(
      raw_bytes_ptr.Pass(),
      base::Bind(&StorageManager::OnFlushIOCompletedSQLCallback,
                 base::Unretained(this)));
}

void StorageManager::FinishIO() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(!sql_message_loop_->BelongsToCurrentThread());

  // Make sure that the on change timers fire if they're running.
  sql_message_loop_->PostTask(
      FROM_HERE, base::Bind(&StorageManager::FireRunningOnChangeTimers,
                            base::Unretained(this)));

  // The SQL thread may be communicating with the savegame I/O thread still,
  // flushing all pending updates.  This process can require back and forth
  // communication.  This method exists to wait for that communication to
  // finish and for all pending flushes to complete.

  // Start by finishing all commands currently in the sql message loop queue.
  // This method is called by the destructor, so the only new tasks posted
  // after this one will be generated internally.  We need to do this because
  // it is possible that there are no flushes pending at this instant, but there
  // are tasks queued on |sql_message_loop_| that will begin a flush, and so
  // we make sure that these are executed first.
  sql_message_loop_->WaitForFence();

  // Now wait for all pending flushes to wrap themselves up.  This may involve
  // the savegame I/O thread and the SQL thread posting tasks to each other.
  no_flushes_pending_.Wait();
}

void StorageManager::FireRunningOnChangeTimers() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(sql_message_loop_->BelongsToCurrentThread());

  if (flush_on_last_change_timer_->IsRunning() ||
      flush_on_change_max_delay_timer_->IsRunning()) {
    OnFlushOnChangeTimerFired();
  }
}

void StorageManager::OnDestroy() {
  TRACE_EVENT0("cobalt::storage", __FUNCTION__);
  DCHECK(sql_message_loop_->BelongsToCurrentThread());

  // Stop the savegame thread and have it wrap up any pending I/O operations.
  savegame_thread_.reset();

  // Ensure these objects are destroyed on the proper thread.
  flush_on_last_change_timer_.reset(NULL);
  flush_on_change_max_delay_timer_.reset(NULL);
  sql_vfs_.reset(NULL);
  vfs_.reset(NULL);
}

}  // namespace storage
}  // namespace cobalt
