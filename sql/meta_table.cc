// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/meta_table.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "sql/connection.h"
#include "sql/statement.h"

namespace sql {

// Key used in our meta table for version numbers.
static const char kVersionKey[] = "version";
static const char kCompatibleVersionKey[] = "last_compatible_version";

MetaTable::MetaTable() : db_(NULL) {
}

MetaTable::~MetaTable() {
}

// static
bool MetaTable::DoesTableExist(sql::Connection* db) {
  DCHECK(db);
  return db->DoesTableExist("meta");
}

bool MetaTable::Init(Connection* db, int version, int compatible_version) {
  DCHECK(!db_ && db);
  db_ = db;
  if (!DoesTableExist(db)) {
    if (!db_->Execute("CREATE TABLE meta"
        "(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
         "value LONGVARCHAR)"))
      return false;

    // Note: there is no index over the meta table. We currently only have a
    // couple of keys, so it doesn't matter. If we start storing more stuff in
    // there, we should create an index.
    SetVersionNumber(version);
    SetCompatibleVersionNumber(compatible_version);
  }
  return true;
}

void MetaTable::Reset() {
  db_ = NULL;
}

bool MetaTable::SetValue(const char* key, const std::string& value) {
  Statement s;
  if (!PrepareSetStatement(&s, key))
    return false;
  s.BindString(1, value);
  return s.Run();
}

bool MetaTable::GetValue(const char* key, std::string* value) {
  Statement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  *value = s.ColumnString(0);
  return true;
}

bool MetaTable::SetValue(const char* key, int value) {
  Statement s;
  if (!PrepareSetStatement(&s, key))
    return false;

  s.BindInt(1, value);
  return s.Run();
}

bool MetaTable::GetValue(const char* key, int* value) {
  Statement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  *value = s.ColumnInt(0);
  return true;
}

bool MetaTable::SetValue(const char* key, int64 value) {
  Statement s;
  if (!PrepareSetStatement(&s, key))
    return false;
  s.BindInt64(1, value);
  return s.Run();
}

bool MetaTable::GetValue(const char* key, int64* value) {
  Statement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  *value = s.ColumnInt64(0);
  return true;
}

void MetaTable::SetVersionNumber(int version) {
  SetValue(kVersionKey, version);
}

int MetaTable::GetVersionNumber() {
  int version = 0;
  if (!GetValue(kVersionKey, &version))
    return 0;
  return version;
}

void MetaTable::SetCompatibleVersionNumber(int version) {
  SetValue(kCompatibleVersionKey, version);
}

int MetaTable::GetCompatibleVersionNumber() {
  int version = 0;
  if (!GetValue(kCompatibleVersionKey, &version))
    return 0;
  return version;
}

bool MetaTable::PrepareSetStatement(Statement* statement, const char* key) {
  DCHECK(db_ && statement);
  statement->Assign(db_->GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR REPLACE INTO meta (key,value) VALUES (?,?)"));
  if (!statement->is_valid()) {
    NOTREACHED() << db_->GetErrorMessage();
    return false;
  }
  statement->BindCString(0, key);
  return true;
}

bool MetaTable::PrepareGetStatement(Statement* statement, const char* key) {
  DCHECK(db_ && statement);
  statement->Assign(db_->GetCachedStatement(SQL_FROM_HERE,
      "SELECT value FROM meta WHERE key=?"));
  if (!statement->is_valid()) {
    NOTREACHED() << db_->GetErrorMessage();
    return false;
  }
  statement->BindCString(0, key);
  if (!statement->Step())
    return false;
  return true;
}

}  // namespace sql
