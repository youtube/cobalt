// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/connection.h"

#include <string.h>

#include "base/file_path.h"
#include "base/logging.h"
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

}  // namespace

namespace sql {

bool StatementID::operator<(const StatementID& other) const {
  if (number_ != other.number_)
    return number_ < other.number_;
  return strcmp(str_, other.str_) < 0;
}

ErrorDelegate::ErrorDelegate() {
}

ErrorDelegate::~ErrorDelegate() {
}

Connection::StatementRef::StatementRef()
    : connection_(NULL),
      stmt_(NULL) {
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
      needs_rollback_(false) {
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
  return OpenInternal(":memory:");
}

void Connection::Close() {
  statement_cache_.clear();
  DCHECK(open_statements_.empty());
  if (db_) {
    // TODO(shess): Some additional code to debug http://crbug.com/95527 .
    // If you are reading this due to link errors or something, it can
    // be safely removed.
#if defined(HAS_SQLITE3_95527)
    unsigned int nTouched = 0;
    sqlite3_95527(db_, &nTouched);

    // If a VERY large amount of memory was touched, crash.  This
    // should never happen.
    // TODO(shess): Pull this in.  It should be page_size * page_cache
    // or something like that, 4M or 16M.  For now it's just to
    // prevent optimization.
    CHECK_LT(nTouched, 1000*1000*1000U);
#endif
    sqlite3_close(db_);
    db_ = NULL;
  }
}

void Connection::Preload() {
  if (!db_) {
    NOTREACHED();
    return;
  }

  // A statement must be open for the preload command to work. If the meta
  // table doesn't exist, it probably means this is a new database and there
  // is nothing to preload (so it's OK we do nothing).
  if (!DoesTableExist("meta"))
    return;
  Statement dummy(GetUniqueStatement("SELECT * FROM meta"));
  if (!dummy || !dummy.Step())
    return;

#if !defined(USE_SYSTEM_SQLITE)
  // This function is only defined in Chromium's version of sqlite.
  // Do not call it when using system sqlite.
  sqlite3_preload(db_);
#endif
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
    if (!begin || !begin.Run())
      return false;
  }
  transaction_nesting_++;
  return success;
}

void Connection::RollbackTransaction() {
  if (!transaction_nesting_) {
    NOTREACHED() << "Rolling back a nonexistent transaction";
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
    NOTREACHED() << "Rolling back a nonexistent transaction";
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
  if (!commit)
    return false;
  return commit.Run();
}

bool Connection::Execute(const char* sql) {
  if (!db_)
    return false;
  return sqlite3_exec(db_, sql, NULL, NULL, NULL) == SQLITE_OK;
}

bool Connection::ExecuteWithTimeout(const char* sql, base::TimeDelta timeout) {
  if (!db_)
    return false;

  ScopedBusyTimeout busy_timeout(db_);
  busy_timeout.SetTimeout(timeout);
  return sqlite3_exec(db_, sql, NULL, NULL, NULL) == SQLITE_OK;
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
  if (!db_)
    return new StatementRef(this, NULL);  // Return inactive statement.

  sqlite3_stmt* stmt = NULL;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, NULL) != SQLITE_OK) {
    // Treat this as non-fatal, it can occur in a number of valid cases, and
    // callers should be doing their own error handling.
    DLOG(WARNING) << "SQL compile error " << GetErrorMessage();
    return new StatementRef(this, NULL);
  }
  return new StatementRef(this, stmt);
}

bool Connection::DoesTableExist(const char* table_name) const {
  // GetUniqueStatement can't be const since statements may modify the
  // database, but we know ours doesn't modify it, so the cast is safe.
  Statement statement(const_cast<Connection*>(this)->GetUniqueStatement(
      "SELECT name FROM sqlite_master "
      "WHERE type='table' AND name=?"));
  if (!statement)
    return false;
  statement.BindString(0, table_name);
  return statement.Step();  // Table exists if any row was returned.
}

bool Connection::DoesColumnExist(const char* table_name,
                                 const char* column_name) const {
  std::string sql("PRAGMA TABLE_INFO(");
  sql.append(table_name);
  sql.append(")");

  // Our SQL is non-mutating, so this cast is OK.
  Statement statement(const_cast<Connection*>(this)->GetUniqueStatement(
      sql.c_str()));
  if (!statement)
    return false;

  while (statement.Step()) {
    if (!statement.ColumnString(1).compare(column_name))
      return true;
  }
  return false;
}

int64 Connection::GetLastInsertRowId() const {
  if (!db_) {
    NOTREACHED();
    return 0;
  }
  return sqlite3_last_insert_rowid(db_);
}

int Connection::GetLastChangeCount() const {
  if (!db_) {
    NOTREACHED();
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
  if (db_) {
    NOTREACHED() << "sql::Connection is already open.";
    return false;
  }

  int err = sqlite3_open(file_name.c_str(), &db_);
  if (err != SQLITE_OK) {
    OnSqliteError(err, NULL);
    db_ = NULL;
    return false;
  }

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
      NOTREACHED() << "Could not set locking mode: " << GetErrorMessage();
  }

  const base::TimeDelta kBusyTimeout =
    base::TimeDelta::FromSeconds(kBusyTimeoutSeconds);

  if (page_size_ != 0) {
    // Enforce SQLite restrictions on |page_size_|.
    DCHECK(!(page_size_ & (page_size_ - 1)))
        << " page_size_ " << page_size_ << " is not a power of two.";
    static const int kSqliteMaxPageSize = 32768;  // from sqliteLimit.h
    DCHECK_LE(page_size_, kSqliteMaxPageSize);
    const std::string sql = StringPrintf("PRAGMA page_size=%d", page_size_);
    if (!ExecuteWithTimeout(sql.c_str(), kBusyTimeout))
      NOTREACHED() << "Could not set page size: " << GetErrorMessage();
  }

  if (cache_size_ != 0) {
    const std::string sql = StringPrintf("PRAGMA cache_size=%d", cache_size_);
    if (!ExecuteWithTimeout(sql.c_str(), kBusyTimeout))
      NOTREACHED() << "Could not set cache size: " << GetErrorMessage();
  }

  if (!ExecuteWithTimeout("PRAGMA secure_delete=ON", kBusyTimeout)) {
    NOTREACHED() << "Could not enable secure_delete: " << GetErrorMessage();
    Close();
    return false;
  }

  return true;
}

void Connection::DoRollback() {
  Statement rollback(GetCachedStatement(SQL_FROM_HERE, "ROLLBACK"));
  if (rollback)
    rollback.Run();
}

void Connection::StatementRefCreated(StatementRef* ref) {
  DCHECK(open_statements_.find(ref) == open_statements_.end());
  open_statements_.insert(ref);
}

void Connection::StatementRefDeleted(StatementRef* ref) {
  StatementRefSet::iterator i = open_statements_.find(ref);
  if (i == open_statements_.end())
    NOTREACHED();
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
  NOTREACHED() << GetErrorMessage();
  return err;
}

}  // namespace sql
