// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/connection.h"

#include <string.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"

namespace {

// Spin for up to a second waiting for the lock to clear when setting
// up the database.
// TODO(shess): Better story on this.  http://crbug.com/56559
const int kBusyTimeoutSeconds = 1;

class ScopedBusyTimeout {
 public:
  explicit ScopedBusyTimeout(sqlite3* db)
      : db_(db) {
  }
  ~ScopedBusyTimeout() {
    sqlite3_busy_timeout(db_, 0);
  }

  int SetTimeout(base::TimeDelta timeout) {
    DCHECK_LT(timeout.InMilliseconds(), INT_MAX);
    return sqlite3_busy_timeout(db_,
                                static_cast<int>(timeout.InMilliseconds()));
  }

 private:
  sqlite3* db_;
};

// Helper to "safely" enable writable_schema.  No error checking
// because it is reasonable to just forge ahead in case of an error.
// If turning it on fails, then most likely nothing will work, whereas
// if turning it off fails, it only matters if some code attempts to
// continue working with the database and tries to modify the
// sqlite_master table (none of our code does this).
class ScopedWritableSchema {
 public:
  explicit ScopedWritableSchema(sqlite3* db)
      : db_(db) {
    sqlite3_exec(db_, "PRAGMA writable_schema=1", NULL, NULL, NULL);
  }
  ~ScopedWritableSchema() {
    sqlite3_exec(db_, "PRAGMA writable_schema=0", NULL, NULL, NULL);
  }

 private:
  sqlite3* db_;
};

}  // namespace

