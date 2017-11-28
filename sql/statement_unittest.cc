// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace {

class StatementErrorHandler : public sql::ErrorDelegate {
 public:
  StatementErrorHandler(int* error, std::string* sql_text)
    : error_(error),
      sql_text_(sql_text) {}

  virtual ~StatementErrorHandler() {}

  virtual int OnError(int error, sql::Connection* connection,
                      sql::Statement* stmt) override {
    *error_ = error;
    const char* sql_txt = stmt ? stmt->GetSQLStatement() : NULL;
    *sql_text_ = sql_txt ? sql_txt : "no statement available";
    return error;
  }

 private:
  int* error_;
  std::string* sql_text_;

 DISALLOW_COPY_AND_ASSIGN(StatementErrorHandler);
};

class SQLStatementTest : public testing::Test {
 public:
  SQLStatementTest() : error_(SQLITE_OK) {}

  void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(db_.Open(temp_dir_.path().AppendASCII("SQLStatementTest.db")));
    // The error delegate will set |error_| and |sql_text_| when any sqlite
    // statement operation returns an error code.
    db_.set_error_delegate(new StatementErrorHandler(&error_, &sql_text_));
  }

  void TearDown() {
    // If any error happened the original sql statement can be found in
    // |sql_text_|.
    EXPECT_EQ(SQLITE_OK, error_);
    db_.Close();
  }

  sql::Connection& db() { return db_; }

  int sqlite_error() const { return error_; }

  void ResetError() {
    error_ = SQLITE_OK;
    sql_text_.clear();
  }

 private:
  base::ScopedTempDir temp_dir_;
  sql::Connection db_;

  // The error code of the most recent error.
  int error_;
  // Original statement which caused the error.
  std::string sql_text_;
};

}  // namespace

TEST_F(SQLStatementTest, Assign) {
  sql::Statement s;
  EXPECT_FALSE(s.is_valid());

  s.Assign(db().GetUniqueStatement("CREATE TABLE foo (a, b)"));
  EXPECT_TRUE(s.is_valid());
}

TEST_F(SQLStatementTest, Run) {
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (3, 12)"));

  sql::Statement s(db().GetUniqueStatement("SELECT b FROM foo WHERE a=?"));
  EXPECT_FALSE(s.Succeeded());

  // Stepping it won't work since we haven't bound the value.
  EXPECT_FALSE(s.Step());

  // Run should fail since this produces output, and we should use Step(). This
  // gets a bit wonky since sqlite says this is OK so succeeded is set.
  s.Reset(true);
  s.BindInt(0, 3);
  EXPECT_FALSE(s.Run());
  EXPECT_EQ(SQLITE_ROW, db().GetErrorCode());
  EXPECT_TRUE(s.Succeeded());

  // Resetting it should put it back to the previous state (not runnable).
  s.Reset(true);
  EXPECT_FALSE(s.Succeeded());

  // Binding and stepping should produce one row.
  s.BindInt(0, 3);
  EXPECT_TRUE(s.Step());
  EXPECT_TRUE(s.Succeeded());
  EXPECT_EQ(12, s.ColumnInt(0));
  EXPECT_EQ("b", s.ColumnName(0));
  EXPECT_FALSE(s.Step());
  EXPECT_TRUE(s.Succeeded());
}

TEST_F(SQLStatementTest, BasicErrorCallback) {
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a INTEGER PRIMARY KEY, b)"));
  EXPECT_EQ(SQLITE_OK, sqlite_error());
  // Insert in the foo table the primary key. It is an error to insert
  // something other than an number. This error causes the error callback
  // handler to be called with SQLITE_MISMATCH as error code.
  sql::Statement s(db().GetUniqueStatement("INSERT INTO foo (a) VALUES (?)"));
  EXPECT_TRUE(s.is_valid());
  s.BindCString(0, "bad bad");
  EXPECT_FALSE(s.Run());
  EXPECT_EQ(SQLITE_MISMATCH, sqlite_error());
  ResetError();
}

TEST_F(SQLStatementTest, Reset) {
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (3, 12)"));
  ASSERT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (4, 13)"));

  sql::Statement s(db().GetUniqueStatement(
      "SELECT b FROM foo WHERE a = ? "));
  s.BindInt(0, 3);
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(12, s.ColumnInt(0));
  EXPECT_EQ("b", s.ColumnName(0));
  ASSERT_FALSE(s.Step());

  s.Reset(false);
  // Verify that we can get all rows again.
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(12, s.ColumnInt(0));
  EXPECT_EQ("b", s.ColumnName(0));
  EXPECT_FALSE(s.Step());

  s.Reset(true);
  ASSERT_FALSE(s.Step());
}
