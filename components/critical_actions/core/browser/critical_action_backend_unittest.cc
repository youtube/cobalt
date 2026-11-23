// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_backend.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace critical_actions {

class CriticalActionBackendTest : public testing::Test {
 public:
  CriticalActionBackendTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("TestCriticalActions.db");
    backend_ = std::make_unique<CriticalActionBackend>(db_path_);
  }

  void TearDown() override { backend_.reset(); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<CriticalActionBackend> backend_;
};

TEST_F(CriticalActionBackendTest, InitBackend) {
  EXPECT_TRUE(backend_->Init());
  EXPECT_TRUE(base::PathExists(db_path_));
}

TEST_F(CriticalActionBackendTest, CallBeforeInitReturnsGracefully) {
  // Database is not initialized. Operations should return gracefully without
  // crashing.
  CriticalActionEntry entry;
  entry.critical_action_id = "test-uuid";
  entry.timestamp = base::Time::Now();
  entry.action_type = ActionType::kFormFill;

  EXPECT_FALSE(backend_->AddCriticalAction(entry));
  EXPECT_FALSE(backend_->GetCriticalAction("test-uuid").has_value());
  EXPECT_FALSE(backend_->DeleteCriticalAction("test-uuid"));
  EXPECT_FALSE(backend_->DeleteCriticalActionsInTimeRange(base::Time::Now(),
                                                          base::Time::Now()));
}

TEST_F(CriticalActionBackendTest, ForwardCallsToDatabase) {
  ASSERT_TRUE(backend_->Init());

  CriticalActionEntry entry;
  entry.critical_action_id = "test-uuid";
  entry.timestamp = base::Time::Now();
  entry.visit_id = 123;
  entry.conversation_id = "conversation-123";
  entry.actor_task_id = "task-456";
  entry.action_type = ActionType::kFormFill;
  entry.url = GURL("https://example.com");
  entry.metadata = "{}";

  // Verify basic crud operations are successfully forwarded.
  EXPECT_TRUE(backend_->AddCriticalAction(entry));

  auto retrieved = backend_->GetCriticalAction("test-uuid");
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);

  EXPECT_TRUE(backend_->DeleteCriticalAction("test-uuid"));
  EXPECT_FALSE(backend_->GetCriticalAction("test-uuid").has_value());
}

}  // namespace critical_actions