namespace sql {

bool StatementID::operator<(const StatementID& other) const {
  if (number_ != other.number_)
    return number_ < other.number_;
  return strcmp(str_, other.str_) < 0;
}

ErrorDelegate::~ErrorDelegate() {
}

Connection::StatementRef::StatementRef()
    : connection_(NULL),
      stmt_(NULL) {
}

Connection::StatementRef::StatementRef(sqlite3_stmt* stmt)
    : connection_(NULL),
      stmt_(stmt) {
}

Connection::StatementRef::StatementRef(Connection* connection,
                                       sqlite3_stmt* stmt)
    : connection_(connection),
      stmt_(stmt) {
  connection_->StatementRefCreated(this);
}

Connection::StatementRef::~StatementRef() {
  if (connection_)
    connection_->StatementRefDeleted(this);
  Close();
}

void Connection::StatementRef::Close() {
  if (stmt_) {
    // Call to AssertIOAllowed() cannot go at the beginning of the function
    // because Close() is called unconditionally from destructor to clean
    // connection_. And if this is inactive statement this won't cause any
    // disk access and destructor most probably will be called on thread
    // not allowing disk access.
    // TODO(paivanof@gmail.com): This should move to the beginning
    // of the function. http://crbug.com/136655.
    AssertIOAllowed();
    sqlite3_finalize(stmt_);
    stmt_ = NULL;
  }
  connection_ = NULL;  // The connection may be getting deleted.
}

Connection::Connection()
    : db_(NULL),
      page_size_(0),
      cache_size_(0),
      exclusive_locking_(false),
      transaction_nesting_(0),
      needs_rollback_(false),
      in_memory_(false),
      error_delegate_(NULL) {
}

Connection::~Connection() {
  Close();
}

bool Connection::Open(const FilePath& path) {
#if defined(OS_WIN)
  return OpenInternal(WideToUTF8(path.value()));
#elif defined(OS_POSIX)
  return OpenInternal(path.value());
#endif
}

bool Connection::OpenInMemory() {
  in_memory_ = true;
  return OpenInternal(":memory:");
}

void Connection::Close() {
  // TODO(shess): Calling "PRAGMA journal_mode = DELETE" at this point
  // will delete the -journal file.  For ChromiumOS or other more
  // embedded systems, this is probably not appropriate, whereas on
  // desktop it might make some sense.

  // sqlite3_close() needs all prepared statements to be finalized.
  // Release all cached statements, then assert that the client has
  // released all statements.
  statement_cache_.clear();
  DCHECK(open_statements_.empty());

  // Additionally clear the prepared statements, because they contain
  // weak references to this connection.  This case has come up when
  // error-handling code is hit in production.
  ClearCache();

  if (db_) {
    // Call to AssertIOAllowed() cannot go at the beginning of the function
    // because Close() must be called from destructor to clean
    // statement_cache_, it won't cause any disk access and it most probably
    // will happen on thread not allowing disk access.
    // TODO(paivanof@gmail.com): This should move to the beginning
    // of the function. http://crbug.com/136655.
    AssertIOAllowed();
    // TODO(shess): Histogram for failure.
    sqlite3_close(db_);
    db_ = NULL;
  }
}

void Connection::Preload() {
  AssertIOAllowed();

  if (!db_) {
    DLOG(FATAL) << "Cannot preload null db";
    return;
  }

  // A statement must be open for the preload command to work. If the meta
  // table doesn't exist, it probably means this is a new database and there
  // is nothing to preload (so it's OK we do nothing).
  if (!DoesTableExist("meta"))
    return;
  Statement dummy(GetUniqueStatement("SELECT * FROM meta"));
  if (!dummy.Step())
    return;

#if !defined(USE_SYSTEM_SQLITE)
  // This function is only defined in Chromium's version of sqlite.
  // Do not call it when using system sqlite.
  sqlite3_preload(db_);
#endif
}

// Create an in-memory database with the existing database's page
// size, then backup that database over the existing database.
bool Connection::Raze() {
  AssertIOAllowed();

  if (!db_) {
    DLOG(FATAL) << "Cannot raze null db";
    return false;
  }

  if (transaction_nesting_ > 0) {
    DLOG(FATAL) << "Cannot raze within a transaction";
    return false;
  }

  sql::Connection null_db;
  if (!null_db.OpenInMemory()) {
    DLOG(FATAL) << "Unable to open in-memory database.";
    return false;
  }

  if (page_size_) {
    // Enforce SQLite restrictions on |page_size_|.
    DCHECK(!(page_size_ & (page_size_ - 1)))
        << " page_size_ " << page_size_ << " is not a power of two.";
    const int kSqliteMaxPageSize = 32768;  // from sqliteLimit.h
    DCHECK_LE(page_size_, kSqliteMaxPageSize);
    const std::string sql = StringPrintf("PRAGMA page_size=%d", page_size_);
    if (!null_db.Execute(sql.c_str()))
      return false;
  }

#if defined(OS_ANDROID)
  // Android compiles with SQLITE_DEFAULT_AUTOVACUUM.  Unfortunately,
  // in-memory databases do not respect this define.
  // TODO(shess): Figure out a way to set this without using platform
  // specific code.  AFAICT from sqlite3.c, the only way to do it
  // would be to create an actual filesystem database, which is
  // unfortunate.
  if (!null_db.Execute("PRAGMA auto_vacuum = 1"))
    return false;
#endif

  // The page size doesn't take effect until a database has pages, and
  // at this point the null database has none.  Changing the schema
  // version will create the first page.  This will not affect the
  // schema version in the resulting database, as SQLite's backup
  // implementation propagates the schema version from the original
  // connection to the new version of the database, incremented by one
  // so that other readers see the schema change and act accordingly.
  if (!null_db.Execute("PRAGMA schema_version = 1"))
    return false;

  // SQLite tracks the expected number of database pages in the first
  // page, and if it does not match the total retrieved from a
  // filesystem call, treats the database as corrupt.  This situation
  // breaks almost all SQLite calls.  "PRAGMA writable_schema" can be
  // used to hint to SQLite to soldier on in that case, specifically
  // for purposes of recovery.  [See SQLITE_CORRUPT_BKPT case in
  // sqlite3.c lockBtree().]
  // TODO(shess): With this, "PRAGMA auto_vacuum" and "PRAGMA
  // page_size" can be used to query such a database.
  ScopedWritableSchema writable_schema(db_);

  sqlite3_backup* backup = sqlite3_backup_init(db_, "main",
                                               null_db.db_, "main");
  if (!backup) {
    DLOG(FATAL) << "Unable to start sqlite3_backup().";
    return false;
  }

  // -1 backs up the entire database.
  int rc = sqlite3_backup_step(backup, -1);
  int pages = sqlite3_backup_pagecount(backup);
  sqlite3_backup_finish(backup);

  // The destination database was locked.
  if (rc == SQLITE_BUSY) {
    return false;
  }

  // The entire database should have been backed up.
  if (rc != SQLITE_DONE) {
    DLOG(FATAL) << "Unable to copy entire null database.";
    return false;
  }

  // Exactly one page should have been backed up.  If this breaks,
  // check this function to make sure assumptions aren't being broken.
  DCHECK_EQ(pages, 1);

  return true;
}

bool Connection::RazeWithTimout(base::TimeDelta timeout) {
  if (!db_) {
    DLOG(FATAL) << "Cannot raze null db";
    return false;
  }

  ScopedBusyTimeout busy_timeout(db_);
  busy_timeout.SetTimeout(timeout);
  return Raze();
}

bool Connection::BeginTransaction() {
  if (needs_rollback_) {
    DCHECK_GT(transaction_nesting_, 0);

    // When we're going to rollback, fail on this begin and don't actually
    // mark us as entering the nested transaction.
    return false;
  }

  bool success = true;
  if (!transaction_nesting_) {
    needs_rollback_ = false;

    Statement begin(GetCachedStatement(SQL_FROM_HERE, "BEGIN TRANSACTION"));
    if (!begin.Run())
      return false;
  }
  transaction_nesting_++;
  return success;
}

void Connection::RollbackTransaction() {
  if (!transaction_nesting_) {
    DLOG(FATAL) << "Rolling back a nonexistent transaction";
    return;
  }

  transaction_nesting_--;

  if (transaction_nesting_ > 0) {
    // Mark the outermost transaction as needing rollback.
    needs_rollback_ = true;
    return;
  }

  DoRollback();
}

bool Connection::CommitTransaction() {
  if (!transaction_nesting_) {
    DLOG(FATAL) << "Rolling back a nonexistent transaction";
    return false;
  }
  transaction_nesting_--;

  if (transaction_nesting_ > 0) {
    // Mark any nested transactions as failing after we've already got one.
    return !needs_rollback_;
  }

  if (needs_rollback_) {
    DoRollback();
    return false;
  }

  Statement commit(GetCachedStatement(SQL_FROM_HERE, "COMMIT"));
  return commit.Run();
}

int Connection::ExecuteAndReturnErrorCode(const char* sql) {
  AssertIOAllowed();
  if (!db_)
    return false;
  return sqlite3_exec(db_, sql, NULL, NULL, NULL);
}

bool Connection::Execute(const char* sql) {
  int error = ExecuteAndReturnErrorCode(sql);
  if (error != SQLITE_OK)
    error = OnSqliteError(error, NULL);

  // This needs to be a FATAL log because the error case of arriving here is
  // that there's a malformed SQL statement. This can arise in development if
  // a change alters the schema but not all queries adjust.
  if (error == SQLITE_ERROR)
    DLOG(FATAL) << "SQL Error in " << sql << ", " << GetErrorMessage();
  return error == SQLITE_OK;
}

bool Connection::ExecuteWithTimeout(const char* sql, base::TimeDelta timeout) {
  if (!db_)
    return false;

  ScopedBusyTimeout busy_timeout(db_);
  busy_timeout.SetTimeout(timeout);
  return Execute(sql);
}

bool Connection::HasCachedStatement(const StatementID& id) const {
  return statement_cache_.find(id) != statement_cache_.end();
}

scoped_refptr<Connection::StatementRef> Connection::GetCachedStatement(
    const StatementID& id,
    const char* sql) {
  CachedStatementMap::iterator i = statement_cache_.find(id);
  if (i != statement_cache_.end()) {
    // Statement is in the cache. It should still be active (we're the only
    // one invalidating cached statements, and we'll remove it from the cache
    // if we do that. Make sure we reset it before giving out the cached one in
    // case it still has some stuff bound.
    DCHECK(i->second->is_valid());
    sqlite3_reset(i->second->stmt());
    return i->second;
  }

  scoped_refptr<StatementRef> statement = GetUniqueStatement(sql);
  if (statement->is_valid())
    statement_cache_[id] = statement;  // Only cache valid statements.
  return statement;
}

scoped_refptr<Connection::StatementRef> Connection::GetUniqueStatement(
    const char* sql) {
  AssertIOAllowed();

  if (!db_)
    return new StatementRef();  // Return inactive statement.

  sqlite3_stmt* stmt = NULL;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    // This is evidence of a syntax error in the incoming SQL.
    DLOG(FATAL) << "SQL compile error " << GetErrorMessage();

    // It could also be database corruption.
    OnSqliteError(rc, NULL);
    return new StatementRef();
  }
  return new StatementRef(this, stmt);
}

