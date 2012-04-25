// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

class SQLConnectionTest : public testing::Test {
 public:
  SQLConnectionTest() {}

  void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(db_.Open(db_path()));
  }

  void TearDown() {
    db_.Close();
  }

  sql::Connection& db() { return db_; }

  FilePath db_path() {
    return temp_dir_.path().AppendASCII("SQLConnectionTest.db");
  }

 private:
  ScopedTempDir temp_dir_;
  sql::Connection db_;
};

TEST_F(SQLConnectionTest, Execute) {
  // Valid statement should return true.
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  EXPECT_EQ(SQLITE_OK, db().GetErrorCode());

  // Invalid statement should fail.
  ASSERT_EQ(SQLITE_ERROR,
            db().ExecuteAndReturnErrorCode("CREATE TAB foo (a, b"));
  EXPECT_EQ(SQLITE_ERROR, db().GetErrorCode());
}

TEST_F(SQLConnectionTest, ExecuteWithErrorCode) {
  ASSERT_EQ(SQLITE_OK,
            db().ExecuteAndReturnErrorCode("CREATE TABLE foo (a, b)"));
  ASSERT_EQ(SQLITE_ERROR,
            db().ExecuteAndReturnErrorCode("CREATE TABLE TABLE"));
  ASSERT_EQ(SQLITE_ERROR,
            db().ExecuteAndReturnErrorCode(
                "INSERT INTO foo(a, b) VALUES (1, 2, 3, 4)"));
}

TEST_F(SQLConnectionTest, CachedStatement) {
  sql::StatementID id1("foo", 12);

  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db().Execute("INSERT INTO foo(a, b) VALUES (12, 13)"));

  // Create a new cached statement.
  {
    sql::Statement s(db().GetCachedStatement(id1, "SELECT a FROM foo"));
    ASSERT_TRUE(s.is_valid());

    ASSERT_TRUE(s.Step());
    EXPECT_EQ(12, s.ColumnInt(0));
  }

  // The statement should be cached still.
  EXPECT_TRUE(db().HasCachedStatement(id1));

  {
    // Get the same statement using different SQL. This should ignore our
    // SQL and use the cached one (so it will be valid).
    sql::Statement s(db().GetCachedStatement(id1, "something invalid("));
    ASSERT_TRUE(s.is_valid());

    ASSERT_TRUE(s.Step());
    EXPECT_EQ(12, s.ColumnInt(0));
  }

  // Make sure other statements aren't marked as cached.
  EXPECT_FALSE(db().HasCachedStatement(SQL_FROM_HERE));
}

TEST_F(SQLConnectionTest, IsSQLValidTest) {
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db().IsSQLValid("SELECT a FROM foo"));
  ASSERT_FALSE(db().IsSQLValid("SELECT no_exist FROM foo"));
}

TEST_F(SQLConnectionTest, DoesStuffExist) {
  // Test DoesTableExist.
  EXPECT_FALSE(db().DoesTableExist("foo"));
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  EXPECT_TRUE(db().DoesTableExist("foo"));

  // Should be case sensitive.
  EXPECT_FALSE(db().DoesTableExist("FOO"));

  // Test DoesColumnExist.
  EXPECT_FALSE(db().DoesColumnExist("foo", "bar"));
  EXPECT_TRUE(db().DoesColumnExist("foo", "a"));

  // Testing for a column on a nonexistent table.
  EXPECT_FALSE(db().DoesColumnExist("bar", "b"));
}

TEST_F(SQLConnectionTest, GetLastInsertRowId) {
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (id INTEGER PRIMARY KEY, value)"));

  ASSERT_TRUE(db().Execute("INSERT INTO foo (value) VALUES (12)"));

  // Last insert row ID should be valid.
  int64 row = db().GetLastInsertRowId();
  EXPECT_LT(0, row);

  // It should be the primary key of the row we just inserted.
  sql::Statement s(db().GetUniqueStatement("SELECT value FROM foo WHERE id=?"));
  s.BindInt64(0, row);
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(12, s.ColumnInt(0));
}

TEST_F(SQLConnectionTest, Rollback) {
  ASSERT_TRUE(db().BeginTransaction());
  ASSERT_TRUE(db().BeginTransaction());
  EXPECT_EQ(2, db().transaction_nesting());
  db().RollbackTransaction();
  EXPECT_FALSE(db().CommitTransaction());
  EXPECT_TRUE(db().BeginTransaction());
}

