// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_META_TABLE_H_
#define SQL_META_TABLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "sql/sql_export.h"

namespace sql {

class Connection;
class Statement;

class SQL_EXPORT MetaTable {
 public:
  MetaTable();
  ~MetaTable();

  // Returns true if the 'meta' table exists.
  static bool DoesTableExist(Connection* db);

  // Initializes the MetaTableHelper, creating the meta table if necessary. For
  // new tables, it will initialize the version number to |version| and the
  // compatible version number to |compatible_version|.
  bool Init(Connection* db, int version, int compatible_version);

  // Resets this MetaTable object, making another call to Init() possible.
  void Reset();

  // The version number of the database. This should be the version number of
  // the creator of the file. The version number will be 0 if there is no
  // previously set version number.
  //
  // See also Get/SetCompatibleVersionNumber().
  void SetVersionNumber(int version);
  int GetVersionNumber();

  // The compatible version number is the lowest version of the code that this
  // database can be read by. If there are minor changes or additions, old
  // versions of the code can still work with the database without failing.
  //
  // For example, if an optional column is added to a table in version 3, the
  // new code will set the version to 3, and the compatible version to 2, since
  // the code expecting version 2 databases can still read and write the table.
  //
  // Rule of thumb: check the version number when you're upgrading, but check
  // the compatible version number to see if you can read the file at all. If
  // it's larger than you code is expecting, fail.
  //
  // The compatible version number will be 0 if there is no previously set
  // compatible version number.
  void SetCompatibleVersionNumber(int version);
  int GetCompatibleVersionNumber();

  // Set the given arbitrary key with the given data. Returns true on success.
  bool SetValue(const char* key, const std::string& value);
  bool SetValue(const char* key, int value);
  bool SetValue(const char* key, int64 value);

  // Retrieves the value associated with the given key. This will use sqlite's
  // type conversion rules. It will return true on success.
  bool GetValue(const char* key, std::string* value);
  bool GetValue(const char* key, int* value);
  bool GetValue(const char* key, int64* value);

 private:
  // Conveniences to prepare the two types of statements used by
  // MetaTableHelper.
  bool PrepareSetStatement(Statement* statement, const char* key);
  bool PrepareGetStatement(Statement* statement, const char* key);

  Connection* db_;

  DISALLOW_COPY_AND_ASSIGN(MetaTable);
};

}  // namespace sql

#endif  // SQL_META_TABLE_H_
