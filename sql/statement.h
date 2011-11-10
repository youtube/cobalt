// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_STATEMENT_H_
#define SQL_STATEMENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "sql/connection.h"
#include "sql/sql_export.h"

namespace sql {

// Possible return values from ColumnType in a statement. These should match
// the values in sqlite3.h.
enum ColType {
  COLUMN_TYPE_INTEGER = 1,
  COLUMN_TYPE_FLOAT = 2,
  COLUMN_TYPE_TEXT = 3,
  COLUMN_TYPE_BLOB = 4,
  COLUMN_TYPE_NULL = 5,
};

// Normal usage:
//   sql::Statement s(connection_.GetUniqueStatement(...));
//   if (!s)  // You should check for errors before using the statement.
//     return false;
//
//   s.BindInt(0, a);
//   if (s.Step())
//     return s.ColumnString(0);
//
// Step() and Run() just return true to signal success. If you want to handle
// specific errors such as database corruption, install an error handler in
// in the connection object using set_error_delegate().
class SQL_EXPORT Statement {
 public:
  // Creates an uninitialized statement. The statement will be invalid until
  // you initialize it via Assign.
  Statement();

  explicit Statement(scoped_refptr<Connection::StatementRef> ref);
  ~Statement();

  // Initializes this object with the given statement, which may or may not
  // be valid. Use is_valid() to check if it's OK.
  void Assign(scoped_refptr<Connection::StatementRef> ref);

  // Returns true if the statement can be executed. All functions can still
  // be used if the statement is invalid, but they will return failure or some
  // default value. This is because the statement can become invalid in the
  // middle of executing a command if there is a serioud error and the database
  // has to be reset.
  bool is_valid() const { return ref_->is_valid(); }

  // These operators allow conveniently checking if the statement is valid
  // or not. See the pattern above for an example.
  operator bool() const { return is_valid(); }
  bool operator!() const { return !is_valid(); }

  // Running -------------------------------------------------------------------

  // Executes the statement, returning true on success. This is like Step but
  // for when there is no output, like an INSERT statement.
  bool Run();

  // Executes the statement, returning true if there is a row of data returned.
  // You can keep calling Step() until it returns false to iterate through all
  // the rows in your result set.
  //
  // When Step returns false, the result is either that there is no more data
  // or there is an error. This makes it most convenient for loop usage. If you
  // need to disambiguate these cases, use Succeeded().
  //
  // Typical example:
  //   while (s.Step()) {
  //     ...
  //   }
  //   return s.Succeeded();
  bool Step();

  // Resets the statement to its initial condition. This includes clearing all
  // the bound variables and any current result row.
  void Reset();

  // Returns true if the last executed thing in this statement succeeded. If
  // there was no last executed thing or the statement is invalid, this will
  // return false.
  bool Succeeded() const;

  // Binding -------------------------------------------------------------------

  // These all take a 0-based argument index and return true on failure. You
  // may not always care about the return value (they'll DCHECK if they fail).
  // The main thing you may want to check is when binding large blobs or
  // strings there may be out of memory.
  bool BindNull(int col);
  bool BindBool(int col, bool val);
  bool BindInt(int col, int val);
  bool BindInt64(int col, int64 val);
  bool BindDouble(int col, double val);
  bool BindCString(int col, const char* val);
  bool BindString(int col, const std::string& val);
  bool BindString16(int col, const string16& value);
  bool BindBlob(int col, const void* value, int value_len);

  // Retrieving ----------------------------------------------------------------

  // Returns the number of output columns in the result.
  int ColumnCount() const;

  // Returns the type associated with the given column.
  //
  // Watch out: the type may be undefined if you've done something to cause a
  // "type conversion." This means requesting the value of a column of a type
  // where that type is not the native type. For safety, call ColumnType only
  // on a column before getting the value out in any way.
  ColType ColumnType(int col) const;

  // These all take a 0-based argument index.
  bool ColumnBool(int col) const;
  int ColumnInt(int col) const;
  int64 ColumnInt64(int col) const;
  double ColumnDouble(int col) const;
  std::string ColumnString(int col) const;
  string16 ColumnString16(int col) const;

  // When reading a blob, you can get a raw pointer to the underlying data,
  // along with the length, or you can just ask us to copy the blob into a
  // vector. Danger! ColumnBlob may return NULL if there is no data!
  int ColumnByteLength(int col) const;
  const void* ColumnBlob(int col) const;
  bool ColumnBlobAsString(int col, std::string* blob);
  void ColumnBlobAsVector(int col, std::vector<char>* val) const;
  void ColumnBlobAsVector(int col, std::vector<unsigned char>* val) const;

  // Diagnostics --------------------------------------------------------------

  // Returns the original text of sql statement. Do not keep a pointer to it.
  const char* GetSQLStatement();

 private:
  // This is intended to check for serious errors and report them to the
  // connection object. It takes a sqlite error code, and returns the same
  // code. Currently this function just updates the succeeded flag, but will be
  // enhanced in the future to do the notification.
  int CheckError(int err);

  // The actual sqlite statement. This may be unique to us, or it may be cached
  // by the connection, which is why it's refcounted. This pointer is
  // guaranteed non-NULL.
  scoped_refptr<Connection::StatementRef> ref_;

  // See Succeeded() for what this holds.
  bool succeeded_;

  DISALLOW_COPY_AND_ASSIGN(Statement);
};

}  // namespace sql

#endif  // SQL_STATEMENT_H_
