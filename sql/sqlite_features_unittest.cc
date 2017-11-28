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

// Test that certain features are/are-not enabled in our SQLite.

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

class SQLiteFeaturesTest : public testing::Test {
 public:
  SQLiteFeaturesTest() : error_(SQLITE_OK) {}

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

  int sqlite_error() const {
    return error_;
  }

 private:
  base::ScopedTempDir temp_dir_;
  sql::Connection db_;

  // The error code of the most recent error.
  int error_;
  // Original statement which has caused the error.
  std::string sql_text_;
};

// Do not include fts1 support, it is not useful, and nobody is
// looking at it.
TEST_F(SQLiteFeaturesTest, NoFTS1) {
  ASSERT_EQ(SQLITE_ERROR, db().ExecuteAndReturnErrorCode(
      "CREATE VIRTUAL TABLE foo USING fts1(x)"));
}

#if !defined(COBALT)
#if !defined(OS_IOS)
// fts2 is used for older history files, so we're signed on for keeping our
// version up-to-date.  iOS does not include fts2, so this test does not run on
// iOS.
// TODO(shess): Think up a crazy way to get out from having to support
// this forever.
TEST_F(SQLiteFeaturesTest, FTS2) {
  ASSERT_TRUE(db().Execute("CREATE VIRTUAL TABLE foo USING fts2(x)"));
}
#endif

// fts3 is used for current history files, and also for WebDatabase.
TEST_F(SQLiteFeaturesTest, FTS3) {
  ASSERT_TRUE(db().Execute("CREATE VIRTUAL TABLE foo USING fts3(x)"));
}
#endif  // !defined(COBALT)

}  // namespace