scoped_refptr<Connection::StatementRef> Connection::GetUntrackedStatement(
    const char* sql) const {
  if (!db_)
    return new StatementRef();  // Return inactive statement.

  sqlite3_stmt* stmt = NULL;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    // This is evidence of a syntax error in the incoming SQL.
    DLOG(FATAL) << "SQL compile error " << GetErrorMessage();
    return new StatementRef();
  }
  return new StatementRef(stmt);
}

bool Connection::IsSQLValid(const char* sql) {
  AssertIOAllowed();
  sqlite3_stmt* stmt = NULL;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, NULL) != SQLITE_OK)
    return false;

  sqlite3_finalize(stmt);
  return true;
}

bool Connection::DoesTableExist(const char* table_name) const {
  return DoesTableOrIndexExist(table_name, "table");
}

bool Connection::DoesIndexExist(const char* index_name) const {
  return DoesTableOrIndexExist(index_name, "index");
}

bool Connection::DoesTableOrIndexExist(
    const char* name, const char* type) const {
  const char* kSql = "SELECT name FROM sqlite_master WHERE type=? AND name=?";
  Statement statement(GetUntrackedStatement(kSql));
  statement.BindString(0, type);
  statement.BindString(1, name);

  return statement.Step();  // Table exists if any row was returned.
}

