// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/statement.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

// This empty constructor initializes our reference with an empty one so that
// we don't have to NULL-check the ref_ to see if the statement is valid: we
// only have to check the ref's validity bit.
Statement::Statement()
    : ref_(new Connection::StatementRef),
      succeeded_(false) {
}

Statement::Statement(scoped_refptr<Connection::StatementRef> ref)
    : ref_(ref),
      succeeded_(false) {
}

Statement::~Statement() {
  // Free the resources associated with this statement. We assume there's only
  // one statement active for a given sqlite3_stmt at any time, so this won't
  // mess with anything.
  Reset();
}

void Statement::Assign(scoped_refptr<Connection::StatementRef> ref) {
  Reset();
  ref_ = ref;
}

bool Statement::Run() {
  if (!is_valid())
    return false;
  return CheckError(sqlite3_step(ref_->stmt())) == SQLITE_DONE;
}

bool Statement::Step() {
  if (!is_valid())
    return false;
  return CheckError(sqlite3_step(ref_->stmt())) == SQLITE_ROW;
}

void Statement::Reset() {
  if (is_valid()) {
    // We don't call CheckError() here because sqlite3_reset() returns
    // the last error that Step() caused thereby generating a second
    // spurious error callback.
    sqlite3_clear_bindings(ref_->stmt());
    sqlite3_reset(ref_->stmt());
  }
  succeeded_ = false;
}

bool Statement::Succeeded() const {
  if (!is_valid())
    return false;
  return succeeded_;
}

bool Statement::BindNull(int col) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_null(ref_->stmt(), col + 1));
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindBool(int col, bool val) {
  return BindInt(col, val ? 1 : 0);
}

bool Statement::BindInt(int col, int val) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_int(ref_->stmt(), col + 1, val));
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindInt64(int col, int64 val) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_int64(ref_->stmt(), col + 1, val));
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindDouble(int col, double val) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_double(ref_->stmt(), col + 1, val));
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindCString(int col, const char* val) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_text(ref_->stmt(), col + 1, val, -1,
                         SQLITE_TRANSIENT));
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindString(int col, const std::string& val) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_text(ref_->stmt(), col + 1, val.data(),
                                           val.size(), SQLITE_TRANSIENT));
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindString16(int col, const string16& value) {
  return BindString(col, UTF16ToUTF8(value));
}

bool Statement::BindBlob(int col, const void* val, int val_len) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_blob(ref_->stmt(), col + 1,
                         val, val_len, SQLITE_TRANSIENT));
    return err == SQLITE_OK;
  }
  return false;
}

int Statement::ColumnCount() const {
  if (!is_valid()) {
    NOTREACHED();
    return 0;
  }
  return sqlite3_column_count(ref_->stmt());
}

ColType Statement::ColumnType(int col) const {
  // Verify that our enum matches sqlite's values.
  COMPILE_ASSERT(COLUMN_TYPE_INTEGER == SQLITE_INTEGER, integer_no_match);
  COMPILE_ASSERT(COLUMN_TYPE_FLOAT == SQLITE_FLOAT, float_no_match);
  COMPILE_ASSERT(COLUMN_TYPE_TEXT == SQLITE_TEXT, integer_no_match);
  COMPILE_ASSERT(COLUMN_TYPE_BLOB == SQLITE_BLOB, blob_no_match);
  COMPILE_ASSERT(COLUMN_TYPE_NULL == SQLITE_NULL, null_no_match);

  return static_cast<ColType>(sqlite3_column_type(ref_->stmt(), col));
}

bool Statement::ColumnBool(int col) const {
  return !!ColumnInt(col);
}

int Statement::ColumnInt(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return 0;
  }
  return sqlite3_column_int(ref_->stmt(), col);
}

int64 Statement::ColumnInt64(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return 0;
  }
  return sqlite3_column_int64(ref_->stmt(), col);
}

double Statement::ColumnDouble(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return 0;
  }
  return sqlite3_column_double(ref_->stmt(), col);
}

std::string Statement::ColumnString(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return "";
  }
  const char* str = reinterpret_cast<const char*>(
      sqlite3_column_text(ref_->stmt(), col));
  int len = sqlite3_column_bytes(ref_->stmt(), col);

  std::string result;
  if (str && len > 0)
    result.assign(str, len);
  return result;
}

string16 Statement::ColumnString16(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return string16();
  }
  std::string s = ColumnString(col);
  return !s.empty() ? UTF8ToUTF16(s) : string16();
}

int Statement::ColumnByteLength(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return 0;
  }
  return sqlite3_column_bytes(ref_->stmt(), col);
}

const void* Statement::ColumnBlob(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return NULL;
  }

  return sqlite3_column_blob(ref_->stmt(), col);
}

bool Statement::ColumnBlobAsString(int col, std::string* blob) {
  if (!is_valid()) {
    NOTREACHED();
    return false;
  }
  const void* p = ColumnBlob(col);
  size_t len = ColumnByteLength(col);
  blob->resize(len);
  if (blob->size() != len) {
    return false;
  }
  blob->assign(reinterpret_cast<const char*>(p), len);
  return true;
}

void Statement::ColumnBlobAsVector(int col, std::vector<char>* val) const {
  val->clear();
  if (!is_valid()) {
    NOTREACHED();
    return;
  }

  const void* data = sqlite3_column_blob(ref_->stmt(), col);
  int len = sqlite3_column_bytes(ref_->stmt(), col);
  if (data && len > 0) {
    val->resize(len);
    memcpy(&(*val)[0], data, len);
  }
}

void Statement::ColumnBlobAsVector(
    int col,
    std::vector<unsigned char>* val) const {
  ColumnBlobAsVector(col, reinterpret_cast< std::vector<char>* >(val));
}

const char* Statement::GetSQLStatement() {
  return sqlite3_sql(ref_->stmt());
}

int Statement::CheckError(int err) {
  // Please don't add DCHECKs here, OnSqliteError() already has them.
  succeeded_ = (err == SQLITE_OK || err == SQLITE_ROW || err == SQLITE_DONE);
  if (!succeeded_ && is_valid())
    return ref_->connection()->OnSqliteError(err, this);
  return err;
}

}  // namespace sql
