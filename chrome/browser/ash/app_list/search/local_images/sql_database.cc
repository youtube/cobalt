// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_images/sql_database.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

namespace app_list {
namespace {

constexpr int kUninitializedDbVersionNumber = 1;

}  // namespace

SqlDatabase::SqlDatabase(
    const base::FilePath& path_to_db,
    const std::string& histogram_tag,
    int current_version_number,
    base::RepeatingCallback<int(SqlDatabase* db)> create_table_schema,
    base::RepeatingCallback<int(SqlDatabase* db, int current_version_number)>
        migrate_table_schema)
    : create_table_schema_(std::move(create_table_schema)),
      migrate_table_schema_(std::move(migrate_table_schema)),
      path_to_db_(path_to_db),
      histogram_tag_(histogram_tag),
      current_version_number_(current_version_number) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK_GT(current_version_number_, 1);
  DCHECK(!path_to_db_.empty());
}

SqlDatabase::~SqlDatabase() = default;

bool SqlDatabase::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_ == nullptr);

  const base::FilePath dir = path_to_db_.DirName();
  if (!base::PathExists(dir) && !base::CreateDirectory(dir)) {
    LOG(ERROR) << "Could not create a directory to the new database.";
    return false;
  }

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions());
  db_->set_histogram_tag(histogram_tag_);
  meta_table_ = std::make_unique<sql::MetaTable>();

  // base::Unretained is safe because `this` owns (and therefore outlives) the
  // sql::Database held by `db_`. That is, `db_` calls the error callback and
  // if `this` destroyed then `db_` is destroyed, as well.
  db_->set_error_callback(base::BindRepeating(&SqlDatabase::OnErrorCallback,
                                              base::Unretained(this)));

  if (!db_->Open(path_to_db_)) {
    LOG(ERROR) << "Unable to open " << histogram_tag_ << " DB.";
    // TODO(b/260646344): make a callback RecordOpenDBProblem();
    return RazeDb();
  }

  // Either initializes a new meta table or loads it from the db if exists.
  if (!meta_table_->Init(db_.get(), kUninitializedDbVersionNumber,
                         kUninitializedDbVersionNumber)) {
    return false;
  }

  if (meta_table_->GetVersionNumber() == kUninitializedDbVersionNumber) {
    const int new_version_number = create_table_schema_.Run(this);
    DCHECK_GT(new_version_number, 1);
    // TODO(crbug.com/1414092): Set the version numbers atomically with the
    // schema within a transaction and check the return values instead of
    // ignoring them.
    std::ignore = meta_table_->SetVersionNumber(new_version_number);
    std::ignore = meta_table_->SetCompatibleVersionNumber(new_version_number);
    return true;
  }

  if (meta_table_->GetVersionNumber() == current_version_number_) {
    return true;
  }

  if (!MigrateDatabaseSchema()) {
    LOG(ERROR) << "Unable to migrate the schema for " << histogram_tag_;
    // TODO(b/260646344): make a callback RecordDBMigrationProblem();
    return false;
  }

  return true;
}

bool SqlDatabase::MigrateDatabaseSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // May happen if the code migrated from dev to stable channel.
  if (meta_table_->GetVersionNumber() > current_version_number_) {
    LOG(ERROR) << histogram_tag_ << " database is too new. Razing.";
    return RazeDb() && Initialize();
  }

  const int new_version_number =
      migrate_table_schema_.Run(this, meta_table_->GetVersionNumber());
  DCHECK_GT(new_version_number, 1);
  if (new_version_number < meta_table_->GetVersionNumber()) {
    LOG(ERROR) << "Failed to migrate the schema. Razing.";
    return RazeDb() && Initialize();
  }

  return true;
}

void SqlDatabase::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  meta_table_->Reset();
  db_.reset();
  meta_table_.reset();
}

void SqlDatabase::OnErrorCallback(int error, sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << sql::ToSqliteResultCode(error);
  if (sql::IsErrorCatastrophic(error)) {
    LOG(ERROR) << "The error is catastrophic. Razing db.";
    RazeDb();
  }
}

sql::Statement SqlDatabase::GetStatementForQuery(
    const sql::StatementID& sql_from_here,
    const char* query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Making statement for query: " << query;
  DCHECK(db_->IsSQLValid(query));
  return sql::Statement(db_->GetCachedStatement(sql_from_here, query));
}

bool SqlDatabase::RazeDb() {
  DVLOG(1) << "Razing db.";
  if (db_ && db_->is_open()) {
    // Sometimes it fails to do it due to locks or open handles.
    if (!db_->Raze() && !sql::Database::Delete(path_to_db_)) {
      return false;
    }
    db_.reset();
  }
  if (meta_table_) {
    meta_table_->Reset();
    meta_table_.reset();
  }
  return true;
}

}  // namespace app_list