bool Connection::DoesColumnExist(const char* table_name,
                                 const char* column_name) const {
  std::string sql("PRAGMA TABLE_INFO(");
  sql.append(table_name);
  sql.append(")");

  Statement statement(GetUntrackedStatement(sql.c_str()));
  while (statement.Step()) {
    if (!statement.ColumnString(1).compare(column_name))
      return true;
  }
  return false;
}

int64 Connection::GetLastInsertRowId() const {
  if (!db_) {
    DLOG(FATAL) << "Illegal use of connection without a db";
    return 0;
  }
  return sqlite3_last_insert_rowid(db_);
}

int Connection::GetLastChangeCount() const {
  if (!db_) {
    DLOG(FATAL) << "Illegal use of connection without a db";
    return 0;
  }
  return sqlite3_changes(db_);
}

int Connection::GetErrorCode() const {
  if (!db_)
    return SQLITE_ERROR;
  return sqlite3_errcode(db_);
}

int Connection::GetLastErrno() const {
  if (!db_)
    return -1;

  int err = 0;
  if (SQLITE_OK != sqlite3_file_control(db_, NULL, SQLITE_LAST_ERRNO, &err))
    return -2;

  return err;
}

const char* Connection::GetErrorMessage() const {
  if (!db_)
    return "sql::Connection has no connection.";
  return sqlite3_errmsg(db_);
}

