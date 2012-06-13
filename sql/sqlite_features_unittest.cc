// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

// Test that certain features are/are-not enabled in our SQLite.

namespace {


class StatementErrorHandler : public sql::ErrorDelegate {
 public:
  StatementErrorHandler() : error_(SQLITE_OK) {}

  virtual int OnError(int error, sql::Connection* connection,
                      sql::Statement* stmt) OVERRIDE {
    error_ = error;
    const char* sql_txt = stmt ? stmt->GetSQLStatement() : NULL;
    sql_text_ = sql_txt ? sql_txt : "no statement available";
    return error;
  }

  int error() const { return error_; }

  void reset_error() {
    sql_text_.clear();
    error_ = SQLITE_OK;
  }

  const char* sql_statement() const { return sql_text_.c_str(); }

 protected:
  virtual ~StatementErrorHandler() {}

 private:
  int error_;
  std::string sql_text_;
};

class SQLiteFeaturesTest : public testing::Test {
 public:
  SQLiteFeaturesTest() : error_handler_(new StatementErrorHandler) {}

  void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(db_.Open(temp_dir_.path().AppendASCII("SQLStatementTest.db")));

    // The |error_handler_| will be called if any sqlite statement operation
    // returns an error code.
    db_.set_error_delegate(error_handler_);
  }

  void TearDown() {
    // If any error happened the original sql statement can be found in
    // error_handler_->sql_statement().
    EXPECT_EQ(SQLITE_OK, error_handler_->error());
    db_.Close();
  }

  sql::Connection& db() { return db_; }

  int sqlite_error() const { return error_handler_->error(); }
  void reset_error() const { error_handler_->reset_error(); }

 private:
  ScopedTempDir temp_dir_;
  sql::Connection db_;
  scoped_refptr<StatementErrorHandler> error_handler_;
};

// Do not include fts1 support, it is not useful, and nobody is
// looking at it.
TEST_F(SQLiteFeaturesTest, NoFTS1) {
  ASSERT_EQ(SQLITE_ERROR, db().ExecuteAndReturnErrorCode(
      "CREATE VIRTUAL TABLE foo USING fts1(x)"));
}

// fts2 is used for older history files, so we're signed on for
// keeping our version up-to-date.
// TODO(shess): Think up a crazy way to get out from having to support
// this forever.
TEST_F(SQLiteFeaturesTest, FTS2) {
  ASSERT_TRUE(db().Execute("CREATE VIRTUAL TABLE foo USING fts2(x)"));
}

// fts3 is used for current history files, and also for WebDatabase.
TEST_F(SQLiteFeaturesTest, FTS3) {
  ASSERT_TRUE(db().Execute("CREATE VIRTUAL TABLE foo USING fts3(x)"));
}

}  // namespace
