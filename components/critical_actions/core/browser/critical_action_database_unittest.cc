// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_database.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace critical_actions {

class CriticalActionDatabaseTest : public testing::Test {
 public:
  CriticalActionDatabaseTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("TestCriticalActions.db");
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
};

TEST_F(CriticalActionDatabaseTest, InitDatabase) {
  CriticalActionDatabase database(db_path_);
  EXPECT_TRUE(database.Init());
  EXPECT_TRUE(base::PathExists(db_path_));
  database.Close();
}

TEST_F(CriticalActionDatabaseTest, AddAndGetEntry) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  CriticalActionEntry entry;
  entry.critical_action_id = "test-uuid-value-1";
  entry.timestamp = base::Time::Now();
  entry.visit_id = 123;
  entry.conversation_id = "conversation-uuid-456";
  entry.actor_task_id = "task-uuid-789";
  entry.action_type = ActionType::kFormFill;
  entry.url = GURL("https://example.com/login");
  entry.metadata = "{\"key\": \"val\"}";

  EXPECT_TRUE(database.AddCriticalAction(entry));

  auto retrieved = database.GetCriticalAction("test-uuid-value-1");
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, AddDuplicateEntryFails) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  CriticalActionEntry entry;
  entry.critical_action_id = "dup-id";
  entry.timestamp = base::Time::Now();
  entry.action_type = ActionType::kDownload;

  EXPECT_TRUE(database.AddCriticalAction(entry));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CONSTRAINT_PRIMARYKEY);

  // Primary key constraint should make duplicate insertion fail (return false).
  EXPECT_FALSE(database.AddCriticalAction(entry));
  EXPECT_TRUE(expecter.SawExpectedErrors());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, GetNonExistentReturnsNullopt) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  auto retrieved = database.GetCriticalAction("does-not-exist");
  EXPECT_FALSE(retrieved.has_value());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, DeleteSingleEntry) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  CriticalActionEntry entry;
  entry.critical_action_id = "delete-me";
  entry.timestamp = base::Time::Now();
  entry.action_type = ActionType::kSettingChange;

  EXPECT_TRUE(database.AddCriticalAction(entry));
  EXPECT_TRUE(database.DeleteCriticalAction("delete-me"));

  auto retrieved = database.GetCriticalAction("delete-me");
  EXPECT_FALSE(retrieved.has_value());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, DeleteInTimeRange) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  base::Time base_time = base::Time::Now();

  // Create entries at offset offsets
  CriticalActionEntry entry1;
  entry1.critical_action_id = "range-id-1";
  entry1.timestamp = base_time - base::Hours(2);
  entry1.action_type = ActionType::kFormFill;
  ASSERT_TRUE(database.AddCriticalAction(entry1));

  CriticalActionEntry entry2;
  entry2.critical_action_id = "range-id-2";
  entry2.timestamp = base_time;
  entry2.action_type = ActionType::kFormFill;
  ASSERT_TRUE(database.AddCriticalAction(entry2));

  CriticalActionEntry entry3;
  entry3.critical_action_id = "range-id-3";
  entry3.timestamp = base_time + base::Hours(2);
  entry3.action_type = ActionType::kFormFill;
  ASSERT_TRUE(database.AddCriticalAction(entry3));

  // Delete everything around the middle entry (base_time).
  // Time range is inclusive of start, exclusive of end.
  // Start from -1 hour to +1 hour.
  EXPECT_TRUE(database.DeleteCriticalActionsInTimeRange(
      base_time - base::Hours(1), base_time + base::Hours(1)));

  // entry1 should remain (2 hours ago)
  EXPECT_TRUE(database.GetCriticalAction("range-id-1").has_value());
  // entry2 should have been deleted (exactly base_time)
  EXPECT_FALSE(database.GetCriticalAction("range-id-2").has_value());
  // entry3 should remain (2 hours from now)
  EXPECT_TRUE(database.GetCriticalAction("range-id-3").has_value());

  database.Close();
}

}  // namespace critical_actions
