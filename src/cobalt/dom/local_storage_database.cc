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

#include "cobalt/dom/local_storage_database.h"

#include "base/debug/trace_event.h"
#include "cobalt/dom/storage_area.h"
#include "cobalt/storage/storage_manager.h"
#include "nb/memory_scope.h"
#include "sql/statement.h"

namespace cobalt {
namespace dom {

namespace {

const int kOriginalLocalStorageSchemaVersion = 1;
const int kLatestLocalStorageSchemaVersion = 1;

void SqlInit(storage::SqlContext* sql_context) {
  TRACK_MEMORY_SCOPE("Storage");
  TRACE_EVENT0("cobalt::storage", "LocalStorage::SqlInit()");
  sql::Connection* conn = sql_context->sql_connection();
  int schema_version;
  bool table_exists =
      sql_context->GetSchemaVersion("LocalStorageTable", &schema_version);

  if (table_exists) {
    if (schema_version == storage::StorageManager::kSchemaTableIsNew) {
      // This savegame predates the existence of the schema table.
      // Since the local-storage table did not change between the initial
      // release of the app and the introduction of the schema table, assume
      // that this existing local-storage table is schema version 1.  This
      // avoids a loss of data on upgrade.

      DLOG(INFO) << "Updating LocalStorageTable schema version to "
                 << kOriginalLocalStorageSchemaVersion;
      sql_context->UpdateSchemaVersion("LocalStorageTable",
                                       kOriginalLocalStorageSchemaVersion);
    } else if (schema_version == storage::StorageManager::kSchemaVersionLost) {
      // Since there has only been one schema so far, treat this the same as
      // kSchemaTableIsNew.  When there are multiple schemas in the wild,
      // we may want to drop the table instead.
      sql_context->UpdateSchemaVersion("LocalStorageTable",
                                       kOriginalLocalStorageSchemaVersion);
    }
  } else {
    // The table does not exist, so create it in its latest form.
    sql::Statement create_table(conn->GetUniqueStatement(
        "CREATE TABLE LocalStorageTable ("
        "  site_identifier TEXT, "
        "  key TEXT, "
        "  value TEXT NOT NULL ON CONFLICT FAIL, "
        "  UNIQUE(site_identifier, key) ON CONFLICT REPLACE"
        ")"));
    bool ok = create_table.Run();
    DCHECK(ok);
    sql_context->UpdateSchemaVersion("LocalStorageTable",
                                     kLatestLocalStorageSchemaVersion);
  }
}

void SqlReadValues(const std::string& id,
                   const LocalStorageDatabase::ReadCompletionCallback& callback,
                   storage::SqlContext* sql_context) {
  TRACK_MEMORY_SCOPE("Storage");
  scoped_ptr<StorageArea::StorageMap> values(new StorageArea::StorageMap);
  sql::Connection* conn = sql_context->sql_connection();
  sql::Statement get_values(conn->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT key, value FROM LocalStorageTable WHERE site_identifier = ?"));
  get_values.BindString(0, id);
  while (get_values.Step()) {
    // TODO: In Steel, these were string16.
    std::string key(get_values.ColumnString(0));
    std::string value(get_values.ColumnString(1));
    values->insert(std::make_pair(key, value));
  }
  callback.Run(values.Pass());
}

void SqlWrite(const std::string& id, const std::string& key,
              const std::string& value, storage::SqlContext* sql_context) {
  TRACK_MEMORY_SCOPE("Storage");
  sql::Connection* conn = sql_context->sql_connection();
  sql::Statement write_statement(conn->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO LocalStorageTable (site_identifier, key, value) "
      "VALUES (?, ?, ?)"));
  write_statement.BindString(0, id);
  write_statement.BindString(1, key);
  write_statement.BindString(2, value);
  bool ok = write_statement.Run();
  DCHECK(ok);
  sql_context->FlushOnChange();
}

void SqlDelete(const std::string& id, const std::string& key,
               storage::SqlContext* sql_context) {
  TRACK_MEMORY_SCOPE("Storage");
  sql::Connection* conn = sql_context->sql_connection();
  sql::Statement delete_statement(
      conn->GetCachedStatement(SQL_FROM_HERE,
                               "DELETE FROM LocalStorageTable "
                               "WHERE site_identifier = ? AND key = ?"));
  delete_statement.BindString(0, id);
  delete_statement.BindString(1, key);
  bool ok = delete_statement.Run();
  DCHECK(ok);
  sql_context->FlushOnChange();
}

void SqlClear(const std::string& id, storage::SqlContext* sql_context) {
  TRACK_MEMORY_SCOPE("Storage");
  sql::Connection* conn = sql_context->sql_connection();
  sql::Statement clear_statement(
      conn->GetCachedStatement(SQL_FROM_HERE,
                               "DELETE FROM LocalStorageTable "
                               "WHERE site_identifier = ?"));
  clear_statement.BindString(0, id);
  bool ok = clear_statement.Run();
  DCHECK(ok);
  sql_context->FlushOnChange();
}
}  // namespace

LocalStorageDatabase::LocalStorageDatabase(storage::StorageManager* storage)
    : storage_(storage), initialized_(false) {}

// Init is done lazily only once the first operation occurs. This is to avoid
// a potential wait while the storage manager loads from disk.
void LocalStorageDatabase::Init() {
  if (!initialized_) {
    storage_->GetSqlContext(base::Bind(&SqlInit));
    initialized_ = true;
  }
}

void LocalStorageDatabase::ReadAll(const std::string& id,
                                   const ReadCompletionCallback& callback) {
  TRACK_MEMORY_SCOPE("Storage");
  Init();
  storage_->GetSqlContext(base::Bind(&SqlReadValues, id, callback));
}

void LocalStorageDatabase::Write(const std::string& id, const std::string& key,
                                 const std::string& value) {
  TRACK_MEMORY_SCOPE("Storage");
  Init();
  storage_->GetSqlContext(base::Bind(&SqlWrite, id, key, value));
}

void LocalStorageDatabase::Delete(const std::string& id,
                                  const std::string& key) {
  Init();
  storage_->GetSqlContext(base::Bind(&SqlDelete, id, key));
}

void LocalStorageDatabase::Clear(const std::string& id) {
  Init();
  storage_->GetSqlContext(base::Bind(&SqlClear, id));
}

void LocalStorageDatabase::Flush(const base::Closure& callback) {
  storage_->FlushNow(callback);
}

}  // namespace dom
}  // namespace cobalt