bool Connection::OpenInternal(const std::string& file_name) {
  AssertIOAllowed();

  if (db_) {
    DLOG(FATAL) << "sql::Connection is already open.";
    return false;
  }

  int err = sqlite3_open(file_name.c_str(), &db_);
  if (err != SQLITE_OK) {
    // Histogram failures specific to initial open for debugging
    // purposes.
    UMA_HISTOGRAM_ENUMERATION("Sqlite.OpenFailure", err & 0xff, 50);

    OnSqliteError(err, NULL);
    Close();
    db_ = NULL;
    return false;
  }

  // sqlite3_open() does not actually read the database file (unless a
  // hot journal is found).  Successfully executing this pragma on an
  // existing database requires a valid header on page 1.
  // TODO(shess): For now, just probing to see what the lay of the
  // land is.  If it's mostly SQLITE_NOTADB, then the database should
  // be razed.
  err = ExecuteAndReturnErrorCode("PRAGMA auto_vacuum");
  if (err != SQLITE_OK)
    UMA_HISTOGRAM_ENUMERATION("Sqlite.OpenProbeFailure", err & 0xff, 50);

  // Enable extended result codes to provide more color on I/O errors.
  // Not having extended result codes is not a fatal problem, as
  // Chromium code does not attempt to handle I/O errors anyhow.  The
  // current implementation always returns SQLITE_OK, the DCHECK is to
  // quickly notify someone if SQLite changes.
  err = sqlite3_extended_result_codes(db_, 1);
  DCHECK_EQ(err, SQLITE_OK) << "Could not enable extended result codes";

  // If indicated, lock up the database before doing anything else, so
  // that the following code doesn't have to deal with locking.
  // TODO(shess): This code is brittle.  Find the cases where code
  // doesn't request |exclusive_locking_| and audit that it does the
  // right thing with SQLITE_BUSY, and that it doesn't make
  // assumptions about who might change things in the database.
  // http://crbug.com/56559
  if (exclusive_locking_) {
    // TODO(shess): This should probably be a full CHECK().  Code
    // which requests exclusive locking but doesn't get it is almost
    // certain to be ill-tested.
    if (!Execute("PRAGMA locking_mode=EXCLUSIVE"))
      DLOG(FATAL) << "Could not set locking mode: " << GetErrorMessage();
  }

  // http://www.sqlite.org/pragma.html#pragma_journal_mode
  // DELETE (default) - delete -journal file to commit.
  // TRUNCATE - truncate -journal file to commit.
  // PERSIST - zero out header of -journal file to commit.
  // journal_size_limit provides size to trim to in PERSIST.
  // TODO(shess): Figure out if PERSIST and journal_size_limit really
  // matter.  In theory, it keeps pages pre-allocated, so if
  // transactions usually fit, it should be faster.
  ignore_result(Execute("PRAGMA journal_mode = PERSIST"));
  ignore_result(Execute("PRAGMA journal_size_limit = 16384"));

  const base::TimeDelta kBusyTimeout =
    base::TimeDelta::FromSeconds(kBusyTimeoutSeconds);

  if (page_size_ != 0) {
    // Enforce SQLite restrictions on |page_size_|.
    DCHECK(!(page_size_ & (page_size_ - 1)))
        << " page_size_ " << page_size_ << " is not a power of two.";
    const int kSqliteMaxPageSize = 32768;  // from sqliteLimit.h
    DCHECK_LE(page_size_, kSqliteMaxPageSize);
    const std::string sql = StringPrintf("PRAGMA page_size=%d", page_size_);
    if (!ExecuteWithTimeout(sql.c_str(), kBusyTimeout))
      DLOG(FATAL) << "Could not set page size: " << GetErrorMessage();
  }

  if (cache_size_ != 0) {
    const std::string sql = StringPrintf("PRAGMA cache_size=%d", cache_size_);
    if (!ExecuteWithTimeout(sql.c_str(), kBusyTimeout))
      DLOG(FATAL) << "Could not set cache size: " << GetErrorMessage();
  }

  if (!ExecuteWithTimeout("PRAGMA secure_delete=ON", kBusyTimeout)) {
    DLOG(FATAL) << "Could not enable secure_delete: " << GetErrorMessage();
    Close();
    return false;
  }

  return true;
}

void Connection::DoRollback() {
  Statement rollback(GetCachedStatement(SQL_FROM_HERE, "ROLLBACK"));
  rollback.Run();
  needs_rollback_ = false;
}

void Connection::StatementRefCreated(StatementRef* ref) {
  DCHECK(open_statements_.find(ref) == open_statements_.end());
  open_statements_.insert(ref);
}

void Connection::StatementRefDeleted(StatementRef* ref) {
  StatementRefSet::iterator i = open_statements_.find(ref);
  if (i == open_statements_.end())
    DLOG(FATAL) << "Could not find statement";
  else
    open_statements_.erase(i);
}

void Connection::ClearCache() {
  statement_cache_.clear();

  // The cache clear will get most statements. There may be still be references
  // to some statements that are held by others (including one-shot statements).
  // This will deactivate them so they can't be used again.
  for (StatementRefSet::iterator i = open_statements_.begin();
       i != open_statements_.end(); ++i)
    (*i)->Close();
}

int Connection::OnSqliteError(int err, sql::Statement *stmt) {
  if (error_delegate_.get())
    return error_delegate_->OnError(err, this, stmt);
  // The default handling is to assert on debug and to ignore on release.
  DLOG(FATAL) << GetErrorMessage();
  return err;
}

}  // namespace sql
