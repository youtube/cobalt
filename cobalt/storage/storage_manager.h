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

#ifndef STORAGE_STORAGE_MANAGER_H_
#define STORAGE_STORAGE_MANAGER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "cobalt/storage/savegame.h"
#include "cobalt/storage/sql_vfs.h"
#include "cobalt/storage/virtual_file_system.h"
#include "sql/connection.h"

namespace cobalt {
namespace storage {
class SqlContext;

// StorageManager manages a SQLite database containing cookies and local
// storage data. On most platforms, this is written to disk as a savegame
// using platform APIs. On Linux/Windows, it's a regular file.
// Internally this runs two threads: one thread to perform blocking I/O,
// and one where SQL operations occur.  Users are expected to access the
// database via an SqlContext, which can be obtained with GetSqlContext().
// The callback to GetSqlCallback will run on the SQL thread.
// Operations on SqlContext will block the SQL thread until the savegame
// is loaded.
class StorageManager {
 public:
  struct Options {
    Savegame::Options savegame_options;
  };

  typedef base::Callback<void(SqlContext*)> SqlCallback;

  // Database version "2" indicates that this was created by Steel v1.x.
  // Database version "0" indicates that this was created by v2.x beta,
  //   patches 0-3.  Version "0" is a default from sqlite3 because these
  //   versions of the application did not set this value at all.
  // Database version "3" indicates that the schema versions of individual
  //   tables should be tracked in SchemaTable.
  static const int kDatabaseUserVersion = 3;

  // Flush the database to disk every five minutes.
  static const int kFlushSeconds = 300;

  // Schema-related error codes.  See GetSchemaVersion().
  enum {
    kSchemaTableIsNew = -1,
    kSchemaVersionLost = -2,
  };

  explicit StorageManager(const Options& options);
  virtual ~StorageManager();

  // Obtain the SqlContext for our database.
  // callback will be called with an SqlContext that can be used to operate on
  // the database. The callback will run on the storage manager's message loop.
  void GetSqlContext(const SqlCallback& callback);

  // Trigger a write of our database to disk. This call returns immediately.
  // callback, if provided, will be called when the Flush() has completed,
  // and will be run on the storage manager's IO thread.
  void Flush(const base::Closure& callback);

  const Options& options() const { return options_; }

 private:
  // SqlContext needs access to our internal APIs.
  friend class SqlContext;
  // Give StorageManagerTest access, so we can more easily test some internals.
  friend class StorageManagerTest;

  // Initialize the SQLite database. This blocks until the savegame load is
  // complete.
  void FinishInit();

  // Run on the storage I/O thread to start loading the savegame.
  void OnInitIO();

  // Callback that runs on the storage I/O thread to write the database
  // to the savegame's persistent storage.
  void OnFlushIO(const base::Closure& callback,
                 scoped_ptr<Savegame::ByteVector> raw_bytes);

  // Called by the destructor, to ensure we destroy certain objects on the
  // sql thread.
  void OnDestroy();

  // Called by the destructor, to ensure we destroy certain objects on the
  // I/O thread.
  void OnDestroyIO();

  // Called by the flush timer every so often, if a Flush() hasn't already
  // been explicitly called.
  void TimedFlush();

  // Internal API for use by SqlContext.
  sql::Connection* sql_connection();
  bool GetSchemaVersion(const char* table_name, int* schema_version);
  void UpdateSchemaVersion(const char* table_name, int version);

  // Configuration options for the Storage Manager.
  Options options_;

  // Storage manager runs on its own thread. This is where SQL database
  // operations are done.
  scoped_ptr<base::Thread> sql_thread_;
  scoped_refptr<base::MessageLoopProxy> sql_message_loop_;

  // Storage I/O (savegame reads/writes) runs on a separate thread.
  scoped_ptr<base::Thread> io_thread_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // An interface to the storage manager's SQL database that will run on
  // the correct thread.
  scoped_ptr<SqlContext> sql_context_;

  // The database gets loaded from disk. We block on returning a SQL context
  // until storage_ready_ is signalled.
  base::WaitableEvent storage_ready_;

  // The in-memory database connection.
  scoped_ptr<sql::Connection> connection_;

  // Virtual file system that contains our in-memory SQLite database.
  scoped_ptr<VirtualFileSystem> vfs_;

  // An interface between Sqlite and VirtualFileSystem.
  scoped_ptr<SqlVfs> sql_vfs_;

  // Interface to platform-specific savegame data.
  scoped_ptr<Savegame> savegame_;

  // When the savegame is loaded at startup, we keep the raw data around
  // until we can initialize the database on the correct thread.
  scoped_ptr<Savegame::ByteVector> loaded_raw_bytes_;

  // Timer that runs every N seconds to flush storage to disk.
  scoped_ptr<base::DelayTimer<StorageManager> > flush_timer_;

  // See comments for for kDatabaseUserVersion.
  int loaded_database_version_;
  // false until the SQL database is fully configured.
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(StorageManager);
};

// Proxy for accessing StorageManager's SQL database in a thread-safe way.
// All access to the StorageManager's database should be done via an SqlContext.
// Only the StorageManager can create this class.
class SqlContext {
 public:
  sql::Connection* sql_connection() const {
    return storage_manager_->sql_connection();
  }

  // Get the schema version for the given table.
  // Returns false if the table does not exist, otherwise returns true
  // and writes the version number to the schema_version pointer.
  // schema_version will be set to kSchemaTableIsNew if the table exists,
  // but the schema table was newly created in this session.
  // schema_version will be set to kSchemaVersionLost if the table exists,
  // and the schema table existed once before, but has since been lost.
  // In this case, the schema version cannot be known or directly inferred.
  bool GetSchemaVersion(const char* table_name, int* schema_version) {
    return storage_manager_->GetSchemaVersion(table_name, schema_version);
  }

  // Updates the schema version for the given table.
  // The version number must be greater than 0.
  void UpdateSchemaVersion(const char* table_name, int version) {
    return storage_manager_->UpdateSchemaVersion(table_name, version);
  }

  void Flush(const base::Closure& callback) {
    storage_manager_->Flush(callback);
  }

 private:
  StorageManager* storage_manager_;

  explicit SqlContext(StorageManager* storage_manager)
      : storage_manager_(storage_manager) {}

  friend StorageManager::StorageManager(const Options& options);
  DISALLOW_COPY_AND_ASSIGN(SqlContext);
};

}  // namespace storage
}  // namespace cobalt

#endif  // STORAGE_STORAGE_MANAGER_H_