// Test that sql::Connection::Raze() results in a database without the
// tables from the original database.
TEST_F(SQLConnectionTest, Raze) {
  const char* kCreateSql = "CREATE TABLE foo (id INTEGER PRIMARY KEY, value)";
  ASSERT_TRUE(db().Execute(kCreateSql));
  ASSERT_TRUE(db().Execute("INSERT INTO foo (value) VALUES (12)"));

  {
    sql::Statement s(db().GetUniqueStatement("PRAGMA page_count"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(2, s.ColumnInt(0));
  }

  {
    sql::Statement s(db().GetUniqueStatement("SELECT * FROM sqlite_master"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ("table", s.ColumnString(0));
    EXPECT_EQ("foo", s.ColumnString(1));
    EXPECT_EQ("foo", s.ColumnString(2));
    EXPECT_EQ(2, s.ColumnInt(3));
    EXPECT_EQ(kCreateSql, s.ColumnString(4));
  }

  ASSERT_TRUE(db().Raze());

  {
    sql::Statement s(db().GetUniqueStatement("PRAGMA page_count"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(1, s.ColumnInt(0));
  }

  {
    sql::Statement s(db().GetUniqueStatement("SELECT * FROM sqlite_master"));
    ASSERT_FALSE(s.Step());
  }
}

// Test that Raze() maintains page_size.
TEST_F(SQLConnectionTest, RazePageSize) {
  const int kPageSize = 4096;

  // Make sure that the default size isn't already |kPageSize|.
  // Scoped to release statement before Close().
  {
    sql::Statement s(db().GetUniqueStatement("PRAGMA page_size"));
    ASSERT_TRUE(s.Step());
    ASSERT_NE(kPageSize, s.ColumnInt(0));
  }

  // Re-open the database to allow setting the page size.
  db().Close();
  db().set_page_size(kPageSize);
  ASSERT_TRUE(db().Open(db_path()));

  // page_size should match the indicated value.
  sql::Statement s(db().GetUniqueStatement("PRAGMA page_size"));
  ASSERT_TRUE(s.Step());
  ASSERT_EQ(kPageSize, s.ColumnInt(0));

  // After raze, page_size should still match the indicated value.
  ASSERT_TRUE(db().Raze());
  s.Reset(true);
  ASSERT_TRUE(s.Step());
  ASSERT_EQ(kPageSize, s.ColumnInt(0));
}

// Test that Raze() results are seen in other connections.
TEST_F(SQLConnectionTest, RazeMultiple) {
  const char* kCreateSql = "CREATE TABLE foo (id INTEGER PRIMARY KEY, value)";
  ASSERT_TRUE(db().Execute(kCreateSql));

  sql::Connection other_db;
  ASSERT_TRUE(other_db.Open(db_path()));

  // Check that the second connection sees the table.
  const char *kTablesQuery = "SELECT COUNT(*) FROM sqlite_master";
  sql::Statement s(other_db.GetUniqueStatement(kTablesQuery));
  ASSERT_TRUE(s.Step());
  ASSERT_EQ(1, s.ColumnInt(0));
  ASSERT_FALSE(s.Step());  // Releases the shared lock.

  ASSERT_TRUE(db().Raze());

  // The second connection sees the updated database.
  s.Reset(true);
  ASSERT_TRUE(s.Step());
  ASSERT_EQ(0, s.ColumnInt(0));
}

TEST_F(SQLConnectionTest, RazeLocked) {
  const char* kCreateSql = "CREATE TABLE foo (id INTEGER PRIMARY KEY, value)";
  ASSERT_TRUE(db().Execute(kCreateSql));

  // Open a transaction and write some data in a second connection.
  // This will acquire a PENDING or EXCLUSIVE transaction, which will
  // cause the raze to fail.
  sql::Connection other_db;
  ASSERT_TRUE(other_db.Open(db_path()));
  ASSERT_TRUE(other_db.BeginTransaction());
  const char* kInsertSql = "INSERT INTO foo VALUES (1, 'data')";
  ASSERT_TRUE(other_db.Execute(kInsertSql));

  ASSERT_FALSE(db().Raze());

  // Works after COMMIT.
  ASSERT_TRUE(other_db.CommitTransaction());
  ASSERT_TRUE(db().Raze());

  // Re-create the database.
  ASSERT_TRUE(db().Execute(kCreateSql));
  ASSERT_TRUE(db().Execute(kInsertSql));

  // An unfinished read transaction in the other connection also
  // blocks raze.
  const char *kQuery = "SELECT COUNT(*) FROM foo";
  sql::Statement s(other_db.GetUniqueStatement(kQuery));
  ASSERT_TRUE(s.Step());
  ASSERT_FALSE(db().Raze());

  // Complete the statement unlocks the database.
  ASSERT_FALSE(s.Step());
  ASSERT_TRUE(db().Raze());
}

// TODO(shess): Spin up a background thread to hold other_db, to more
// closely match real life.  That would also allow testing
// RazeWithTimeout().
