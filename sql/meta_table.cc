// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/meta_table.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

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

  // If values stored are null or missing entirely, 0 will be reported.
  // Require new clients to start with a greater initial version.
  DCHECK_GT(version, 0);
  DCHECK_GT(compatible_version, 0);

  // Make sure the table is created an populated atomically.
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (!DoesTableExist(db)) {
    if (!db_->Execute("CREATE TABLE meta"
        "(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR)"))
      return false;

    // Note: there is no index over the meta table. We currently only have a
    // couple of keys, so it doesn't matter. If we start storing more stuff in
    // there, we should create an index.
    SetVersionNumber(version);
    SetCompatibleVersionNumber(compatible_version);
  }
  return transaction.Commit();
}

void MetaTable::Reset() {
  db_ = NULL;
}

void MetaTable::SetVersionNumber(int version) {
  DCHECK_GT(version, 0);
  SetValue(kVersionKey, version);
}

int MetaTable::GetVersionNumber() {
  int version = 0;
  return GetValue(kVersionKey, &version) ? version : 0;
}

void MetaTable::SetCompatibleVersionNumber(int version) {
  DCHECK_GT(version, 0);
  SetValue(kCompatibleVersionKey, version);
}

int MetaTable::GetCompatibleVersionNumber() {
  int version = 0;
  return GetValue(kCompatibleVersionKey, &version) ? version : 0;
}

bool MetaTable::SetValue(const char* key, const std::string& value) {
  Statement s;
  PrepareSetStatement(&s, key);
  s.BindString(1, value);
  return s.Run();
}

bool MetaTable::SetValue(const char* key, int value) {
  Statement s;
  PrepareSetStatement(&s, key);
  s.BindInt(1, value);
  return s.Run();
}

bool MetaTable::SetValue(const char* key, int64 value) {
  Statement s;
  PrepareSetStatement(&s, key);
  s.BindInt64(1, value);
  return s.Run();
}

bool MetaTable::GetValue(const char* key, std::string* value) {
  Statement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  *value = s.ColumnString(0);
  return true;
}

bool MetaTable::GetValue(const char* key, int* value) {
  Statement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  *value = s.ColumnInt(0);
  return true;
}

bool MetaTable::GetValue(const char* key, int64* value) {
  Statement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  *value = s.ColumnInt64(0);
  return true;
}

bool MetaTable::DeleteKey(const char* key) {
  DCHECK(db_);
  Statement s(db_->GetUniqueStatement("DELETE FROM meta WHERE key=?"));
  s.BindCString(0, key);
  return s.Run();
}

void MetaTable::PrepareSetStatement(Statement* statement, const char* key) {
  DCHECK(db_ && statement);
  statement->Assign(db_->GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR REPLACE INTO meta (key,value) VALUES (?,?)"));
  statement->BindCString(0, key);
}

bool MetaTable::PrepareGetStatement(Statement* statement, const char* key) {
  DCHECK(db_ && statement);
  statement->Assign(db_->GetCachedStatement(SQL_FROM_HERE,
      "SELECT value FROM meta WHERE key=?"));
  statement->BindCString(0, key);
  return statement->Step();
}

}  // namespace sql
